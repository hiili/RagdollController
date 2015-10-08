// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"

#include "Utility.h"

#include "PrimitiveComponentSnapshot.h"




void UPrimitiveComponentSnapshot::SerializeTarget( FArchive & archive, UActorComponent & target )
{
	UPrimitiveComponent * uPrimitiveTarget = dynamic_cast<UPrimitiveComponent *>(&target);
	if( !uPrimitiveTarget ) return;

	if( archive.IsSaving() )
	{
		archive << Utility::as_lvalue( uPrimitiveTarget->GetComponentTransform() );
	}
	else
	{
		FTransform t;
		archive << t;
		uPrimitiveTarget->SetWorldTransform( t, /* bSweep =*/ false, /*OutSweepHitResult =*/ nullptr, ETeleportType::TeleportPhysics );
	}
}
