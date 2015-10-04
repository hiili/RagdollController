// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "DeepSnapshotSystem/PrimitiveComponentSnapshot.h"
#include "SkeletalMeshComponentSnapshot.generated.h"

/**
 * 
 */
UCLASS( ClassGroup = (Custom), meta = (BlueprintSpawnableComponent) )
class RAGDOLLCONTROLLER_API USkeletalMeshComponentSnapshot : public UPrimitiveComponentSnapshot
{
	GENERATED_BODY()
	

protected:

	virtual void SerializeTarget( FArchive & archive, UActorComponent & target ) override;

	virtual bool IsAcceptableTargetType( UActorComponent * targetCandidate ) override
	{
		return dynamic_cast<USkeletalMeshComponent *>(targetCandidate) != nullptr;
	}

};
