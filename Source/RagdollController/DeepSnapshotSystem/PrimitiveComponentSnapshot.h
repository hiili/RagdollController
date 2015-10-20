// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "DeepSnapshotSystem/DeepSnapshotBase.h"
#include "PrimitiveComponentSnapshot.generated.h"

/**
 * Abstract middle type for deep snapshot storage components that target PrimitiveComponent-derived classes.
 */
UCLASS( Abstract )
class RAGDOLLCONTROLLER_API UPrimitiveComponentSnapshot : public UDeepSnapshotBase
{
	GENERATED_BODY()
	
	
protected:

	virtual void SerializeTarget( FArchive & archive, UActorComponent & target ) override;

};