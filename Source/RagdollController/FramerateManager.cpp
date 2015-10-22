// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "FramerateManager.h"

#include <App.h>
#include <Net/UnrealNetwork.h>

#include <unordered_set>
#include <utility>
#include <algorithm>


// average tick rate logging: frame timestamp window size (must be >= 2)
#define ESTIMATE_TICKRATE_SAMPLES 10


DECLARE_LOG_CATEGORY_EXTERN( LogFramerateManager, Log, All );
DEFINE_LOG_CATEGORY( LogFramerateManager );




void AFramerateManager::GetLifetimeReplicatedProps( TArray<FLifetimeProperty> & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( AFramerateManager, currentAverageAuthorityTickRate );
}




// Sets default values
AFramerateManager::AFramerateManager() :
	tickTimestamps( ESTIMATE_TICKRATE_SAMPLES )
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// create a dummy root component and a sprite for the editor
	Utility::AddDefaultRootComponent( this, "/Game/Assets/Gears128" );

	// we need replication for syncing the server's estimated frame rate to clients
	bReplicates = true;

	// Register self for automatic NetUpdateFrequency management
	RegisterManagedNetUpdateFrequency( this );
}


// Called when the game starts or when spawned
void AFramerateManager::BeginPlay()
{
	Super::BeginPlay();
	
	// apply the fixed dt (remember to use the -UseFixedTimeStep command line option!)
	SetFixedFps( GetFixedFps() );
}


// Called every frame
void AFramerateManager::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	// If not dedicated server, or CapServerFps == true, then cap fps here. The -UseFixedTimeStep commannd line option disables built-in framerate
	// control in UEngine::UpdateTimeAndHandleMaxTickRate(), and we can't override that method as it is not virtual.
	UWorld * world = GetWorld();
	check( world );
	if( world->GetNetMode() != NM_DedicatedServer || CapServerFps )
	{
		handleMaxTickRate( FixedFps );
	}

	// estimate the current average frame rate
	estimateAverageTickRate();

	// If we are not authority and SyncGameSpeedWithServer is true, then sync game speed with server
	if( !HasAuthority() && SyncGameSpeedWithServer )
	{
		performSyncGameSpeedWithServer();
	}

	// Adjust the net update frequencies of all registered actors
	manageNetUpdateFrequencies( DeltaTime );
}




void AFramerateManager::SetFixedFps( float newFixedFps )
{
	FixedFps = newFixedFps;

	check( FixedFps > 0.f );
	FApp::SetFixedDeltaTime( 1.f / FixedFps );
}




/**
 * The following implementation is mostly copied from UEngine::UpdateTimeAndHandleMaxTickRate().
 *
 * Note that UEngine::UpdateTimeAndHandleMaxTickRate()'s fixed dt implementation will cause FApp::CurrentTime to drift!
 * However, do not poke the engine in any way (do not try to imitate the dynamic dt code path in UpdateTimeAndHandleMaxTickRate).
 *
 * Do not use FApp::LastTime, as we do not know the execution order with UpdateTimeAndHandleMaxTickRate().
 */
