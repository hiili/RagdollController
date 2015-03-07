// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "ControlledRagdoll.h"

#include "RCLevelScriptActor.h"
#include "ScopeGuard.h"
#include "Utility.h"

#include <Net/UnrealNetwork.h>
#include <GenericPlatform/GenericPlatformProperties.h>

#include <extensions/PxD6Joint.h>
#include <PxRigidBody.h>
#include <PxRigidDynamic.h>
#include <PxTransform.h>

#include <cstring>
#include <cmath>


/** Target real-time value for AActor::NetUpdateFrequency (the nominal value must be corrected by the wall clock vs game time fps difference). */
#define NET_UPDATE_FREQUENCY 60.f




AControlledRagdoll::AControlledRagdoll()
	: IRemoteControllable( this ),
	lastSendPoseTime( -INFINITY )
{
}




void AControlledRagdoll::GetLifetimeReplicatedProps( TArray<FLifetimeProperty> & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( AControlledRagdoll, BoneStates );
	DOREPLIFETIME( AControlledRagdoll, ServerInterpretationOfDeadbeef );
}




void AControlledRagdoll::PostInitializeComponents()
{
	Super::PostInitializeComponents();


	/* init SkeletalMeshComponent: scan all components and choose the first USkeletalMeshComponent */

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
		UE_LOG( LogRcCr, Error, TEXT( "(%s) No USkeletalMeshComponents found!" ), TEXT( __FUNCTION__ ) );
	}


	if( this->Role >= ROLE_Authority )
	{

		/* We are standalone or a server */
		
		// Store server's interpretation of 0xdeadbeef to a replicated float
		std::memcpy( &this->ServerInterpretationOfDeadbeef, "\xde\xad\xbe\xef", sizeof(this->ServerInterpretationOfDeadbeef) );

		// make sure that physics simulation is enabled also on a dedicated server
		if( this->SkeletalMeshComponent )
		{
			this->SkeletalMeshComponent->bEnablePhysicsOnDedicatedServer = true;
			this->SkeletalMeshComponent->SetSimulatePhysics( true ); // this must be called after bEnablePhysicsOnDedicatedServer = true even if physics are enabled also via editor!
		}
		else
		{
			UE_LOG( LogRcCr, Error, TEXT( "(%s) Failed to enable SkeletalMeshComponent physics on dedicated server!" ), TEXT( __FUNCTION__ ) );
		}

	}
	else
	{

		/* We are a network client, assume spectator role (although UE spectator mode is not used explicitly at the moment) */

		// Set the skeletal mesh to kinematic mode, so as to track exactly the pose stream received from the server (we do not want any local physics interactions or simulation)
		if( this->SkeletalMeshComponent )
		{
			// cannot do this, as UE appears to be using internally a different the coordinate system for kinematic skeletons!
			//this->SkeletalMeshComponent->SetSimulatePhysics( false );
		}
		else
		{
			UE_LOG( LogRcCr, Error, TEXT( "(%s) Failed to switch SkeletalMeshComponent to kinematic mode on a network client!" ), TEXT( __FUNCTION__ ) );
		}

	}


	// Initialize the internal state structs
	InitState();

}




void AControlledRagdoll::BeginPlay()
{
	Super::BeginPlay();
}




