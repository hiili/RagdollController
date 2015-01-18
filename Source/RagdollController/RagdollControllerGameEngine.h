// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/GameEngine.h"
#include "RagdollControllerGameEngine.generated.h"

/**
 * 
 */
UCLASS()
class RAGDOLLCONTROLLER_API URagdollControllerGameEngine : public UGameEngine
{
	GENERATED_BODY()


public:

	URagdollControllerGameEngine();

	/** Override the dedicated server hard-coded tick rate cap. */
	virtual float GetMaxTickRate( float DeltaTime, bool bAllowFrameRateSmoothing = true ) const override;
	
};
