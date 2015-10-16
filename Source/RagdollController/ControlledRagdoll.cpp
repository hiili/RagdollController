// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "ControlledRagdoll.h"

#include "RCLevelScriptActor.h"
#include "XmlFSocket.h"
#include "ScopeGuard.h"
#include "Utility.h"
#include "Mbml.h"

#include <pugixml.hpp>

#include <Engine/GameInstance.h>
#include <Net/UnrealNetwork.h>
#include <GenericPlatform/GenericPlatformProperties.h>

#include <extensions/PxD6Joint.h>
#include <PxRigidBody.h>
#include <PxRigidDynamic.h>
#include <PxTransform.h>

#include <cstring>
#include <cmath>




AControlledRagdoll::AControlledRagdoll()
{
}




void AControlledRagdoll::PostInitializeComponents()
{
	Super::PostInitializeComponents();


	/* init the SkeletalMeshComponent pointer: scan all components and choose the first USkeletalMeshComponent */

	// get all components of the right type
	TArray<USkeletalMeshComponent*> comps;
	GetComponents( comps );

	// if at least one found, choose the first one
	if( comps.Num() >= 1 )
	{
		this->SkeletalMeshComponent = comps[0];

		// warn about multiple SkeletalMeshComponents
		if( comps.Num() > 1 )
		{
			UE_LOG( LogRcCr, Warning, TEXT( "(%s) Multiple USkeletalMeshComponents found! Using the first one." ), TEXT( __FUNCTION__ ) );
		}
	}
	else
	{
		UE_LOG( LogRcCr, Error, TEXT( "(%s) No USkeletalMeshComponent found!" ), TEXT( __FUNCTION__ ) );
		return;
	}


	if( HasAuthority() )
	{

		/* We are standalone or a server */
		
		// make sure that physics simulation is enabled also on a dedicated server
		this->SkeletalMeshComponent->bEnablePhysicsOnDedicatedServer = true;
		this->SkeletalMeshComponent->SetSimulatePhysics( true ); // this must be called after bEnablePhysicsOnDedicatedServer = true even if physics are enabled also via editor!

	}
	else
	{

		/* We are a network client, assume spectator role (although UE spectator mode is not used explicitly at the moment) */

	}


	// Initialize the internal state structs
	InitState();
}




void AControlledRagdoll::BeginPlay()
{
	Super::BeginPlay();

	// init the RCLevelScriptActor and GameMode pointers
	check( GetWorld() );
	this->RCLevelScriptActor = dynamic_cast<ARCLevelScriptActor *>(GetWorld()->GetLevelScriptActor());
	this->GameMode = GetWorld()->GetAuthGameMode();
	
	// Check that we got a RCLevelScriptActor. It is unavailable if we are in an editor session, but BeginPlay() should not have been called in that case.
	check( this->RCLevelScriptActor );


	// Register for automatic NetUpdateFrequency management
	this->RCLevelScriptActor->RegisterManagedNetUpdateFrequency( this );

	// If running on authority, then make sure that we tick after GameMode (both the GameMode and regular actors tick in the same group, TG_PrePhysics)
	if( HasAuthority() )
	{
		check( this->GameMode );
		AddTickPrerequisiteActor( this->GameMode );
	}
}




void AControlledRagdoll::Tick( float deltaSeconds )
{
	// sanity check
	if( !this->SkeletalMeshComponent )
	{
		UE_LOG( LogRcCr, Error, TEXT( "(%s) Internal error: invalid state! Skipping this tick." ), TEXT(__FUNCTION__) );
	}

	// If network client, then we are just visualizing the ragdoll that is being simulated on the server
	if( !HasAuthority() )
	{
		// Call the tick hook (available for inherited C++ classes), then Super::Tick(), which runs this actor's Blueprint
		TickHook( deltaSeconds );
		Super::Tick( deltaSeconds );

		return;
	}


	/* We are standalone or a server */
	

	/* Relay data between the game engine and the remote controller, while calling additional tick functionality in between. */

	// Read inbound data
	PrepareRemoteControllerCommunication();
	ReadFromRemoteController();
	ReadFromSimulation();

	// Call the tick hook (available for inherited C++ classes), then Super::Tick(), which runs this actor's Blueprint
	TickHook( deltaSeconds );
	Super::Tick( deltaSeconds );
	ValidateBlueprintWritables();

	// Write outbound data
	WriteToSimulation();
	WriteToRemoteController();
	FinalizeRemoteControllerCommunication();








	/* temporary test code */

	++tickCounter;

	int32 jointInd = this->JointNames.Find( FName( "upperarm_l" ) );

    // multiplication order verified (using joint upperarm_l, bone 2)
	FQuat rf1global = this->JointStates[jointInd].BoneGlobalRotations[0] * this->JointStates[jointInd].RefFrameRotations[0];
	FQuat rf2global = this->JointStates[jointInd].BoneGlobalRotations[1] * this->JointStates[jointInd].RefFrameRotations[1];

	//this->JointStates[jointInd].Bodies[0]->AddTorque( 200000.f * sin( float( tickCounter ) / 20.f ) * rf1global.RotateVector( FVector( 0.f, 0.f, 1.f ) ) );
	//this->JointStates[jointInd].Bodies[1]->AddTorque( -200000.f * sin( float( tickCounter ) / 20.f ) * rf1global.RotateVector( FVector( 0.f, 0.f, 1.f ) ) );
	this->JointStates[jointInd].Bodies[0]->AddForce( FVector( 0.f, 0.f, 10.f ) );
}




