// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"

#include "FramerateManager.h"
#include "DeepSnapshotManager.h"

#include <Net/UnrealNetwork.h>

#include "DeepSnapshotBase.h"


DEFINE_LOG_CATEGORY( LogDeepSnapshotSystem );




void UDeepSnapshotBase::GetLifetimeReplicatedProps( TArray<FLifetimeProperty> & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( UDeepSnapshotBase, ReplicationSnapshot );
}




// Sets default values for this component's properties
UDeepSnapshotBase::UDeepSnapshotBase()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UDeepSnapshotBase::InitializeComponent()
{
	Super::InitializeComponent();

	// try to select a target component
	if( AutoSelectTarget )
	{
		if( !SelectTargetComponentByType() )
		{
			UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) Automatic target component selection was requested but no matching component was found!" ),
				TEXT( __FUNCTION__ ) );
			UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "    (%s)" ), *LogCreateDiagnosticLine() );
		}
	}
	else if( !InitialTargetComponentName.IsNone() )
	{
		if( !SelectTargetComponentByName() )
		{
			UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) InitialTargetComponentName = %s, but no such component was found!" ),
				TEXT( __FUNCTION__ ) );
			UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "    (%s)" ), *LogCreateDiagnosticLine() );
		}
	}

	// register with a manager, if one exists
	TArray<AActor *> managers;
	UGameplayStatics::GetAllActorsOfClass( this, ADeepSnapshotManager::StaticClass(), managers );
	if( managers.Num() > 0 )
	{
		// manager(s) exist. make sure that there is only one.
		if( managers.Num() >= 2 )
		{
			// several managers exist -> log an error
			UE_LOG( LogDeepSnapshotSystem, Error,
				TEXT( "(%s) There should exist at most one DeepSnapshotManager actor in the world! Number of found DeepSnapshotManager actors: %d" ),
				TEXT( __FUNCTION__ ), managers.Num() );
			UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "    (%s)" ), *LogCreateDiagnosticLine() );
		}
		else
		{
			// exactly one exists; register with that
			check( managers.Num() == 1 );
			ADeepSnapshotManager * manager = dynamic_cast<ADeepSnapshotManager *>(managers[0]);
			check( manager );   // the cast should always succeed, because we searched only actors of this class
			manager->RegisterSnapshotComponent( this, SnapshotGroups );
		}
	}

	UE_LOG( LogDeepSnapshotSystem, Log, TEXT( "(%s) Initialization finished. %s" ), TEXT( __FUNCTION__ ), *LogCreateDiagnosticLine() );
}


// Called every frame
void UDeepSnapshotBase::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	// automatic replication on authority
	ConsiderTakingAutomaticReplicationSnapshot();

	// if network client && HardSync enabled && we have a snapshot -> apply it on each frame
	check( GetOwner() );
	if( !GetOwner()->HasAuthority() && AutomaticReplication.HardSync && ReplicationSnapshot.Data.Num() > 0 )
	{
		ApplyReplicatedSnapshot();
	}
}




bool UDeepSnapshotBase::CanEditChange( const UProperty* InProperty ) const
{
	bool result = Super::CanEditChange(InProperty);

	// should use GET_MEMBER_NAME_CHECKED()
	static const FName nameAutomaticReplication( "AutomaticReplication" );
	static const FName nameFrameSkipMultiplier( "FrameSkipMultiplier" );
	static const FName nameTargetFrequency( "TargetFrequency" );
	static const FName nameHardSync( "HardSync" );

	// FrameSkipMultiplier
	if( (InProperty->GetOuter() && InProperty->GetOuter()->GetFName() == nameAutomaticReplication) &&
		InProperty->GetFName() == nameFrameSkipMultiplier )
	{
		result &= AutomaticReplication.ReplicationMode == EAutomaticReplicationMode::EveryNthFrame;
	}
	// TargetFrequency
	else if( (InProperty->GetOuter() && InProperty->GetOuter()->GetFName() == nameAutomaticReplication) &&
		InProperty->GetFName() == nameTargetFrequency )
	{
		result &=
			AutomaticReplication.ReplicationMode == EAutomaticReplicationMode::ConstantGameTimeFrequency ||
			AutomaticReplication.ReplicationMode == EAutomaticReplicationMode::ConstantWallTimeFrequency;
	}
	// HardSync
	else if( (InProperty->GetOuter() && InProperty->GetOuter()->GetFName() == nameAutomaticReplication) &&
		InProperty->GetFName() == nameHardSync )
	{
		result &= AutomaticReplication.ReplicationMode != EAutomaticReplicationMode::Disabled;
	}

	return result;
}




