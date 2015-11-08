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

	UE_LOG( LogTemp, Error, TEXT( "************* Reg comp names num: %d, SNgr1 num: %d, SNgr2 num: %d, SNgr3 num: %d, SNgr10 num: %d" ),
		registeredSnapshotComponentsByGroup.Num(),
		registeredSnapshotComponentsByGroup[FName( "SNgr1" )].Num(), 
		registeredSnapshotComponentsByGroup[FName( "SNgr2" )].Num(),
		registeredSnapshotComponentsByGroup[FName( "SNgr3" )].Num(),
		registeredSnapshotComponentsByGroup[FName( "SNgr10" )].Num()
	);
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
	for( auto & groupName : snapshotGroups )
	{
		// does this group already exist?
		if( !registeredSnapshotComponentsByGroup.Contains( groupName ) )
		{
			// no: create an empty group
			registeredSnapshotComponentsByGroup.Emplace( groupName );
		}

		// add to the group list
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
