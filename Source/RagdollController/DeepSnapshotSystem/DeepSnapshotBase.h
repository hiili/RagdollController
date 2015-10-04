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

	///** Whether to automatically perform a snapshot or recall when the component is being serialized or deserialized, respectively. The automatic snapshot
	// ** will be stored with the name 'SerializationAutoSnapshot'. (Not yet implemented) */
	//UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "DeepSnapshotSystem" )
	//bool AutoSnapshotOrRecallOnSerialize = false;

	/** If set to other than zero, then the network authority will take a snapshot automatically on every n-th frame, where n is the value of this field, and
	 ** all network clients will apply this snapshot whenever an update is received. (Not yet implemented) */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "DeepSnapshotSystem" )
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


	///* serialization */

	// temporary test
	virtual void Serialize( FArchive & Ar ) override
	{ UE_LOG( LogTemp, Error, TEXT( "*** Serialization detected." ) ); Super::Serialize( Ar ); }

	// well, trying to intercept a serialization call might be error-prone, due to the spurious operator<< and Serialize() alternatives. skip for now.
	///** The serialization operator. Serializes the stored snapshots. Note that you need to use an FNameAsStringProxyArchive (or some derived one), so as to get
	// ** the snapshot names serialized.*/
	//friend FArchive & operator<<( FArchive & Ar, DeepSnapshotBase & obj );


protected:

	/* Core snapshot functionality */

	/** Serialize/deserialize the target to/from the provided archive. */
	virtual void SerializeTarget( FArchive & archive, UActorComponent & target ) PURE_VIRTUAL( UDeepSnapshotBase::Snapshot, return; )


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
	uint32 AutoReplicationPhase = 0;


	/** Storage slots for serialized snapshot data. We do not use a TMap on the top level as it is not supported by the UE reflection system. */
	UPROPERTY()
	TArray<FSnapshotData> Snapshots;

	/** Special storage array for replication. This array is not affected by any snapshot bulk operations, nor is it serialized. */
	UPROPERTY( ReplicatedUsing = OnReplicationSnapshotUpdate )
	TArray<uint8> ReplicationData;

};
