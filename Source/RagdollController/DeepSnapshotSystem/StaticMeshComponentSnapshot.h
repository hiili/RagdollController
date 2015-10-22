// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "DeepSnapshotSystem/PrimitiveComponentSnapshot.h"
#include "StaticMeshComponentSnapshot.generated.h"

/**
 * Deep snapshot storage component for StaticMeshComponent targets.
 */
UCLASS( ClassGroup = (Custom), meta = (BlueprintSpawnableComponent) )
class RAGDOLLCONTROLLER_API UStaticMeshComponentSnapshot : public UPrimitiveComponentSnapshot
{
	GENERATED_BODY()


protected:

	virtual void SerializeTarget( FArchive & archive, UActorComponent & target ) override;
	virtual bool IsAcceptableTargetType( UActorComponent * targetCandidate ) const override;

};
