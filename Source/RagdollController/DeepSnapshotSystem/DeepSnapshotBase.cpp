// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"

#include <Net/UnrealNetwork.h>

#include "DeepSnapshotBase.h"




void UDeepSnapshotBase::GetLifetimeReplicatedProps( TArray<FLifetimeProperty> & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( UDeepSnapshotBase, ReplicationData );
}


void UDeepSnapshotBase::OnReplicationSnapshotUpdate()
{
	// create an archive reader for the incoming data and perform a recall from it
	FMemoryReader reader( ReplicationData );
	SerializeTargetIfNotNull( reader );
}




// Sets default values for this component's properties
UDeepSnapshotBase::UDeepSnapshotBase()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UDeepSnapshotBase::InitializeComponent()
{
	Super::InitializeComponent();

	// try to select a suitable target component
	if( InitialTargetComponentName.IsNone() )
	{
		SelectTargetComponentByType();
	}
	else
	{
		SelectTargetComponentByName();
	}

	UE_LOG( LogTemp, Error, TEXT("target ptr: %p, target name: %s"), TargetComponent, TargetComponent ? *TargetComponent->GetName() : TEXT("") );
}


// Called every frame
void UDeepSnapshotBase::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	// perform automatic replication if automatic replication is enabled and we have authority
	check( GetOwner() );
	if( GetOwner()->HasAuthority() && AutoReplicationFrequency > 0 )
	{
		// is it time to replicate?
		if( AutoReplicationPhase == 0 )
		{
			// erase existing replication data (keep slack for the new data)
			ReplicationData.SetNum( 0, /*bAllowShrinking =*/ false );

			// create an archive writer and perform a snapshot to it
			FMemoryWriter writer( ReplicationData );
			SerializeTargetIfNotNull( writer );
		}

		// increment the phase counter
		AutoReplicationPhase = (AutoReplicationPhase + 1) % AutoReplicationFrequency;
	}
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


void UDeepSnapshotBase::Erase( FName name, bool & success )
{
	int32 numBefore = Snapshots.Num();

	// try to erase matching slots
	Snapshots.RemoveAllSwap( [name]( FSnapshotData & candidate ){
		return candidate.Name == name;
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




//FArchive & operator<<( FArchive & Ar, DeepSnapshotBase & obj )
//{
//	Ar << &obj;
//}
