// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "OutputDevice.h"

#include "FramerateManager.h"

#include <functional>

#include "DeepSnapshotBase.generated.h"


DECLARE_LOG_CATEGORY_EXTERN( LogDeepSnapshotSystem, Log, All );




USTRUCT()
struct FSnapshotData
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	TArray<uint8> Data;
};




UENUM()
enum class EAutomaticReplicationMode : uint8
{
	/** No automatic replication. */
	Disabled,

	/** Replicate every n-th frame. The frame skip multiplier n can be set using the FrameSkipMultiplier field. */
	EveryNthFrame,

	/** Replicate with a constant game time frequency irrespective of the current frame rate. The target frequency can be set using the
	 * TargetFrequency field. */
	ConstantGameTimeFrequency,

	/** Replicate with a constant wall time frequency irrespective of the current frame rate or game speed. The target frequency can be set using the
	 * TargetFrequency field. */
	ConstantWallTimeFrequency
};


USTRUCT()
struct FAutomaticReplication
{
	GENERATED_BODY()

	/** If enabled, then the network authority will automatically take a private, replicated snapshot on a certain schedule.
	 *  All network clients will apply this snapshot automatically as soon as it has been transmitted over the network.
	 *  Note that the effective replication frequency depends also on the current NetUpdateFrequency value of the owning actor. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite )
	EAutomaticReplicationMode ReplicationMode = EAutomaticReplicationMode::Disabled;

	/** If ReplicationMode == EveryNthFrame, then this field defines how often a replication snapshot is to be taken. For example, setting this to 3 will
	 *  cause a replication snapshot to be taken on every third frame. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", UIMin = "1", UIMax = "60") )
	int32 FrameSkipMultiplier = 1;

	/** If ReplicationMode == ConstantGameTimeFrequency or ConstantWallTimeFrequency, then this field defines the target frequency (snapshots/second) for
	 *  taking replication snapshots. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", UIMin = "1.0", UIMax = "80.0") )
	float TargetFrequency = 80.f;

	/** If enabled, then network clients will apply the last received snapshot on each tick, effectively locking the state of the target component to that
	 *  snapshot. If disabled, then the replication snapshot is applied to the target component only whenever a new snapshot has been received. This can permit
	 *  client-side prediction and related approaches in case that the replication frequency is lower than the frame rate.
	 *  Note that client-side prediction with a non-realtime server might require adjustments of the max physics (sub)step size! */
	UPROPERTY( EditAnywhere, BlueprintReadWrite )
	bool HardSync = false;


	/** Phase counter for automatic replication when AutomaticReplication == EveryNthFrame. */
	int FrameSkipPhase = 0;

	/** Last wall clock time when an automatic replication snapshot was taken. */
	double lastReplicationTime = 0.0;

};




/**
 * The abstract base class of deep snapshot system storage components.
 */
UCLASS( Abstract )
class RAGDOLLCONTROLLER_API UDeepSnapshotBase : public UActorComponent
{
	/*
	 * On binary compatibility:
	 * 
	 * In principle, deriving classes should make proper checks on binary compatibility with respect to replication. You should be pretty safe if you use UE
	 * types, serialize them through the serialization operator (or the FArchive::Serialize() method), make sure that you have the ArIsPersistent flag raised on
	 * the archiver, and access all data in the target component through well-defined API functions.
	 * However, if accessing the internals of the target using non-standard ways and/or if serializing data at the raw memory level, endianness and software
	 * version mismatches might interfere.
	 * 
	 * @todo: It might be a good idea to make this kind of checks in a centralized fashion, so as to avoid performing redundant checks for every replicated
	 * component on every replication tick. Maybe via DeepSnapshotManager, somehow?
	 * 
	 *
	 * Note on deriving new subclasses:
	 * 
	 * We permit automatic matching between snapshot components and target components. This is done by calling the IsAcceptableTargetType() pure virtual
	 * method, which the derived snapshot components then implement. There is a pitfall: a more general snapshot component might match and accept a more derived
	 * target component, for which the user did add a separate snapshot component. For example, consider that you have an Actor with a StaticMeshComponent and a
	 * SkeletalMeshComponent. Then you drop in a PrimitiveComponentSnapshot and a SkeletalMeshComponentSnapshot component, with auto matching enabled in both.
	 * It could happen that both snapshot components pick the SkeletalMeshComponent as their target, which is not what the user probably wanted.
	 * 
	 * To avoid this, you should make only leaf classes non-abstract, so as to remove any ambiguity.
	 * 
	 * An alternative solution would be to enhance the auto-matching logic so as to reject targets for which a better-matching snapshot class exists.
	 * This would be probably trivial to implement without hard-coding a class tree if the UE reflection system were a bit better documented.
	 */

	GENERATED_BODY()


	/* constructors/destructor */


public:

	// Sets default values for this component's properties
	UDeepSnapshotBase();




	/* UE interface overrides */


public:

	// Called when the game starts
	virtual void InitializeComponent() override;

	// Called every frame
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

	// Called by the editor to query which properties are currently editable
	virtual bool CanEditChange( const UProperty* InProperty ) const override;




