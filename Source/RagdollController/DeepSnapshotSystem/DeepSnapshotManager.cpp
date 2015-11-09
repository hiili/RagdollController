// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "DeepSnapshotManager.h"

#include "Utility.h"




// Sets default values
ADeepSnapshotManager::ADeepSnapshotManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// create a dummy root component and a sprite for the editor
	Utility::AddDefaultRootComponent( this, "/Game/Assets/Gears128" );
}


// Called when the game starts or when spawned
void ADeepSnapshotManager::BeginPlay()
{
	Super::BeginPlay();
	
}


// Called every frame
void ADeepSnapshotManager::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

}




bool ADeepSnapshotManager::IsGroupNonEmpty( FName groupName )
{
	if( groupName.IsNone() ) return true;

	// find the group set
	TSet< TWeakObjectPtr<UDeepSnapshotBase> > * groupPtr = registeredSnapshotComponentsByGroup.Find( groupName );
	if( !groupPtr ) return false;

	// check that it contains at least one non-stale pointer
	for( TWeakObjectPtr<UDeepSnapshotBase> & componentWeakPtr : *groupPtr )
	{
		if( componentWeakPtr.IsValid() ) return true;
	}

	// no non-stale pointers: take the opportunity to prune fast, then return false
	groupPtr->Empty();
	return false;
}




void ADeepSnapshotManager::Snapshot( FName groupName, FName slotName, bool & success )
{
	success = ForEachInGroup( groupName, [slotName]( UDeepSnapshotBase & component ){
		component.Snapshot( slotName );
	} );
}




void ADeepSnapshotManager::Recall( FName groupName, FName slotName, bool & success )
{
	bool foreachSuccess = true;
	success = ForEachInGroup( groupName, [slotName, &foreachSuccess]( UDeepSnapshotBase & component ){
		bool thisSuccess;
		component.Recall( slotName, thisSuccess );
		foreachSuccess &= thisSuccess;
	} );

	success &= foreachSuccess;
}


void ADeepSnapshotManager::Erase( FName groupName, FName slotName, bool & success )
{
	bool foreachSuccess = true;
	success = ForEachInGroup( groupName, [slotName, &foreachSuccess]( UDeepSnapshotBase & component ){
		bool thisSuccess;
		component.Erase( slotName, thisSuccess );
		foreachSuccess &= thisSuccess;
	} );

	success &= foreachSuccess;
}


void ADeepSnapshotManager::EraseAll( FName groupName, bool & success )
{
	success = ForEachInGroup( groupName, []( UDeepSnapshotBase & component ){
		component.EraseAll();
	} );
}




void ADeepSnapshotManager::RegisterSnapshotComponent( UDeepSnapshotBase * component, const TArray<FName> & snapshotGroups )
{
	// null pointer?
	if( !component )
	{
		// log and return
		UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) The provided component pointer is null!" ), TEXT( __FUNCTION__ ) );
		return;
	}

	// register for all specified snapshot groups
	for( auto groupName : snapshotGroups )
	{
		// does this group already exist?
		if( !registeredSnapshotComponentsByGroup.Contains( groupName ) )
		{
			// no: create an empty group
			registeredSnapshotComponentsByGroup.Emplace( groupName );
		}

		// add the component to the current group
		bool isAlreadyInSet;
		registeredSnapshotComponentsByGroup[groupName].Emplace( component, &isAlreadyInSet );

		// multiple registration?
		if( isAlreadyInSet )
		{
			// log
			UE_LOG( LogDeepSnapshotSystem, Error,
				TEXT( "(%s) A deep snapshot component is trying to register multiple times! Component: name=%s, owner=%s. Snapshot group name: %s" ),
				TEXT( __FUNCTION__ ),
				*component->GetName(), component->GetOwner() ? *component->GetOwner()->GetName() : TEXT("(no owner)"),
				*groupName.ToString() );
		}
	}
}
