// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController45.h"
#include "ControlledRagdoll.h"

#include "Net/UnrealNetwork.h"
#include "GenericPlatform/GenericPlatformProperties.h"

#include <extensions/PxD6Joint.h>
#include <PxRigidBody.h>
#include <PxRigidDynamic.h>
#include <PxTransform.h>

#include "ScopeGuard.h"


AControlledRagdoll::AControlledRagdoll(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// enable ticking
	PrimaryActorTick.bCanEverTick = true;
}




void AControlledRagdoll::GetLifetimeReplicatedProps( TArray<FLifetimeProperty> & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( AControlledRagdoll, BoneStates );
	DOREPLIFETIME( AControlledRagdoll, IsServerLittleEndian );
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
			UE_LOG( LogTemp, Warning, TEXT( "(%s) Multiple USkeletalMeshComponents found! Using the first one." ), TEXT( __FUNCTION__ ) );
		}
	}
	else
	{
		UE_LOG( LogTemp, Error, TEXT( "(%s) No USkeletalMeshComponents found!" ), TEXT( __FUNCTION__ ) );
	}


	if( this->Role >= ROLE_Authority )
	{

		/* We are standalone or a server */
		
		// Store server platform endianness to a replicated flag
		this->IsServerLittleEndian = FGenericPlatformProperties::IsLittleEndian();

		// make sure that physics simulation is enabled also on a dedicated server
		if( this->SkeletalMeshComponent )
		{
			this->SkeletalMeshComponent->bEnablePhysicsOnDedicatedServer = true;
			this->SkeletalMeshComponent->SetSimulatePhysics( true ); // this must be called after bEnablePhysicsOnDedicatedServer = true even if physics are enabled also via editor!
		}
		else
		{
			UE_LOG( LogTemp, Error, TEXT( "(%s) Failed to enable SkeletalMeshComponent physics on dedicated server!" ), TEXT( __FUNCTION__ ) );
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
			UE_LOG( LogTemp, Error, TEXT( "(%s) Failed to switch SkeletalMeshComponent to kinematic mode on a network client!" ), TEXT( __FUNCTION__ ) );
		}

	}


	/* refresh joint data caches */

	refreshStaticJointData();

}




void AControlledRagdoll::BeginPlay()
{
	Super::BeginPlay();
}




void AControlledRagdoll::Tick( float deltaSeconds )
{
	Super::Tick( deltaSeconds );

	refreshDynamicJointData();

	
	// If network client, then apply the received pose from the server and return
	if( this->Role < ROLE_Authority )
	{
		receivePose();
		return;
	}

	// Standalone or server: store pose so that it can be replicated to client(s)
	sendPose();




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






void AControlledRagdoll::refreshStaticJointData()
{
	// init the error cleanup scope guard
	auto sgError = MakeScopeGuard( [this](){
		UE_LOG( LogTemp, Error, TEXT( "(%s) Scope guard 'sgError' triggered!" ), TEXT( __FUNCTION__ ) );

		// erase all data if any errors
		this->JointStates.Empty();
	} );


	// check that we have everything we need, otherwise bail out
	if( !this->SkeletalMeshComponent ) return;

	// check the number of joints and preallocate the top level array
	int32 numJoints = this->SkeletalMeshComponent->Constraints.Num();
	this->JointStates.SetNum( numJoints );
	this->JointNames.SetNum( numJoints );

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

		// bail out if something was not available
		if( !this->JointStates[joint].Bodies[0] || !this->JointStates[joint].Bodies[1]
			|| this->JointStates[joint].BoneInds[0] == INDEX_NONE || this->JointStates[joint].BoneInds[1] == INDEX_NONE ) return;
	}

	// all good, release the error cleanup scope guard and return
	sgError.release();
	return;
}




