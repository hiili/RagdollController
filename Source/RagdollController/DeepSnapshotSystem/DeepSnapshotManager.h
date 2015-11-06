// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include "DeepSnapshotBase.h"

#include "DeepSnapshotManager.generated.h"

UCLASS()
class RAGDOLLCONTROLLER_API ADeepSnapshotManager : public AActor
{
	GENERATED_BODY()

	/** We permit deep snapshot components private access during initialization so that they can register themselves with us. */
	friend void UDeepSnapshotBase::InitializeComponent();
public:	
	// Sets default values for this actor's properties
	ADeepSnapshotManager();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

	
	

	/* deep snapshot component management */

private:

	/** Registers a deep snapshot component. Deep snapshot components consider registering themselves during the InitializeComponent() stage.
	 *  There is no unregister method at the moment; registration is currently for lifetime! */
	void RegisterSnapshotComponent( UDeepSnapshotBase * component );


	/** The set of registered snapshot components that should receive commands from this manager. */
	TSet< TWeakObjectPtr<UDeepSnapshotBase> > RegisteredSnapshotComponents;

};
