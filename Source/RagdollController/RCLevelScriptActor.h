// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/LevelScriptActor.h"
#include "RCLevelScriptActor.generated.h"




/**
 * A custom LevelScriptActor class with the following functionality:
 *   - Manage connecting with PhysX Visual Debugger
 */
UCLASS( Config = RagdollController )
class RAGDOLLCONTROLLER_API ARCLevelScriptActor : public ALevelScriptActor
{
	GENERATED_BODY()




public:

	/* .ini configuration */

	/** Whether to attempt to connect to PhysX Visual Debugger (slows down startup for a second or two if PVD not found) */
	UPROPERTY( Config )
	bool ConnectToPhysXVisualDebugger = false;




	ARCLevelScriptActor( const FObjectInitializer & ObjectInitializer );


	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	/** See the class documentation for a summary of Tick operations. */
	virtual void Tick( float deltaSeconds ) override;


};
