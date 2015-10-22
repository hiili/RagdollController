// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"

#include "Utility.h"

#include "PrimitiveComponentSnapshot.h"




void UPrimitiveComponentSnapshot::SerializeTarget( FArchive & archive, UActorComponent & target )
{
	Super::SerializeTarget( archive, target );

	// downcast the target, log and return if wrong type
	UPrimitiveComponent * primitiveTarget = dynamic_cast<UPrimitiveComponent *>(&target);
	if( !primitiveTarget )
	{
		LogFailedDowncast( __FUNCTION__ );
		return;
	}

	if( archive.IsSaving() )
	{
		// snapshot: serialize
		archive << Utility::as_lvalue( primitiveTarget->GetComponentTransform() );
		archive << Utility::as_lvalue( primitiveTarget->GetPhysicsLinearVelocity() );
		archive << Utility::as_lvalue( primitiveTarget->GetPhysicsAngularVelocity() );
	}
	else
	{
		// recall: deserialize

		FTransform t;
		FVector v;

		archive << t;
		primitiveTarget->SetWorldTransform( t, /* bSweep =*/ false, /*OutSweepHitResult =*/ nullptr, ETeleportType::TeleportPhysics );

		archive << v;
		primitiveTarget->SetPhysicsLinearVelocity( v );

		archive << v;
		primitiveTarget->SetPhysicsAngularVelocity( v );
	}
}
