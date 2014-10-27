// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController45.h"
#include "RagdollController45GameMode.h"

#include <App.h>


// constant frame rate
#define FIXED_FPS 60.f

// framerate logging: averaging window size (set to 0 to disable logging)
#define ESTIMATE_FRAMERATE_SAMPLES 100


ARagdollController45GameMode::ARagdollController45GameMode(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// set the fixed dt (remember to use the -UseFixedTimeStep command line option!)
	FApp::SetFixedDeltaTime( 1.f / FIXED_FPS );
}




void ARagdollController45GameMode::Tick( float DeltaSeconds )
{
	Super::Tick( DeltaSeconds );

	// cap fps here, as the -UseFixedTimeStep commannd line option disables built-in framerate control in UEngine::UpdateTimeAndHandleMaxTickRate(),
	// and we can't override that method as it is not virtual.
	//HandleMaxTickRate( FIXED_FPS );

	// estimate the current average frame rate
	estimateAverageFrameRate();
}




/**
 * The following implementation is mostly copied from UEngine::UpdateTimeAndHandleMaxTickRate().
 *
 * Note that UEngine::UpdateTimeAndHandleMaxTickRate()'s fixed dt implementation will cause FApp::CurrentTime to drift!
 * However, do not poke the engine in any way (do not try to imitate the dynamic dt code path in UpdateTimeAndHandleMaxTickRate), except for stats macros.
 *
 * Do not use FApp::LastTime, as we do not know the execution order with UpdateTimeAndHandleMaxTickRate().
 */
void ARagdollController45GameMode::HandleMaxTickRate( const float MaxTickRate )
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
	STAT(double ActualWaitTime = 0.f);
	if (WaitTime > 0)
	{
		double WaitEndTime = CurrentTime + WaitTime;
		SCOPE_SECONDS_COUNTER(ActualWaitTime);
		SCOPE_CYCLE_COUNTER(STAT_GameTickWaitTime);
		SCOPE_CYCLE_COUNTER(STAT_GameIdleTime);
		
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


	SET_FLOAT_STAT(STAT_GameTickWantedWaitTime, WaitTime * 1000.f);
	SET_FLOAT_STAT(STAT_GameTickAdditionalWaitTime, FMath::Max<float>((ActualWaitTime - WaitTime)*1000.f, 0.f));


	// update LastTime
	LastTime = CurrentTime + WaitTime;
}




void ARagdollController45GameMode::estimateAverageFrameRate()
{
	// fps estimation disabled?
	if( ESTIMATE_FRAMERATE_SAMPLES == 0 ) return;

	static double lastTime = FPlatformTime::Seconds();
	static int ticksLeft = ESTIMATE_FRAMERATE_SAMPLES;

	--ticksLeft;
	if( ticksLeft == 0 ) {

		double currentTime = FPlatformTime::Seconds();
		this->currentAverageFps = ESTIMATE_FRAMERATE_SAMPLES / (currentTime - lastTime);

		UE_LOG( LogTemp, Log, TEXT( "Current average frame rate: %g" ), this->currentAverageFps );

		lastTime = currentTime;
		ticksLeft = ESTIMATE_FRAMERATE_SAMPLES;

	}
}