void UDeepSnapshotBase::Snapshot( FName slotName )
{
	// try to find an already existing storage slot that has the provided name
	FSnapshotData * slot = FindSnapshotByName( slotName );

	// existing slot found?
	if( slot )
	{
		// yes, erase existing data from the slot (keep slack for the new data)
		slot->Data.SetNum( 0, /*bAllowShrinking =*/ false );
	}
	else
	{
		// no, create and add a new, empty one
		slot = &Snapshots[Snapshots.Add( { slotName } )];
	}

	// create a serializer and take the actual snapshot
	FMemoryWriter writer( slot->Data );
	SerializeTargetIfNotNull( writer );
}


void UDeepSnapshotBase::Recall( FName slotName, bool & success )
{
	// try to find the storage slot that has the provided name
	FSnapshotData * slot = FindSnapshotByName( slotName );
	if( !slot )
	{
		success = false;
		return;
	}

	// create a serializer and perform the actual recall
	FMemoryReader reader( slot->Data );
	SerializeTargetIfNotNull( reader );

	success = true;
	return;
}


void UDeepSnapshotBase::Erase( FName slotName, bool & success )
{
	int32 numBefore = Snapshots.Num();

	// try to erase matching slots
	Snapshots.RemoveAllSwap( [slotName]( FSnapshotData & candidate ){
		return candidate.Name == slotName;
	} );

	// return true if something was erased
	success = numBefore != Snapshots.Num();
	return;
}


void UDeepSnapshotBase::EraseAll()
{
	Snapshots.Empty();
}


FSnapshotData * UDeepSnapshotBase::FindSnapshotByName( FName name )
{
	return Snapshots.FindByPredicate( [name]( FSnapshotData & candidate ){
		return candidate.Name == name;
	} );
}




void UDeepSnapshotBase::SerializeTargetIfNotNull( FArchive & archive )
{
	if( TargetComponent ) SerializeTarget( archive, *TargetComponent );
}




bool UDeepSnapshotBase::SelectTargetComponentByName()
{
	// no-op if name is not set
	if( InitialTargetComponentName.IsNone() ) return false;

	// try to find the component with a matching name from the owning actor
	return SelectTargetComponentByPredicate( [this]( UActorComponent * candidate ){
		return candidate && candidate->GetFName() == InitialTargetComponentName;
	} );
}


bool UDeepSnapshotBase::SelectTargetComponentByType()
{
	// try to find a type-matching component from the owning actor
	return SelectTargetComponentByPredicate( [this]( UActorComponent * candidate ){
		return candidate && IsAcceptableTargetType( candidate );
	} );
}


bool UDeepSnapshotBase::SelectTargetComponentByPredicate( std::function<bool( UActorComponent * )> pred )
{
	// try to find an accepted component from the owning actor
	check( GetOwner() );
	UActorComponent * const * result = GetOwner()->GetComponents().FindByPredicate( pred );

	// return false if not found (double-check that the found component pointer is not null)
	if( !result || !*result ) return false;

	// target found; assign to TargetComponent and return true
	TargetComponent = *result;
	return true;
}




