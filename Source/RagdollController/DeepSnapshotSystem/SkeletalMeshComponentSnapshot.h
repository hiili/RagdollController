// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "DeepSnapshotSystem/PrimitiveComponentSnapshot.h"
#include "SkeletalMeshComponentSnapshot.generated.h"

/**
 * Deep snapshot storage component for SkeletalMeshComponent targets.
 */
UCLASS( ClassGroup = (Custom), meta = (BlueprintSpawnableComponent) )
class RAGDOLLCONTROLLER_API USkeletalMeshComponentSnapshot : public UPrimitiveComponentSnapshot
{
	GENERATED_BODY()
	

protected:

	virtual void SerializeTarget( FArchive & archive, UActorComponent & target ) override;
	virtual bool IsAcceptableTargetType( UActorComponent * targetCandidate ) const override;


private:

	/** If serializing, then write a compatibility check block to the archive. If deserializing, read such a block and runs the test. Return true if
	 ** compatible, false otherwise. */
	bool RunBinaryCompatibilityTest( FArchive & archive );

};
