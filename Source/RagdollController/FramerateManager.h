// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include "ObjectSelector.h"

#include <boost/circular_buffer.hpp>
#include <unordered_set>

#include "FramerateManager.generated.h"




/**
 * Perform frame rate management operations, including overall frame rate management and NetUpdateFrequency management of all registered actors.
 */
UCLASS()
class RAGDOLLCONTROLLER_API AFramerateManager : public AActor
{
	GENERATED_BODY()
	
public:


	// Sets default values for this actor's properties
	AFramerateManager();




	/* UE interface overrides */


	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;




	/* frame rate management */


public:

	/** Returns the computed estimate of the current average tick rate (averaging window length is controlled by ESTIMATE_TICKRATE_SAMPLES). */
	UFUNCTION( BlueprintCallable, Category = "Frame rate management" )
	float getCurrentAverageTickRate() const { return currentAverageTickRate; }

	/** Computed estimate of the current average tick rate of the authoritative world: if server or standalone, then this equals the value returned by
	 *  getCurrentAverageTickRate(), otherwise this is a replicated copy of the server's currentAverageTickRate. */
	UFUNCTION( BlueprintCallable, Category = "Frame rate management" )
	float getCurrentAverageAuthorityTickRate() const { return currentAverageAuthorityTickRate; }


	/** Get the current constant virtual (game-time) tick rate. @see SetFixedFps() */
	UFUNCTION( BlueprintCallable, Category = "Frame rate management" )
	float GetFixedFps() const { return FixedFps; }

	/** Set a new constant virtual (game-time) tick rate to be used. The real (wall-clock) tick rate of clients and standalone instances is always capped to not
	 ** exceed this. @see GetFixedFps() */
	UFUNCTION( BlueprintCallable, Category = "Frame rate management" )
	void SetFixedFps( float newFixedFps );

	/** Whether to cap the real (wall-clock) frame rate by FixedFps also on dedicated servers. */
	UPROPERTY( EditAnywhere, Category = "Frame rate management" )
	bool CapServerFps = false;

	/** Whether to sync the game speed of network clients with the game speed of the authority. This can be useful in case of a non-realtime server (eg, a
	 *  simulation server that uses fixed time steps and no tick rate capping). At the moment, this is implemented by adjusting the world time dilation. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Frame rate management" )
	bool SyncGameSpeedWithServer;


private:

	/** Cap the tick rate. Only operates when using fixed time steps (otherwise no-op). */
	void handleMaxTickRate( const float MaxTickRate );

	/** Estimate and log the current average tick rate. */
	void estimateAverageTickRate();

	/** Re-adjust client game speed so as to match the server game speed. Has no effect if called on server / standalone. */
	void performSyncGameSpeedWithServer();


	/** The constant virtual (game-time) tick rate to be used. The real (wall-clock) tick rate of clients and standalone instances is always capped to not
	 ** exceed this. */
	UPROPERTY( EditAnywhere, Category = "Frame rate management" )
	float FixedFps = 60.f;


	/** Average frame rate estimation: timestamps for the last n ticks */
	boost::circular_buffer<double> tickTimestamps;

	/** Computed estimate of the current average tick rate (averaging window length is controlled by ESTIMATE_TICKRATE_SAMPLES). */
	float currentAverageTickRate;

	/** Computed estimate of the current average tick rate of the authoritative world: if server or standalone, then this equals currentAverageTickRate,
	 *  otherwise this is a replicated copy of the server's currentAverageTickRate. */
	UPROPERTY( Replicated )
	float currentAverageAuthorityTickRate;




	/* NetUpdateFrequency management */


public:

	/** Register an actor so as to have its NetUpdateFrequency automatically corrected on each tick, so as to take into account the simulation time vs. wall
	 ** clock time difference; UE does not take care of this in our case of using fixed time steps. No-op with a logged warning if the actor is already
	 ** registered. @see UnregisterManagedNetUpdateFrequency */
	UFUNCTION( BlueprintCallable, Category = "NetUpdateFrequency management" )
	void RegisterManagedNetUpdateFrequency( AActor * actor );

	/** Unregister an actor from receiving automatic NetUpdateFrequency updates. No-op with a logged warning if the actor has not been registered.
	 ** @see RegisterManagedNetUpdateFrequency */
	UFUNCTION( BlueprintCallable, Category = "NetUpdateFrequency management" )
	void UnregisterManagedNetUpdateFrequency( AActor * actor );


	/** Target effective real-time net update frequency for actors that have registered for automatic NetUpdateFrequency management.
	 *  This management system is needed because we usually want to define the net update frequency in terms of wall-clock time, but the nominal
	 *  NetUpdateFrequency value stored in actors does not always do this; when using fixed time steps, this nominal value defines the update frequency in terms
	 *  of game time instead of wall-clock time. @see RegisterManagedNetUpdateFrequency */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "NetUpdateFrequency management", meta = (ClampMin = 0.001f, UIMin = 1.f, UIMax = 70.f) )
	float RealtimeNetUpdateFrequency = 70.f;


private:

	/** Manage net update frequencies of the registered actors (@see NetUpdateFrequencyManagedActors). */
	void manageNetUpdateFrequencies( float gameDeltaTime );

	/** Actors registered for managed NetUpdateFrequency. @see RegisterManagedNetUpdateFrequency, UnregisterManagedNetUpdateFrequency */
	std::unordered_set<AActor *> netUpdateFrequencyManagedActors;

	/** The set of actors to be added to the netUpdateFrequencyManagedActors list on BeginPlay(). */
	UPROPERTY( EditAnywhere, Category = "NetUpdateFrequency management" )
	FActorSelector InitialNetUpdateFrequencyManagedActors;

};
