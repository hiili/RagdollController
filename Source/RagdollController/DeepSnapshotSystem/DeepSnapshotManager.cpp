// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "DeepSnapshotManager.h"


// Sets default values
ADeepSnapshotManager::ADeepSnapshotManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

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

