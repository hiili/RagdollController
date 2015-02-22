// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/LevelScriptActor.h"
#include "RCLevelScriptActor.generated.h"

/**
 * 
 */
UCLASS( Config = RagdollController )
class RAGDOLLCONTROLLER_API ARCLevelScriptActor : public ALevelScriptActor
{
	GENERATED_BODY()
	

	/** Cap FPS. Only operates when using fixed time steps (otherwise no-op). */
	void HandleMaxTickRate( const float MaxTickRate );

	/** Estimate and log the current average frame rate. */
	void estimateAverageFrameRate();


public:

	/* .ini configuration */

	/** Whether to attempt to connect to PhysX Visual Debugger (slows down startup for a second or two if PVD not found) */
	UPROPERTY( Config )
	bool ConnectToPhysXVisualDebugger = false;

	/** The constant virtual (game-time) tick rate to be used. The real (wall-clock) tick rate of clients and standalone instances is always capped to not
	 ** exceed this. */
	UPROPERTY( Config )
	float FixedFps = 60.f;

	/** Whether to cap the real (wall-clock) tick rate by FixedFps also on dedicated servers. */
	UPROPERTY( Config )
	bool CapServerTickRate = false;


	/** Computed estimate of the current average frame rate (averaging window length is controlled by ESTIMATE_FRAMERATE_SAMPLES). At least ControlledRagdoll
	 ** uses this for server-to-client bandwidth capping in replication. */
	float currentAverageFps;


	ARCLevelScriptActor();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick( float deltaSeconds ) override;
	
};
