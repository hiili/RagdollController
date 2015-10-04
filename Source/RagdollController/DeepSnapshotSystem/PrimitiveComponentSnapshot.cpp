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
		archive << Utility::as_lvalue( uPrimitiveTarget->GetComponentLocation() );
	}
	else
	{
		FVector v;
		archive << v;
		uPrimitiveTarget->SetWorldLocation( v );
	}
}
