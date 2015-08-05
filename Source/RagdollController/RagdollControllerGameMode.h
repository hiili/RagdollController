// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/GameMode.h"
#include "RagdollControllerGameMode.generated.h"




/**
 * A custom GameMode class with the following functionality:
 *   - Manage level-wide remote commands (level snapshot and recall, mainly)
 *
 * Ticking order:
 * ARagdollControllerGameMode is currently designed to be ticked before ControlledRagdoll actors. This must be ensured in such actors by using the
 * AddTickPrerequisiteActor() method. Note that whether the actors are ticking in the right order is not currently checked anywhere!
 */
UCLASS()
class RAGDOLLCONTROLLER_API ARagdollControllerGameMode : public AGameMode
{
	GENERATED_BODY()

	int tmp{ 0 };

	FBufferArchive mArchive{ /*bIsPersistent =*/ false };
	FObjectAndNameAsStringProxyArchive mProxyArchive{ mArchive, /*bInLoadIfFindFails =*/ false };


	/** Handle commands from a remote controller. */
	void HandleRemoteCommands();


public:

	ARagdollControllerGameMode( const FObjectInitializer & ObjectInitializer );

	virtual void InitGame( const FString & MapName, const FString & Options, FString & ErrorMessage ) override;
	void Tick( float DeltaSeconds ) override;

};
