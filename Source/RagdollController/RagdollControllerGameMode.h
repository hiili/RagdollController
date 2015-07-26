// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/GameMode.h"
#include "RagdollControllerGameMode.generated.h"




/**
 * 
 */
UCLASS()
class RAGDOLLCONTROLLER_API ARagdollControllerGameMode : public AGameMode
{
	GENERATED_BODY()

	int tmp{ 0 };


public:

	ARagdollControllerGameMode( const FObjectInitializer & ObjectInitializer );

};
