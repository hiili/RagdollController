// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/GameEngine.h"
#include "MyGameEngine.generated.h"

/**
 * 
 */
UCLASS()
class RAGDOLLCONTROLLER45_API UMyGameEngine : public UGameEngine
{
	GENERATED_UCLASS_BODY()


public:

	virtual float GetMaxTickRate( float DeltaTime, bool bAllowFrameRateSmoothing = true ) override;
	
};
