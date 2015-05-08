// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/LevelScriptActor.h"

#include <boost/circular_buffer.hpp>
#include <unordered_set>

#include "RCLevelScriptActor.generated.h"




/**
 * 
 */
UCLASS( Config = RagdollController )
class RAGDOLLCONTROLLER_API ARCLevelScriptActor : public ALevelScriptActor
{
	GENERATED_BODY()


	/** Average tick rate estimation: timestamps for the last n ticks */
	boost::circular_buffer<double> tickTimestamps;

	/** Actors registered for managed NetUpdateFrequency. @see RegisterManagedNetUpdateFrequency, UnregisterManagedNetUpdateFrequency */
	std::unordered_set<AActor *> NetUpdateFrequencyManagedActors;


	/** Cap the tick rate. Only operates when using fixed time steps (otherwise no-op). */
	void HandleMaxTickRate( const float MaxTickRate );

	/** Estimate and log the current average tick rate. */
	void estimateAverageTickRate();

	/** Manage net update frequencies of the registered actors (@see NetUpdateFrequencyManagedActors). */
	void manageNetUpdateFrequencies( float gameDeltaTime );


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

	/** Target real-time value for AActor::NetUpdateFrequency (the nominal value must be corrected by the wall clock vs game time fps difference).
	 ** This is used for actors that have registered for automatic NetUpdateFrequency management. @see RegisterManagedNetUpdateFrequency */
	UPROPERTY( Config )
	float RealtimeNetUpdateFrequency = 60.f;


	/** Computed estimate of the current average tick rate (averaging window length is controlled by ESTIMATE_TICKRATE_SAMPLES). At least ControlledRagdoll
	 ** uses this for server-to-client bandwidth capping in replication. */
	float currentAverageTickRate;


	ARCLevelScriptActor();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick( float deltaSeconds ) override;


	/** Register an actor so as to have its NetUpdateFrequency automatically corrected on each tick, so as to take into account the simulation time vs. wall
	 ** clock time difference; UE does not take care of this in our case of using fixed time steps. No-op with a logged warning if the actor is already
	 ** registered. @see UnregisterManagedNetUpdateFrequency */
	void RegisterManagedNetUpdateFrequency( AActor * actor );

	/** Unregister an actor from receiving automatic NetUpdateFrequency updates. No-op with a logged warning if the actor has not been registered.
	 ** @see RegisterManagedNetUpdateFrequency */
	void UnregisterManagedNetUpdateFrequency( AActor * actor );

};
