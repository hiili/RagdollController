// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "SkeletalMeshComponentSnapshot.h"

#include "PhysicsPublic.h"
#include "PhysXPublic.h"

#include "Utility.h"




void USkeletalMeshComponentSnapshot::SerializeTarget( FArchive & archive, UActorComponent & target )
{
	Super::SerializeTarget( archive, target );

	check( archive.IsSaving() || archive.IsLoading() );

	// downcast the target, log and return if wrong type
	USkeletalMeshComponent * skeletalMeshTarget = dynamic_cast<USkeletalMeshComponent *>(&target);
	if( !skeletalMeshTarget )
	{
		LogFailedDowncast( __FUNCTION__ );
		return;
	}


	// check the number of bodies in target
	int32 numBodies = skeletalMeshTarget->Bodies.Num();

	// sync the number of bodies with archive; numBodies is re-assigned if loading
	archive << numBodies;

	// if recalling, then check that the number of bodies still matches our target (for example, one might not be initialized yet)
	if( archive.IsLoading() && numBodies != skeletalMeshTarget->Bodies.Num() )
	{
		UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) Number of bones do not match. Cannot recall!" ), TEXT( __FUNCTION__ ) );
		UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "%s" ), *LogCreateDiagnosticLine() );
		return;
	}

	// binary compatibility check
	if( !RunBinaryCompatibilityTest( archive ) )
	{
		// mismatch
		UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) Server and client PhysX instances are not binary compatible. Cannot replicate pose!" ),
			TEXT( __FUNCTION__ ) );
		return;
	}

	// lock the scene and loop through bodies
	check( GetWorld() && GetWorld()->GetPhysicsScene() && GetWorld()->GetPhysicsScene()->GetPhysXScene( PST_Sync ) );
	SCOPED_SCENE_READ_LOCK( GetWorld()->GetPhysicsScene()->GetPhysXScene( PST_Sync ) );
	for( int body = 0; body < numBodies; ++body )
	{
		// get the physx handle for this body
		physx::PxRigidDynamic * pxBody = skeletalMeshTarget->Bodies[body]->GetPxRigidDynamic_AssumesLocked();
		if( !pxBody )
		{
			UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) GetPxRididDynamic() failed for body %d!" ), TEXT( __FUNCTION__ ), body );
			UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "%s" ), *LogCreateDiagnosticLine() );
			return;
		}

		// Sync the state of the current body with archive
		if( archive.IsSaving() )
		{
			// snapshot: serialize

			physx::PxTransform t;
			physx::PxVec3 v;

			t = pxBody->getGlobalPose();
			archive.Serialize( &t, sizeof(t) );

			v = pxBody->getLinearVelocity();
			archive.Serialize( &v, sizeof(v) );

			v = pxBody->getAngularVelocity();
			archive.Serialize( &v, sizeof(v) );
		}
		else
		{
			// recall: deserialize

			physx::PxTransform t;
			physx::PxVec3 v;

			archive.Serialize( &t, sizeof(t) );
			pxBody->setGlobalPose( t );

			archive.Serialize( &v, sizeof(v) );
			pxBody->setLinearVelocity( v );

			archive.Serialize( &v, sizeof(v) );
			pxBody->setAngularVelocity( v );
		}
	}
}




bool USkeletalMeshComponentSnapshot::IsAcceptableTargetType( UActorComponent * targetCandidate ) const
{
	return dynamic_cast<USkeletalMeshComponent *>(targetCandidate) != nullptr;
}




bool USkeletalMeshComponentSnapshot::RunBinaryCompatibilityTest( FArchive & archive )
{
	uint8 isLittleEndian = *reinterpret_cast<uint8 *>(&Utility::as_lvalue( uint32( 1 ) ));
	uint8 sizeOfPxTransform = sizeof(physx::PxTransform);
	uint8 sizeOfPxVec3 = sizeof(physx::PxVec3);

	archive << isLittleEndian;
	archive << sizeOfPxTransform;
	archive << sizeOfPxVec3;
	archive << Utility::as_lvalue( uint8( 0 ) );   // maintain at least some alignment

	return archive.IsSaving() ||
		(isLittleEndian == *reinterpret_cast<uint8 *>(&Utility::as_lvalue( uint32( 1 ) )) &&
		sizeOfPxTransform == sizeof(physx::PxTransform) &&
		sizeOfPxVec3 == sizeof(physx::PxVec3));
}
