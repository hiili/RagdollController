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




void ADeepSnapshotManager::RegisterSnapshotComponent( UDeepSnapshotBase * component )
{
	if( !component )
	{
		// null pointer: log and return
		UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) The provided component pointer is null!" ), TEXT( __FUNCTION__ ) );
		return;
	}

	bool isAlreadyInSet;
	RegisteredSnapshotComponents.Emplace( component, &isAlreadyInSet );

	if( isAlreadyInSet )
	{
		// multiple registration: log
		UE_LOG( LogDeepSnapshotSystem, Error, TEXT( "(%s) A deep snapshot component is trying to register multiple times! Component name: %s" ),
			TEXT( __FUNCTION__ ), *component->GetName() );
	}
}
