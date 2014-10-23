// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/GameMode.h"
#include "RagdollController45GameMode.generated.h"

/**
 * 
 */
UCLASS()
class RAGDOLLCONTROLLER45_API ARagdollController45GameMode : public AGameMode
{
	GENERATED_UCLASS_BODY()

	
	/* Cap FPS. Only operates when using fixed time steps (otherwise no-op). */
	void HandleMaxTickRate( const float MaxTickRate );


public:

	void Tick( float DeltaSeconds ) override;

};