void AFramerateManager::handleMaxTickRate( const float MaxTickRate )
{
	// Figure out whether we want to use real or fixed time step.
	const bool bUseFixedTimeStep = FApp::IsBenchmarking() || FApp::UseFixedTimeStep();

	// Only continue if UEngine::UpdateTimeAndHandleMaxTickRate() followed the code path with no fps control (in UE 4.4)
	if( !bUseFixedTimeStep ) return;


	// store current time
	double CurrentTime = FPlatformTime::Seconds();

	// timestamp from previous call, for computing the real dt
	static double LastTime = 0.0;

	// first call? real dt not known -> store time and return
	if( LastTime == 0.0 )
	{
		LastTime = CurrentTime;
		return;
	}

	// compute real dt up to now
	float DeltaTime = CurrentTime - LastTime;


	float WaitTime = 0;
	// Convert from max FPS to wait time.
	if (MaxTickRate > 0)
	{
		WaitTime = FMath::Max(1.f / MaxTickRate - DeltaTime, 0.f);
	}

	// Enforce maximum framerate and smooth framerate by waiting.
	//STAT(double ActualWaitTime = 0.f);
	if (WaitTime > 0)
	{
		double WaitEndTime = CurrentTime + WaitTime;
		//SCOPE_SECONDS_COUNTER(ActualWaitTime);
		//SCOPE_CYCLE_COUNTER(STAT_GameTickWaitTime);
		//SCOPE_CYCLE_COUNTER(STAT_GameIdleTime);
		
		// if (IsRunningDedicatedServer()) // We aren't so concerned about wall time with a server, lots of CPU is wasted spinning. I suspect there is more to do with sleeping and time on dedicated servers.
		if( false ) // use the precise approach also on dedicated servers
		{
			FPlatformProcess::Sleep(WaitTime);
		}
		else
		{
			// Sleep if we're waiting more than 5 ms. We set the scheduler granularity to 1 ms
			// at startup on PC. We reserve 2 ms of slack time which we will wait for by giving
			// up our timeslice.
			if (WaitTime > 5 / 1000.f)
			{
				FPlatformProcess::Sleep(WaitTime - 0.002f);
			}

			// Give up timeslice for remainder of wait time.
			while (FPlatformTime::Seconds() < WaitEndTime)
			{
				FPlatformProcess::Sleep(0);
			}
		}
		//FApp::SetCurrentTime(FPlatformTime::Seconds());
	}


	//SET_FLOAT_STAT(STAT_GameTickWantedWaitTime, WaitTime * 1000.f);
	//SET_FLOAT_STAT(STAT_GameTickAdditionalWaitTime, FMath::Max<float>((ActualWaitTime - WaitTime)*1000.f, 0.f));


	// update LastTime
	LastTime = CurrentTime + WaitTime;
}




void AFramerateManager::estimateAverageTickRate()
{
	check( ESTIMATE_TICKRATE_SAMPLES >= 2 );

	// store a time stamp for the current tick
	tickTimestamps.push_back( FPlatformTime::Seconds() );

	// compute the running average for the last n - 1 deltas
	currentAverageTickRate = (ESTIMATE_TICKRATE_SAMPLES - 1) / (tickTimestamps.back() - tickTimestamps.front());

	// if authority, then copy this also to currentAverageAuthorityTickRate
	if( HasAuthority() )
	{
		currentAverageAuthorityTickRate = currentAverageTickRate;
	}
}




void AFramerateManager::performSyncGameSpeedWithServer()
{
	// no point in syncing speed with self
	if( HasAuthority() ) return;

	// compute the speed difference
	float serverSpeedMultiplier = currentAverageAuthorityTickRate / currentAverageTickRate;
	
	// sync using global time dilation
	AWorldSettings * ws = GetWorldSettings(); check( ws );
	if( ws )
	{
		ws->TimeDilation = serverSpeedMultiplier;
	}
	
	//// sync by modifying fixed dt ... note that this might interfere with other things
	//FApp::SetFixedDeltaTime( serverSpeedMultiplier * (1.f / FixedFps) );
}




void AFramerateManager::RegisterManagedNetUpdateFrequency( AActor * actor )
{
	// check if actor is null
	if( !actor )
	{
		UE_LOG( LogFramerateManager, Warning, TEXT("(%s) The provided actor pointer is null! Ignoring."), TEXT(__FUNCTION__) );
		return;
	}

	// try to insert the actor to the set
	if( !netUpdateFrequencyManagedActors.emplace( actor ).second )
	{
		// the actor was already in the set; log and ignore
		UE_LOG( LogFramerateManager, Warning, TEXT("(%s) The provided actor (%s) is already registered! Ignoring."),
			TEXT(__FUNCTION__), *actor->GetHumanReadableName() );
	}
}


void AFramerateManager::UnregisterManagedNetUpdateFrequency( AActor * actor )
{
	// try to erase the actor from the set
	if( netUpdateFrequencyManagedActors.erase( actor ) != 1 )
	{
		// actor not found, log an error
		UE_LOG( LogFramerateManager, Warning, TEXT( "(%s) The provided actor (%s) is not registered! Ignoring." ),
			TEXT( __FUNCTION__ ), actor ? *actor->GetHumanReadableName() : TEXT( "(nullptr)" ) );
	}
}


void AFramerateManager::manageNetUpdateFrequencies( float gameDeltaTime )
{
	// compute how fast the simulation is running with respect to wall clock time
	float currentSpeedMultiplier = currentAverageTickRate / (1.f / gameDeltaTime);

	// compute the new net update frequency
	float netUpdateFrequency = RealtimeNetUpdateFrequency / currentSpeedMultiplier;

	// apply to managed actors
	for( auto actor : netUpdateFrequencyManagedActors )
	{
		actor->NetUpdateFrequency = netUpdateFrequency;
	}
}