	/* core snapshot functionality */


public:

	/** Take and store a snapshot of the current state of the component pointed by TargetComponent.
	 * @param	slotName	The name of the snapshot slot to be used */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void Snapshot( FName slotName );

	/** Apply a stored snapshot to the component pointed by TargetComponent.
	 * @param	slotName	The name of the snapshot slot to be used
	 * @param	success		Returns true on success, otherwise false */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void Recall( FName slotName, bool & success );

	/** Erase the specified snapshot slot.
	 * @param	slotName	The name of the snapshot slot to be erased
	 * @param	success		Returns true on success, otherwise false */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void Erase( FName slotName, bool & success );

	/** Erase all stored snapshots. */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void EraseAll();


protected:

	/** Serialize/deserialize the target to/from the provided archive. You can use archive.IsLoading() or archive.IsSaving() to determine whether you should
	 ** serialize or deserialize. When deserializing, the archive is guaranteed to contain data from an earlier serializing call to this method. */
	virtual void SerializeTarget( FArchive & archive, UActorComponent & target ) {}


private:

	/** Check that a target component exists, then serialize/deserialize the target to/from the provided archive. */
	void SerializeTargetIfNotNull( FArchive & archive );

	/** Storage slots for serialized snapshot data. We do not use a TMap on the top level as it is not supported by the UE reflection system. */
	UPROPERTY()
	TArray<FSnapshotData> Snapshots;




	/* target component selection */


public:

	/** The current target component for taking and applying snapshots. This can be changed at runtime. */
	UPROPERTY( BlueprintReadWrite, Category = "DeepSnapshotSystem" )
	UActorComponent * TargetComponent;


protected:

	/** If InitialTargetComponentName is set, then look up the corresponding component and assign it to TargetComponent. Return true if the target was found. */
	bool SelectTargetComponentByName();

	/** Find the first component with a matching type and assign it to TargetComponent. Return true if a matching component was found.
	 *	@see InitialTargetComponentName */
	bool SelectTargetComponentByType();

	/** Test whether the type of a target candidate component matches the nominal target type of this snapshot storage component. */
	virtual bool IsAcceptableTargetType( UActorComponent * targetCandidate ) const PURE_VIRTUAL( UDeepSnapshotBase::IsAcceptableTargetType, return false; );


private:

	/** If true, then the first component with a matching type will be selected as the snapshot target during initialization.
	 *  For example, a SkeletalMeshComponentSnapshot component would select the first SkeletalMeshComponent (or a derived type) as the target. */
	UPROPERTY( EditAnywhere, Category = "DeepSnapshotSystem", meta = (EditCondition = "!AutoSelectTargetComponentByType") )
	bool AutoSelectTarget = true;

	/**	The name of the component that should be selected as the snapshot target during initialization. Using this is mutually exclusive with the
	 *	AutoSelectTarget option. */
	UPROPERTY( EditAnywhere, Category = "DeepSnapshotSystem", meta=(EditCondition="!AutoSelectTarget") )
	FName InitialTargetComponentName;




	/* snapshot manager integration */


private:

	/** If there exists a DeepSnapshotManager actor in the world, then all deep snapshot components will automatically register with it during game start.
	 *  This list of snapshot group names can be used to assign this component to different snapshot groups during that registration. */
	UPROPERTY( EditAnywhere, Category = "DeepSnapshotSystem" )
	TArray<FName> SnapshotGroups;




	/* replication */


public:

	/** Deep replicate the current state of the component pointed by TargetComponent. This has an effect only on network authority. Network clients will
	 * automatically apply the state as soon as it is received. */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void Replicate();

	/** Automatic replication functionality. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "DeepSnapshotSystem" )
	FAutomaticReplication AutomaticReplication;


private:

	/** If authority and automatic replication is enabled, then consider taking a new replication snapshot according to the selected schedule. */
	void ConsiderTakingAutomaticReplicationSnapshot();

	/** Apply the snapshot that is currently stored in the replication snapshot field. */
	void ApplyReplicatedSnapshot();

	/** Handle replication events. This is called by the UE replication system whenever an update to the ReplicationData field is received. */
	UFUNCTION()
	void OnReplicationSnapshotUpdate();

	/** Special storage array for replication. This array is not affected by any snapshot bulk operations, nor is it serialized. */
	UPROPERTY( ReplicatedUsing = OnReplicationSnapshotUpdate )
	FSnapshotData ReplicationSnapshot;




	/* logging */


protected:

	/** Log, with diagnostics, a failed attempt to downcast the target component. Verbosity: Error. */
	void LogFailedDowncast( const char * functionName ) const;

	/** Create a diagnostics string for logging purposes. */
	FString LogCreateDiagnosticLine() const;




	/* utility */


private:

	/** Find the specified snapshot slot from the 'Snapshots' field. Return null if not found. */
	FSnapshotData * FindSnapshotByName( FName name );

	/** Look for a component in the owning actor that matches the predicate 'pred'. If found, then assign it to TargetComponent and return true.
	 * Otherwise return false. */
	bool SelectTargetComponentByPredicate( std::function<bool( UActorComponent * )> pred );

};