void AControlledRagdoll::InitState()
{
	// init the error cleanup scope guard
	auto sgError = MakeScopeGuard( [this](){
		UE_LOG( LogRcCr, Error, TEXT( "(%s) Internal error! (scope guard 'sgError' was triggered)" ), TEXT( __FUNCTION__ ) );

		// erase all data if any errors
		this->JointStates.Empty();
	} );


	// check that we have everything we need, otherwise bail out
	if( !this->SkeletalMeshComponent ) return;

	// check the number of joints and preallocate the top level array
	int32 numJoints = this->SkeletalMeshComponent->Constraints.Num();
	this->JointNames.SetNum( numJoints );
	this->JointStates.SetNum( numJoints );

	// fill the array with joint data
	for( int joint = 0; joint < numJoints; ++joint )
	{
		// fetch the FConstraintInstance of this joint
		FConstraintInstance * constraint = this->SkeletalMeshComponent->Constraints[joint];

		// if null then bail out
		if( !constraint ) return;

		// store data
		this->JointStates[joint].Constraint = constraint;
		this->JointNames[joint] = constraint->JointName;
		this->JointStates[joint].Bodies[0] = this->SkeletalMeshComponent->GetBodyInstance( constraint->ConstraintBone1 );
		this->JointStates[joint].Bodies[1] = this->SkeletalMeshComponent->GetBodyInstance( constraint->ConstraintBone2 );
		this->JointStates[joint].BoneInds[0] = this->SkeletalMeshComponent->GetBoneIndex( constraint->ConstraintBone1 );
		this->JointStates[joint].BoneInds[1] = this->SkeletalMeshComponent->GetBoneIndex( constraint->ConstraintBone2 );
		this->JointStates[joint].RefFrameRotations[0] = constraint->GetRefFrame( EConstraintFrame::Frame1 ).GetRotation();
		this->JointStates[joint].RefFrameRotations[1] = constraint->GetRefFrame( EConstraintFrame::Frame2 ).GetRotation();

		// clear the angle readouts and the motor command
		this->JointStates[joint].JointAngles = FVector::ZeroVector;
		this->JointStates[joint].MotorCommand = FVector::ZeroVector;

		// bail out if something was not available
		if( !this->JointStates[joint].Bodies[0] || !this->JointStates[joint].Bodies[1]
			|| this->JointStates[joint].BoneInds[0] == INDEX_NONE || this->JointStates[joint].BoneInds[1] == INDEX_NONE ) return;
	}

	// all good, release the error cleanup scope guard and return
	sgError.release();
	return;
}




void AControlledRagdoll::ValidateBlueprintWritables()
{
	// check if non-Blueprint JointState fields have been accidentally zeroed in Blueprint
	for( auto & jointState : this->JointStates )
	{
		// check only the first field..
		if( !jointState.Constraint ) {
			UE_LOG( LogRcCr, Error,
				TEXT( "(%s) A JointState struct seems to become partially zeroed! Breaking and re-making a JointState in Blueprints is the likely cause. Use a 'Set members in JointState' node instead!" ),
				TEXT( __FUNCTION__ ) );
		}
	}
}




void AControlledRagdoll::ReadFromSimulation()
{
	// init the error cleanup scope guard
	auto sgError = MakeScopeGuard( [this](){
		UE_LOG( LogRcCr, Error, TEXT( "(%s) Internal error! (scope guard 'sgError' was triggered)" ), TEXT( __FUNCTION__ ) );

		// erase all data (including static joint data!) if any errors
		this->JointStates.Empty();
	} );


	// check that the array size matches the skeleton's joint count
	if( this->JointStates.Num() != this->SkeletalMeshComponent->Constraints.Num() ) return;

	// loop through joints
	for( auto & jointState : this->JointStates )
	{
		// check availability of PhysX data
		if( !jointState.Constraint || !jointState.Constraint->ConstraintData ) return;

		// store the global rotations of the connected bones
		for( int i = 0; i < 2; ++i )
		{
			jointState.BoneGlobalRotations[i] = this->SkeletalMeshComponent->GetBoneTransform( jointState.BoneInds[i] ).GetRotation();
		}

		// store the joint rotation angles (we could use the UE wrappers, however they reverse Y and Z for some reason. do not get involved with that:
		// they might be reverted back in a future version, and if/when that goes unnoticed by us then we will have a bug here.)
		jointState.JointAngles = FVector(
			/* jointState.Constraint->ConstraintData->getTwist() */ 0.f,   // see below
			jointState.Constraint->ConstraintData->getSwingYAngle(),
			jointState.Constraint->ConstraintData->getSwingZAngle() );

		// physx::PxD6Joint::getTwist() ignores the sign of q.x, overwrite with our own version. Just correcting the value returned by physx::getTwist() would
		// lead to trouble if Nvidia ever decides to fix their version of getTwist().
		// (the following few lines are mostly copied from PhysX)
		physx::PxQuat q = jointState.Constraint->ConstraintData->getRelativeTransform().q;
		physx::PxQuat twist = q.x != 0.0f ? physx::PxQuat( q.x, 0, 0, q.w ).getNormalized() : physx::PxQuat( physx::PxIdentity );
		physx::PxReal angle = twist.getAngle();
		angle = angle <= physx::PxPi ? angle : angle - 2 * physx::PxPi;
		angle = twist.x >= 0.f ? angle : -angle;   // the correction
		jointState.JointAngles[0] = angle;
	}

	// all good, release the error cleanup scope guard and return
	sgError.release();
	return;
}




