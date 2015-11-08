// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include "DeepSnapshotBase.h"

#include "DeepSnapshotManager.generated.h"




UCLASS()
class RAGDOLLCONTROLLER_API ADeepSnapshotManager : public AActor
{
	GENERATED_BODY()

	/** Permit deep snapshot components to have private access during initialization, so that they can register themselves with us. */
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
	 *  There is no unregister method at the moment; registration is currently for lifetime!
	 *  @param snapshotGroups	A list of snapshot group names for selecting the snapshot groups to which this component will be added. */
	void RegisterSnapshotComponent( UDeepSnapshotBase * component, const TArray<FName> & snapshotGroups );


	/** The set of registered snapshot components that should receive commands from this manager, keyed by their snapshot group name.
	 *
	 *  Usage pattern: Fill on game start (does not need to be fast). During gameplay on snapshot commands, iterate over complete element sets with the same key
	 *  (needs to be fast).
	 *  
	 *  Chosen container type: Split to TMap and TSet instead of using TMultiMap, because documentation on iteration complexity is lacking (some UE iterators
	 *  actually take full copies of the content during construction). */
	TMap< FName, TSet< TWeakObjectPtr<UDeepSnapshotBase> > > registeredSnapshotComponentsByGroup;

};
