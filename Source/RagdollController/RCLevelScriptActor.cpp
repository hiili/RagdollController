// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "RCLevelScriptActor.h"

#include <App.h>
#include <Net/UnrealNetwork.h>
#include <PhysicsPublic.h>

#include <PxPhysics.h>
#include <PxScene.h>
#include <extensions/PxVisualDebuggerExt.h>

#include <unordered_set>
#include <utility>
#include <algorithm>


// average tick rate logging: frame timestamp window size (must be >= 2)
#define ESTIMATE_TICKRATE_SAMPLES 10




void ARCLevelScriptActor::GetLifetimeReplicatedProps( TArray<FLifetimeProperty> & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( ARCLevelScriptActor, currentAverageAuthorityTickRate );
}




ARCLevelScriptActor::ARCLevelScriptActor( const FObjectInitializer & ObjectInitializer ) :
	Super( ObjectInitializer ),
	tickTimestamps( ESTIMATE_TICKRATE_SAMPLES )
{
}




void ARCLevelScriptActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Register self for automatic NetUpdateFrequency management
	RegisterManagedNetUpdateFrequency( this );
}




void ARCLevelScriptActor::BeginPlay()
{
	Super::BeginPlay();

	// set the fixed dt (remember to use the -UseFixedTimeStep command line option!)
	check( this->FixedFps != 0.f );
	FApp::SetFixedDeltaTime( 1.f / this->FixedFps );

	// connect to physics debugger (adapted from https://physx3.googlecode.com/svn/trunk/PhysX-3.2_PC_SDK_Core/Samples/SampleBase/PhysXSample.cpp)
	if( this->ConnectToPhysXVisualDebugger )
	{
		if( GetWorld() && GetWorld()->GetPhysicsScene() && GetWorld()->GetPhysicsScene()->GetPhysXScene( 0 ) )
		{
			physx::PxPhysics & physics = GetWorld()->GetPhysicsScene()->GetPhysXScene( 0 )->getPhysics();
			physx::PxVisualDebuggerConnection * theConnection = 0;
			if( physics.getPvdConnectionManager() && physics.getVisualDebugger() )
			{
				theConnection = physx::PxVisualDebuggerExt::createConnection( physics.getPvdConnectionManager(), "127.0.0.1", 5425, 10000,
					physx::PxVisualDebuggerExt::getAllConnectionFlags() );
				physics.getVisualDebugger()->setVisualDebuggerFlags( physx::PxVisualDebuggerFlag::eTRANSMIT_CONTACTS |
					physx::PxVisualDebuggerFlag::eTRANSMIT_SCENEQUERIES | physx::PxVisualDebuggerFlag::eTRANSMIT_CONSTRAINTS );
			}
			if( theConnection )
			{
				UE_LOG( LogRcSystem, Log, TEXT( "(%s) PhysX Visual Debugger connection initialized succesfully." ), TEXT( __FUNCTION__ ) );
			}
			else
			{
				UE_LOG( LogRcSystem, Error, TEXT( "(%s) PhysX Visual Debugger: Failed to initialize connection!" ), TEXT( __FUNCTION__ ) );
			}
		}
		else
		{
			UE_LOG( LogRcSystem, Error, TEXT( "(%s) PhysX Visual Debugger: Failed to initialize connection: Failed to access the PhysX scene!" ), TEXT( __FUNCTION__ ) );
		}
	}
}




void ARCLevelScriptActor::Tick( float deltaSeconds )
{
	Super::Tick( deltaSeconds );

	// handle remotely sent level commands
	HandleRemoteCommands();

	// If not dedicated server, or CapServerTickRate == true, then cap fps here. The -UseFixedTimeStep commannd line option disables built-in framerate
	// control in UEngine::UpdateTimeAndHandleMaxTickRate(), and we can't override that method as it is not virtual.
	UWorld * world = GetWorld();
	check( world );
	if( world->GetNetMode() != NM_DedicatedServer || this->CapServerTickRate )
	{
		HandleMaxTickRate( this->FixedFps );
	}

	// estimate the current average frame rate
	estimateAverageTickRate();

	// if predictive pose replication is enabled an we are not authority, then sync game speed with server
	if( this->PoseReplicationDoClientsidePrediction && !HasAuthority() )
	{
		syncGameSpeedWithServer();
	}

	// Adjust the net update frequencies of all registered actors
	manageNetUpdateFrequencies( deltaSeconds );
}