void AControlledRagdoll::WriteToSimulation()
{
	// init the error cleanup scope guard
	auto sgError = MakeScopeGuard( [this](){
		UE_LOG( LogRcCr, Error, TEXT( "(%s) Internal error! (scope guard 'sgError' was triggered)" ), TEXT( __FUNCTION__ ) );

		// erase all data (including static joint data!) if any errors
		this->JointStates.Empty();
	} );


	// check that the array size matches the skeleton's joint count
	if( this->JointStates.Num() != this->SkeletalMeshComponent->Constraints.Num() ) return;

	// loop through joints
	for( auto & jointState : this->JointStates )
	{
		// check availability of PhysX data
		if( !jointState.Constraint || !jointState.Constraint->ConstraintData ) return;

		// transform the joint's reference frame for the child bone (bone 0) to global coordinates
		FQuat referenceFrame0Global = jointState.BoneGlobalRotations[0] * jointState.RefFrameRotations[0];

		// transform the motor command vector to a global-coordinate torque vector
		FVector torque0Global = referenceFrame0Global.RotateVector( jointState.MotorCommand );

		// apply the torque to both bodies
		jointState.Bodies[0]->AddTorque( torque0Global );
		jointState.Bodies[1]->AddTorque( -torque0Global );
	}

	// all good, release the error cleanup scope guard and return
	sgError.release();
	return;
}




void AControlledRagdoll::HandleNetworkError( const std::string & description )
{
	// drop the connection
	RemoteControlSocket.reset();

	// set InXmlStatus.status to pugi::status_no_document_element
	RemoteControlSocket->InXmlStatus.status = pugi::status_no_document_element;

	// log
	UE_LOG( LogRcCr, Error, TEXT( "(%s, %s) Remote controller connection failed: %s! Dropping the connection." ),
		TEXT( __FUNCTION__ ), *GetHumanReadableName(), *FString( description.c_str() ) );
}




void AControlledRagdoll::PrepareRemoteControllerCommunication()
{
	// no-op if no remote controller
	if( !this->RemoteControlSocket ) return;

	// check that the connection is good
	if( !RemoteControlSocket->IsGood() )
	{
		// connection failed
		HandleNetworkError( "network level failure" );
		return;
	}

	// read data from socket
	RemoteControlSocket->SetBlocking( true );   // synchronous mode so block with no timeout; we really want that data on each tick
	if( !RemoteControlSocket->GetXml() )
	{
		// read failed
		HandleNetworkError( "failed to read xml data from the socket (" + std::string( RemoteControlSocket->InXmlStatus.description() ) + ")" );
		return;
	}

	check( RemoteControlSocket->InXmlStatus.status == pugi::status_ok );
}




void AControlledRagdoll::FinalizeRemoteControllerCommunication()
{
	// no-op if no remote controller or no inbound data is available (latter test is redundant, because PrepareRemoteControllerCommunication() drops the
	// connection in such cases)
	if( !RemoteControlSocket || RemoteControlSocket->InXmlStatus.status != pugi::status_ok ) return;

	// send OutXml
	if( !RemoteControlSocket->PutXml() )
	{
		// send failed
		HandleNetworkError( "failed to send the xml response document" );
	}
}




void AControlledRagdoll::ReadFromRemoteController()
{
	// no-op if we have no valid data from remote
	if( !RemoteControlSocket || RemoteControlSocket->InXmlStatus.status != pugi::status_ok ) return;
	
	UE_LOG( LogTemp, Log, TEXT( "GOT XML DATA" ) );

	// handle all setter commands here and postpone getter handling to WriteToRemoteController()
	if( pugi::xml_node node = RemoteControlSocket->InXml.child( "setActuators" ) )
	{
		//...
	}
}




void AControlledRagdoll::WriteToRemoteController()
{
	// no-op if we have no valid data from remote
	if( !RemoteControlSocket || RemoteControlSocket->InXmlStatus.status != pugi::status_ok ) return;

	// handle all getter commands here; setters were handled in ReadFromRemoteController()
	if( RemoteControlSocket->InXml.child( "getSensors" ) )
	{
		//...
	}
	if( RemoteControlSocket->InXml.child( "getActuators" ) )
	{
		//...
	}
}