void AControlledRagdoll::refreshDynamicJointData()
{
	// init the error cleanup scope guard
	auto sgError = MakeScopeGuard( [this](){
		UE_LOG( LogTemp, Error, TEXT( "(%s) Scope guard 'sgError' triggered!" ), TEXT( __FUNCTION__ ) );

		// erase all data (including static joint data!) if any errors
		this->JointStates.Empty();
	} );


	// check that we have everything we need, otherwise bail out
	if( !this->SkeletalMeshComponent || this->JointStates.Num() == 0 ) return;

	// check that the array size matches the skeleton's joint count
	check( this->JointStates.Num() == this->SkeletalMeshComponent->Constraints.Num() );
	//check( false );

	// loop through joints
	for( auto & jointState : this->JointStates )
	{
		// check availability of PhysX data
		if( !jointState.Constraint->ConstraintData ) return;

		// store the global rotations of the connected bones
		for( int i = 0; i < 2; ++i ) {
			jointState.BoneGlobalRotations[i] = this->SkeletalMeshComponent->GetBoneTransform( jointState.BoneInds[i] ).GetRotation();
		}

		// store the joint rotation
		jointState.Rotation = FRotator(
			jointState.Constraint->ConstraintData->getTwist(),
			jointState.Constraint->ConstraintData->getSwingYAngle(),
			jointState.Constraint->ConstraintData->getSwingZAngle() );
	}

	// all good, release the error cleanup scope guard and return
	sgError.release();
	return;
}




void AControlledRagdoll::sendPose()
{
	// check the number of bones and resize the BoneStates array
	int numBodies = this->SkeletalMeshComponent->Bodies.Num();
	this->BoneStates.SetNum( numBodies );

	// loop through bones and write out state data
	for( int body = 0; body < numBodies; ++body )
	{
		physx::PxRigidDynamic * pxBody = this->SkeletalMeshComponent->Bodies[body]->GetPxRigidDynamic();
		if( !pxBody )
		{
			UE_LOG( LogTemp, Error, TEXT( "(%s) GetPxRididDynamic() failed for body %d!" ), TEXT( __FUNCTION__ ), body );
			return;
		}

		// Replicate pose, skip velocities
		// (PhysX ignores the velocities if the pose is set during the same tick. Also, we do not want any client-side prediction but just exact replication.)
		this->BoneStates[body].GetPxTransform() = pxBody->getGlobalPose();
		//this->BoneStates[body].GetPxLinearVelocity() = pxBody->getLinearVelocity();
		//this->BoneStates[body].GetPxAngularVelocity() = pxBody->getAngularVelocity();
	}
}




void AControlledRagdoll::receivePose()
{
	int numBodies = this->BoneStates.Num();

	// Verify that the skeletal meshes have the same number of bones (for example, one might not be initialized yet)
	if( numBodies != this->SkeletalMeshComponent->Bodies.Num() )
	{
		UE_LOG( LogTemp, Error, TEXT( "(%s) Number of bones do not match. Cannot replicate pose!" ), TEXT( __FUNCTION__ ) );
		return;
	}

	// Check that endianness and word sizes match
	if( this->IsServerLittleEndian != FGenericPlatformProperties::IsLittleEndian()
		|| (this->BoneStates.Num() > 0 && !this->BoneStates[0].DoDataSizesMatch()) )
	{
		UE_LOG( LogTemp, Error, TEXT( "(%s) Endianity or bone state data sizes do not match. Cannot replicate pose!" ), TEXT( __FUNCTION__ ) );
		return;
	}

	// Loop through bones and apply received replication data to each
	for( int body = 0; body < numBodies; ++body )
	{
		physx::PxRigidDynamic * pxBody = this->SkeletalMeshComponent->Bodies[body]->GetPxRigidDynamic();
		if( !pxBody )
		{
			UE_LOG( LogTemp, Error, TEXT( "(%s) GetPxRididDynamic() failed for body %d!" ), TEXT( __FUNCTION__ ), body );
			return;
		}

		// Replicate pose, skip velocities (see sendPose())
		pxBody->setGlobalPose( this->BoneStates[body].GetPxTransform() );
		//pxBody->setLinearVelocity( this->BoneStates[body].GetPxLinearVelocity() );
		//pxBody->setAngularVelocity( this->BoneStates[body].GetPxAngularVelocity() );
	}
}