/**
 * The following implementation is mostly copied from UEngine::UpdateTimeAndHandleMaxTickRate().
 *
 * Note that UEngine::UpdateTimeAndHandleMaxTickRate()'s fixed dt implementation will cause FApp::CurrentTime to drift!
 * However, do not poke the engine in any way (do not try to imitate the dynamic dt code path in UpdateTimeAndHandleMaxTickRate).
 *
 * Do not use FApp::LastTime, as we do not know the execution order with UpdateTimeAndHandleMaxTickRate().
 */
void ARCLevelScriptActor::HandleMaxTickRate( const float MaxTickRate )
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




void ARCLevelScriptActor::estimateAverageTickRate()
{
	check( ESTIMATE_TICKRATE_SAMPLES >= 2 );

	// store a time stamp for the current tick
	this->tickTimestamps.push_back( FPlatformTime::Seconds() );

	// compute the running average for the last n - 1 deltas
	this->currentAverageTickRate = (ESTIMATE_TICKRATE_SAMPLES - 1) / (this->tickTimestamps.back() - this->tickTimestamps.front());

	// if authority, then copy this also to currentAverageAuthorityTickRate
	if( HasAuthority() )
	{
		this->currentAverageAuthorityTickRate = this->currentAverageTickRate;
	}
}




void ARCLevelScriptActor::syncGameSpeedWithServer()
{
	// no point in syncing auth's speed with itself
	if( HasAuthority() ) return;

	// compute the speed difference
	float serverSpeedMultiplier = this->currentAverageAuthorityTickRate / this->currentAverageTickRate;
	
	// sync using global time dilation
	AWorldSettings * ws = GetWorldSettings(); check( ws );
	if( ws )
	{
		ws->TimeDilation = serverSpeedMultiplier;
	}
	
	//// sync by modifying fixed dt
	//FApp::SetFixedDeltaTime( serverSpeedMultiplier * (1.f / this->FixedFps) );
}




void ARCLevelScriptActor::RegisterManagedNetUpdateFrequency( AActor * actor )
{
	// check if actor is null
	if( !actor )
	{
		UE_LOG( LogRcSystem, Warning, TEXT("(%s) The provided actor pointer is null! Ignoring."), TEXT(__FUNCTION__) );
		return;
	}

	// try to insert the actor to the set
	if( !this->NetUpdateFrequencyManagedActors.emplace( actor ).second )
	{
		// the actor was already in the set; log and ignore
		UE_LOG( LogRcSystem, Warning, TEXT("(%s) The provided actor (%s) is already registered! Ignoring."),
			TEXT(__FUNCTION__), *actor->GetHumanReadableName() );
	}
}


void ARCLevelScriptActor::UnregisterManagedNetUpdateFrequency( AActor * actor )
{
	// try to erase the actor from the set
	if( this->NetUpdateFrequencyManagedActors.erase( actor ) != 1 )
	{
		// actor not found, log an error
		UE_LOG( LogRcSystem, Warning, TEXT( "(%s) The provided actor (%s) is not registered! Ignoring." ),
			TEXT( __FUNCTION__ ), actor ? *actor->GetHumanReadableName() : TEXT( "(nullptr)" ) );
	}
}


void ARCLevelScriptActor::manageNetUpdateFrequencies( float gameDeltaTime )
{
	// compute how fast the simulation is running with respect to wall clock time
	float currentSpeedMultiplier = this->currentAverageTickRate / (1.f / gameDeltaTime);

	// compute the new net update frequency
	float netUpdateFrequency = this->RealtimeNetUpdateFrequency / currentSpeedMultiplier;

	// apply to managed actors
	for( auto actor : this->NetUpdateFrequencyManagedActors )
	{
		actor->NetUpdateFrequency = netUpdateFrequency;
	}
}




void ARCLevelScriptActor::HandleRemoteCommands()
{
	++tmp;

	if( tmp == 25 )
	{
		UE_LOG( LogTemp, Error, TEXT( "*********************************** SNAPSHOT ********************************************************" ) );
	}

	if( tmp % 100 == 0 )
	{
		UE_LOG( LogTemp, Error, TEXT( "*********************************** RESET ********************************************************" ) );
		//ALevelScriptActor::LevelReset();
	}
}