void AControlledRagdoll::Tick( float deltaSeconds )
{
	// If network client, then we are just visualizing the ragdoll that is being simulated on the server
	if( this->Role < ROLE_Authority )
	{
		// Replicate pose from the server
		ReceivePose();

		// Call the tick hook (available for inherited C++ classes), then Super::Tick(), which runs this actor's Blueprint
		TickHook( deltaSeconds );
		Super::Tick( deltaSeconds );

		return;
	}


	/* We are standalone or a server */
	

	/* Relay data between the game engine and the remote controller, while calling additional tick functionality in between. */

	// Read inbound data
	ReadFromSimulation();
	ReadFromRemoteController();

	// Call the tick hook (available for inherited C++ classes), then Super::Tick(), which runs this actor's Blueprint
	TickHook( deltaSeconds );
	Super::Tick( deltaSeconds );
	ValidateBlueprintWritables();

	// Write outbound data
	WriteToSimulation();
	WriteToRemoteController();


	/* Handle client-server pose replication */

	// Adjust net update frequency based on the current wall clock vs game time fps difference
	RecomputeNetUpdateFrequency( deltaSeconds );

	// Store pose so that it can be replicated to client(s)
	SendPose();



	return;




	static int tickCounter = -1;
	++tickCounter;

	int32 jointInd = this->JointNames.Find( FName( "upperarm_l" ) );

    // multiplication order verified (using joint upperarm_l, bone 2)
	FQuat rf1global = this->JointStates[jointInd].BoneGlobalRotations[0] * this->JointStates[jointInd].RefFrameRotations[0];
	FQuat rf2global = this->JointStates[jointInd].BoneGlobalRotations[1] * this->JointStates[jointInd].RefFrameRotations[1];

	for( TActorIterator<AActor> ActorItr( GetWorld() ); ActorItr; ++ActorItr )
	{
		if( ActorItr->GetClass()->GetName() == "BasisVectorsBP_C" )
		{
			FTransform t = this->SkeletalMeshComponent->GetBoneTransform( this->JointStates[jointInd].BoneInds[0] );
			t.SetRotation( rf1global );
			ActorItr->SetActorTransform( t );

			break;
		}
	}

	this->JointStates[jointInd].Bodies[0]->AddTorque( 200000.f * sin( float( tickCounter ) / 20.f ) * rf1global.RotateVector( FVector( 0.f, 0.f, 1.f ) ) );
	this->JointStates[jointInd].Bodies[1]->AddTorque( -200000.f * sin( float( tickCounter ) / 20.f ) * rf1global.RotateVector( FVector( 0.f, 0.f, 1.f ) ) );
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


	// check that we have everything we need, otherwise bail out
	if( !this->SkeletalMeshComponent || this->JointStates.Num() == 0 ) return;

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
			jointState.Constraint->ConstraintData->getTwist(),
			jointState.Constraint->ConstraintData->getSwingYAngle(),
			jointState.Constraint->ConstraintData->getSwingZAngle() );

		// physx::PxD6Joint::getTwist() ignores the sign of q.x, fix this here
		jointState.JointAngles[0] *= jointState.Constraint->ConstraintData->getRelativeTransform().q.x >= 0.f ?
			1.f : -1.f;
	}

	// all good, release the error cleanup scope guard and return
	sgError.release();
	return;
}




void AControlledRagdoll::ReadFromRemoteController()
{
	// no-op if no remote controller
	if( !this->IRemoteControllable::RemoteControlSocket.IsValid() ) return;

	// ...
}




