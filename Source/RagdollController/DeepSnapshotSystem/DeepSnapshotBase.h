// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"

#include "TickrateManager.h"

#include <functional>

#include "DeepSnapshotBase.generated.h"




USTRUCT()
struct FSnapshotData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	TArray<uint8> Data;
};




/**
 * The abstract base class for storage components of the deep snapshot system.
 */
UCLASS( Abstract )
class RAGDOLLCONTROLLER_API UDeepSnapshotBase : public UActorComponent
{
	GENERATED_BODY()


public:

	// Sets default values for this component's properties
	UDeepSnapshotBase();


	/* UActorComponent interface overrides */

	// Called when the game starts
	virtual void InitializeComponent() override;

	// Called every frame
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;


	/* Deep snapshot system interface */

	/** Take and store a snapshot of the current state of the component pointed by TargetComponent.
	 ** @param	slotName	The name of the snapshot slot to be used */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void Snapshot( FName slotName );

	/** Apply a stored snapshot to the component pointed by TargetComponent. Return true on success, otherwise false.
	 ** @param	slotName	The name of the snapshot slot to be used
	 ** @param	success		Returns true on success, otherwise false */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void Recall( FName slotName, bool & success );

	/** Erase the snapshot labeled with 'name'. Return true on success, otherwise false.
	 ** @param	slotName	The name of the snapshot slot to be used
	 ** @param	success		Returns true on success, otherwise false */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void Erase( FName name, bool & success  );

	/** Erase all stored snapshots. */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void EraseAll();


	/* automation */

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "DeepSnapshotSystem" )
	/** If set to some positive value, then the network authority will take a snapshot automatically on every n-th frame, where n is the value of this field,
	 ** and all network clients will apply this snapshot whenever an update is received. If set to -1, then the snapshot frequency will be adjusted adaptively
	 ** based on the current tick rate and the current replication frequency of the owning actor. Note that you need to set the UseTickrateManager property
	 ** to true to use the adaptive mode! Set to 0 to disable. @see UseTickrateManager */
	int32 AutoReplicationFrequency = 0;


	/* target component selection */

	/**	The name of the component that should be selected as the snapshot target during initialization. If 'None', then the first component with a matching
	 **	type will be selected. For example, a SkeletalMeshComponentSnapshot component would select the first SkeletalMeshComponent (or a derived type) as the
	 **	target. */
	UPROPERTY( EditAnywhere, Category = "DeepSnapshotSystem" )
	FName InitialTargetComponentName;

	/** The current target component for taking and applying snapshots. This can be changed at runtime. */
	UPROPERTY( EditInstanceOnly, BlueprintReadWrite, Category = "DeepSnapshotSystem" )
	UActorComponent * TargetComponent;


protected:

	/* Core snapshot functionality */

	/** Serialize/deserialize the target to/from the provided archive. */
	virtual void SerializeTarget( FArchive & archive, UActorComponent & target ) PURE_VIRTUAL( UDeepSnapshotBase::SerializeTarget, return; );

	/** Whether to use (and instantiate if necessary) the singleton TickrateManager actor. This is necessary for adaptive replication frequency management,
	 ** which you can activate using the AutoReplicationFrequency property. This variable is read during BeginPlay; it has no effect later on.
	 ** @see AutoReplicationFrequency */
	UPROPERTY( EditAnywhere, Category = "DeepSnapshotSystem" )
	bool UseTickrateManager = false;

	/* target component selection */

	/** If InitialTargetComponentName is set, then look up the corresponding component and assign it to TargetComponent. Return true if the target was found. */
	bool SelectTargetComponentByName();

	/** Find the first component with a matching type and assign it to TargetComponent. Return true if a matching component was found.
	 ** @see InitialTargetComponentName */
	bool SelectTargetComponentByType();

	/** Test whether the type of a target candidate component matches the nominal target type of this snapshot storage component. */
	virtual bool IsAcceptableTargetType( UActorComponent * targetCandidate ) PURE_VIRTUAL( UDeepSnapshotBase::IsAcceptableTargetType, return false; );


private:

	/** Check that a target component exists, then serialize/deserialize the target to/from the provided archive. */
	void SerializeTargetIfNotNull( FArchive & archive );


	/** Find the specified snapshot slot from the 'Snapshots' field. Return null if not found. */
	FSnapshotData * FindSnapshotByName( FName name );

	/** Look for a component in the owning actor that matches the predicate 'pred'. If found, then assign it to TargetComponent and return true.
	 ** Otherwise return false. */
	bool SelectTargetComponentByPredicate( std::function<bool( UActorComponent * )> pred );

	/** Handle pose replication events. This is called by the UE replication system whenever an update for BoneStates is received. */
	UFUNCTION()
	void OnReplicationSnapshotUpdate();


	/** Phase counter for automatic replication. @see AutoReplicationFrequency */
	int AutoReplicationPhase = 0;


	/** Storage slots for serialized snapshot data. We do not use a TMap on the top level as it is not supported by the UE reflection system. */
	UPROPERTY()
	TArray<FSnapshotData> Snapshots;

	/** Special storage array for replication. This array is not affected by any snapshot bulk operations, nor is it serialized. */
	UPROPERTY( ReplicatedUsing = OnReplicationSnapshotUpdate )
	TArray<uint8> ReplicationData;

};
