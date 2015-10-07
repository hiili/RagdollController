// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "TickrateManager.h"


// Sets default values
ATickrateManager::ATickrateManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ATickrateManager::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATickrateManager::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

}

