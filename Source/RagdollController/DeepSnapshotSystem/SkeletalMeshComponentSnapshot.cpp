// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "SkeletalMeshComponentSnapshot.h"

#include "Utility.h"

#include <extensions/PxD6Joint.h>
#include <PxRigidBody.h>
#include <PxRigidDynamic.h>
#include <PxTransform.h>




static FArchive & operator<<(FArchive & archive, physx::PxTransform & data)
{
	archive.Serialize( &data, sizeof(data) );
	return archive;
}


static FArchive & operator<<(FArchive & archive, physx::PxVec3 & data)
{
	archive.Serialize( &data, sizeof(data) );
	return archive;
}




void USkeletalMeshComponentSnapshot::SerializeTarget( FArchive & archive, UActorComponent & target )
{
	Super::SerializeTarget( archive, target );

	// downcast the target, log and return if wrong type
	USkeletalMeshComponent * skeletalMeshTarget = dynamic_cast<USkeletalMeshComponent *>(&target);
	if( !skeletalMeshTarget )
	{
		LogFailedDowncast( __FUNCTION__ );
		return;
	}

	// check the number of bones in target
	int32 numBodies = skeletalMeshTarget->Bodies.Num();

	if( archive.IsSaving() )
	{
		// snapshot: serialize

		// store the number of bones
		archive << numBodies;

		// loop through bones and store their state data
		for( int body = 0; body < numBodies; ++body )
		{
			physx::PxRigidDynamic * pxBody = skeletalMeshTarget->Bodies[body]->GetPxRigidDynamic();
			if( !pxBody )
			{
				UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) GetPxRididDynamic() failed for body %d!" ), TEXT( __FUNCTION__ ), body );
				UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "%s" ), *LogCreateDiagnosticLine() );
				return;
			}

			// Copy pose from the skeletal mesh to archive
			archive << Utility::as_lvalue( pxBody->getGlobalPose() );
			archive << Utility::as_lvalue( pxBody->getLinearVelocity() );
			archive << Utility::as_lvalue( pxBody->getAngularVelocity() );
		}
	}
	else   /////////////////// MERGE 'EM!
	{
		// recall: deserialize

		// read the number of bones in archive
		int32 numBodiesIncoming;
		archive << numBodiesIncoming;

		// verify that the skeletal meshes have the same number of bones (for example, one might not be initialized yet)
		if( numBodiesIncoming != numBodies )
		{
			UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) Number of bones do not match. Cannot perform the recall!" ), TEXT( __FUNCTION__ ) );
			UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "%s" ), *LogCreateDiagnosticLine() );
			return;
		}

		// Loop through bones and apply data from storage to each
		for( int body = 0; body < numBodies; ++body )
		{
			physx::PxRigidDynamic * pxBody = skeletalMeshTarget->Bodies[body]->GetPxRigidDynamic();
			if( !pxBody )
			{
				UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) GetPxRididDynamic() failed for body %d!" ), TEXT( __FUNCTION__ ), body );
				UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "%s" ), *LogCreateDiagnosticLine() );
				return;
			}

			// Copy pose from archive to the skeletal mesh

			physx::PxTransform t;
			physx::PxVec3 v;

			archive << t;
			pxBody->setGlobalPose( t );

			archive << v;
			pxBody->setLinearVelocity( v );

			archive << v;
			pxBody->setAngularVelocity( v );
		}
	}
}




bool USkeletalMeshComponentSnapshot::IsAcceptableTargetType( UActorComponent * targetCandidate ) const
{
	return dynamic_cast<USkeletalMeshComponent *>(targetCandidate) != nullptr;
}
