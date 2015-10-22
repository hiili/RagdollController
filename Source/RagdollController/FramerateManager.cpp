// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "FramerateManager.h"


// Sets default values
AFramerateManager::AFramerateManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AFramerateManager::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AFramerateManager::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

}