void AControlledRagdoll::WriteToSimulation()
{
	// init the error cleanup scope guard
	auto sgError = MakeScopeGuard( [this](){
		UE_LOG( LogRcCr, Error, TEXT( "(%s) Internal error! (scope guard 'sgError' was triggered)" ), TEXT( __FUNCTION__ ) );

		// erase all data (including static joint data!) if any errors
		this->JointStates.Empty();
	} );


	// check that we have everything we need, otherwise bail out
	if( !this->SkeletalMeshComponent || this->JointStates.Num() == 0 ) return;

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




void AControlledRagdoll::WriteToRemoteController()
{
	// no-op if no remote controller
	if( !this->IRemoteControllable::RemoteControlSocket.IsValid() ) return;

	// ...
}




void AControlledRagdoll::RecomputeNetUpdateFrequency( float gameDeltaTime )
{
	// get a pointer to our LevelScriptActor instance
	check( GetWorld() );
	if( ARCLevelScriptActor * ls = Cast<ARCLevelScriptActor>( GetWorld()->GetLevelScriptActor() ) )
	{
		// compute how fast the simulation is running with respect to wall clock time
		float currentSpeedMultiplier = ls->currentAverageFps / (1.f / gameDeltaTime);

		// apply to AActor::NetUpdateFrequency
		this->NetUpdateFrequency = NET_UPDATE_FREQUENCY / currentSpeedMultiplier;
	}
	else
	{
		UE_LOG( LogRcCr, Error, TEXT( "(%s) Failed to locate our RCLevelScriptActor!" ), TEXT( __FUNCTION__ ) );
	}
}




void AControlledRagdoll::SendPose()
{
	// cap update rate by 2 * NET_UPDATE_FREQUENCY (UE level replication intervals are not accurate, have a safety margin so as to not miss any replications)
	double currentTime = FPlatformTime::Seconds();
	if( currentTime - this->lastSendPoseTime < 1.f / (2.f * NET_UPDATE_FREQUENCY) ) return;
	this->lastSendPoseTime = currentTime;

	// check the number of bones and resize the BoneStates array
	int numBodies = this->SkeletalMeshComponent->Bodies.Num();
	this->BoneStates.SetNum( numBodies );

	// loop through bones and write out state data
	for( int body = 0; body < numBodies; ++body )
	{
		physx::PxRigidDynamic * pxBody = this->SkeletalMeshComponent->Bodies[body]->GetPxRigidDynamic();
		if( !pxBody )
		{
			UE_LOG( LogRcCr, Error, TEXT( "(%s) GetPxRididDynamic() failed for body %d!" ), TEXT( __FUNCTION__ ), body );
			return;
		}

		// Replicate pose, skip velocities
		// (PhysX ignores the velocities if the pose is set during the same tick. Also, we do not want any client-side prediction but just exact replication.)
		this->BoneStates[body].GetPxTransform() = pxBody->getGlobalPose();
		//this->BoneStates[body].GetPxLinearVelocity() = pxBody->getLinearVelocity();
		//this->BoneStates[body].GetPxAngularVelocity() = pxBody->getAngularVelocity();
	}
}




void AControlledRagdoll::ReceivePose()
{
	int numBodies = this->BoneStates.Num();

	// Verify that the skeletal meshes have the same number of bones (for example, one might not be initialized yet, or replication might have not yet started).
	if( numBodies != this->SkeletalMeshComponent->Bodies.Num() )
	{
		UE_LOG( LogRcCr, Error, TEXT( "(%s) Number of bones do not match. Cannot replicate pose!" ), TEXT( __FUNCTION__ ) );
		return;
	}

	// Check for float binary compatibility (eg endianness) and that the PhysX data sizes match. Assume that UE replicates floats always correctly.
	float ourInterpretationOfDeadbeef;
	std::memcpy( &ourInterpretationOfDeadbeef, "\xde\xad\xbe\xef", sizeof(ourInterpretationOfDeadbeef) );
	if( std::abs( (this->ServerInterpretationOfDeadbeef / ourInterpretationOfDeadbeef) - 1.f ) > 1e-6f
		|| (this->BoneStates.Num() > 0 && !this->BoneStates[0].DoDataSizesMatch()) )
	{
		UE_LOG( LogRcCr, Error, TEXT( "(%s) Floats are not binary compatible or bone state data sizes do not match. Cannot replicate pose!" ), TEXT( __FUNCTION__ ) );
		return;
	}

	// Loop through bones and apply received replication data to each
	for( int body = 0; body < numBodies; ++body )
	{
		physx::PxRigidDynamic * pxBody = this->SkeletalMeshComponent->Bodies[body]->GetPxRigidDynamic();
		if( !pxBody )
		{
			UE_LOG( LogRcCr, Error, TEXT( "(%s) GetPxRididDynamic() failed for body %d!" ), TEXT( __FUNCTION__ ), body );
			return;
		}

		// Replicate pose, skip velocities (see SendPose())
		pxBody->setGlobalPose( this->BoneStates[body].GetPxTransform() );
		//pxBody->setLinearVelocity( this->BoneStates[body].GetPxLinearVelocity() );
		//pxBody->setAngularVelocity( this->BoneStates[body].GetPxAngularVelocity() );
	}
}