void UDeepSnapshotBase::ConsiderTakingAutomaticReplicationSnapshot()
{
	// no-op if not authority
	check( GetOwner() );
	if( !GetOwner()->HasAuthority() ) return;

	// time to replicate?
	switch( AutomaticReplication.ReplicationMode )
	{
	case EAutomaticReplicationMode::EveryNthFrame:
	
		// replicate if the frame skip counter has wrapped
		if( AutomaticReplication.FrameSkipPhase == 0 ) Replicate();

		// increment the phase counter
		AutomaticReplication.FrameSkipPhase = (AutomaticReplication.FrameSkipPhase + 1) % AutomaticReplication.FrameSkipMultiplier;

		return;

	case EAutomaticReplicationMode::ConstantGameTimeFrequency:
	case EAutomaticReplicationMode::ConstantWallTimeFrequency:
	{
		// get current time, either game time or wall clock time
		// (we use FPlatformTime::Seconds() in place of UWorld::GetRealTimeSeconds() because the latter does not return the _real_ real-time value when
		// running with a fixed step size)
		check( GetWorld() );
		double currentTime = AutomaticReplication.ReplicationMode == EAutomaticReplicationMode::ConstantGameTimeFrequency ?
			GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();

		// sanity check on the last replication timestamp (might be needed if mode has been switched run-time)
		if( currentTime < AutomaticReplication.lastReplicationTime ) AutomaticReplication.lastReplicationTime = 0.0;

		// early return if enough time has not yet passed
		if( currentTime - AutomaticReplication.lastReplicationTime < 1.f / AutomaticReplication.TargetFrequency ) return;

		// replicate and update the last replication timestamp
		Replicate();
		AutomaticReplication.lastReplicationTime = currentTime;

		return;
	}
	case EAutomaticReplicationMode::Disabled:
		return;
	default:
		return;
	}
}




void UDeepSnapshotBase::Replicate()
{
	// no-op if not authority
	check( GetOwner() );
	if( !GetOwner()->HasAuthority() ) return;

	// erase existing replication data (keep slack for the new data)
	ReplicationSnapshot.Data.SetNum( 0, /*bAllowShrinking =*/ false );

	// create an archive writer and perform a snapshot to it
	FMemoryWriter writer( ReplicationSnapshot.Data );
	//writer.ArIsPersistent = true;   // permit replication between systems with different endianness, as long as the derived class supports it
	// ^ cannot enable until this UE bug is fixed: https://answers.unrealengine.com/questions/333914/serialization-to-archive-can-corrupt-source-data.html
	SerializeTargetIfNotNull( writer );
}


void UDeepSnapshotBase::ApplyReplicatedSnapshot()
{
	// create an archive reader for the incoming data and perform a recall from it
	FMemoryReader reader( ReplicationSnapshot.Data );
	//reader.ArIsPersistent = true;   // permit replication between systems with different endianness, as long as the derived class supports it
	// ^ cannot enable until this UE bug is fixed: https://answers.unrealengine.com/questions/333914/serialization-to-archive-can-corrupt-source-data.html
	SerializeTargetIfNotNull( reader );
}


void UDeepSnapshotBase::OnReplicationSnapshotUpdate()
{
	// if HardSync is enabled, then this is called from TickComponent()
	if( !AutomaticReplication.HardSync ) ApplyReplicatedSnapshot();
}




void UDeepSnapshotBase::LogFailedDowncast( const char * functionName ) const
{
	UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) Downcast failed: target component is of wrong type!" ), *FString(functionName) );
	UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "    (%s)" ), *LogCreateDiagnosticLine() );
}


FString UDeepSnapshotBase::LogCreateDiagnosticLine() const
{
	return FString::Printf( TEXT( "snapshot component: %s; target component: %s" ),
		*GetPathName( GetWorld() ),
		TargetComponent ? *TargetComponent->GetPathName( TargetComponent->GetWorld() ) : *FString( "(no target)" ) );
}
