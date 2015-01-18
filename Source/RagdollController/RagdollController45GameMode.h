// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/GameMode.h"
#include "RagdollController45GameMode.generated.h"




/**
 * 
 */
UCLASS(Config=RagdollController)
class RAGDOLLCONTROLLER45_API ARagdollController45GameMode : public AGameMode
{
	GENERATED_UCLASS_BODY()

	
	/** Cap FPS. Only operates when using fixed time steps (otherwise no-op). */
	void HandleMaxTickRate( const float MaxTickRate );

	/** Estimate and log the current average frame rate. */
	void estimateAverageFrameRate();


protected:


public:

	/** The constant frame rate to be used. This is allowed to be changed run-time. */
	UPROPERTY(Config)
	float FixedFps;

	/** Estimate of the current average frame rate (averaging window length is controlled by ESTIMATE_FRAMERATE_SAMPLES) */
	float currentAverageFps;


	virtual void InitGame( const FString & MapName, const FString & Options, FString & ErrorMessage ) override;
	void Tick( float DeltaSeconds ) override;

};