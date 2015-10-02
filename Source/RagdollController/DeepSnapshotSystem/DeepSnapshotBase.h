// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"

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

	/** Take and store a snapshot of the current state of the component pointed by TargetComponent. */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	virtual void Snapshot() PURE_VIRTUAL( UDeepSnapshotBase::Snapshot, return; )

	/** Apply a stored snapshot back to the component pointed by TargetComponent. */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	virtual void Recall() PURE_VIRTUAL( UDeepSnapshotBase::Snapshot, return; );

	
	/* automation */

	/** Whether to automatically perform a snapshot or recall when the component is being serialized or deserialized, respectively. The automatic snapshot
	 ** will be stored with the name 'SerializationAutoSnapshot'. (Not yet implemented) */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "DeepSnapshotSystem,Automation" )
	bool AutoSnapshotOrRecallOnSerialize = false;

	/** If set to other than zero, then the network authority will take a snapshot automatically on every n-th frame, where n is the value of this field, and
	 ** all network clients will apply this snapshot whenever an update is received. The automatic snapshot is stored with the name 'ReplicationAutoSnapshot'.
	 ** (Not yet implemented) */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "DeepSnapshotSystem,Automation" )
	uint32 AutoReplicationFrequency = 0;


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

	/** If InitialTargetComponentName is set, then look up the corresponding component and assign it to TargetComponent. Return true if the target was found. */
	bool SelectTargetComponentByName();

	/** Find the first component with a matching type and assign it to TargetComponent. Return true if a matching component was found.
	 ** @see InitialTargetComponentName */
	bool SelectTargetComponentByType();

	/** Test whether the type of a target candidate component matches the nominal target type of this snapshot storage component. */
	virtual bool IsAcceptableTargetType( UActorComponent * targetCandidate ) PURE_VIRTUAL( UDeepSnapshotBase::IsAcceptableTargetType, return false; );


	/** Storage area for serialized snapshot data. The top-level map is for taking multiple snapshots that keyed by an FName. Each vector contains the actual
	 ** snapshot data associated with that specific key. We do not use UE types because maps are not supported by the serialization system. */
	UPROPERTY()
	TArray<FSnapshotData> Snapshots;


private:

	/** Look for a component in the owning actor that matches the predicate 'pred'. If found, then assign it to TargetComponent and return true.
	 ** Otherwise return false. */
	bool SelectTargetComponentByPredicate( std::function<bool( UActorComponent * )> pred );

};
