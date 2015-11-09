// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include "DeepSnapshotBase.h"

#include "DeepSnapshotManager.generated.h"




/**
 * Central manager class for all deep snapshot components in the world. It can be used to operate several snapshot components at once.
 * If a manager class exists in the world, then all snapshot components register with it automatically during game start. Registered components can be
 * destroyed safely: the manager performs necessary pointer validity checks and automatically prunes stale pointers from its registry.
 * At the moment, snapshot components assume that at most one manager exists in the world.
 */
UCLASS()
class RAGDOLLCONTROLLER_API ADeepSnapshotManager : public AActor
{
	GENERATED_BODY()

	/** Permit deep snapshot components to have private access during initialization, so that they can register themselves with us. */
	friend void UDeepSnapshotBase::InitializeComponent();




	/* constructors/destructor */


public:	

	// Sets default values for this actor's properties
	ADeepSnapshotManager();




	/* UE interface overrides */


public:

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

	
	

	/* core snapshot functionality */


public:

	/** Return true if the specified snapshot group is not empty, otherwise return false. Always return true if the group name is 'None'. */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	bool IsGroupNonEmpty( FName groupName );

	/** Call Snapshot() on each deep snapshot component that belongs to the specified snapshot group.
	 * @param	groupName	The snapshot group on which to perform the operation. Can be 'None', in which case the operation is performed on all
	 *						snapshot components in the world. Note that in C++, the FName default constructor produces a 'None' name.
	 * @param	slotName	The name of the snapshot slot to be used
	 * @param	success		Return true if the group is not empty (ignore this check if group is 'None'), otherwise return false */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void Snapshot( FName groupName, FName slotName, bool & success );

	/** Call Recall() on each deep snapshot component that belongs to the specified snapshot group.
	 * @param	groupName	The snapshot group on which to perform the operation. Can be 'None', in which case the operation is performed on all
	 *						snapshot components in the world. Note that in C++, the FName default constructor produces a 'None' name.
	 * @param	slotName	The name of the snapshot slot to be used
	 * @param	success		Return true if the group is not empty (ignore this check if group is 'None') and all recall operations succeeded,
	 *						otherwise return false */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void Recall( FName groupName, FName slotName, bool & success );

	/** Erase the specified snapshot slot from all deep snapshot components that belong to the specified snapshot group.
	 * @param	groupName	The snapshot group on which to perform the operation. Can be 'None', in which case the operation is performed on all
	 *						snapshot components in the world. Note that in C++, the FName default constructor produces a 'None' name.
	 * @param	slotName	The name of the snapshot slot to be used
	 * @param	success		Return true if the group is not empty (ignore this check if group is 'None') and all erase operations succeeded,
	 *						otherwise return false */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void Erase( FName groupName, FName slotName, bool & success );

	/** Erase all stored snapshots from all deep snapshot components that belong to the specified snapshot group.
	 * @param	groupName	The snapshot group on which to perform the operation. Can be 'None', in which case the operation is performed on all
	 *						snapshot components in the world. Note that in C++, the FName default constructor produces a 'None' name.
	 * @param	success		Return true if the group is not empty (ignore this check if group is 'None'), otherwise return false */
	UFUNCTION( BlueprintCallable, Category = "DeepSnapshotSystem" )
	void EraseAll( FName groupName, bool & success );




	/* deep snapshot component management */

private:

	/** Registers a deep snapshot component. Deep snapshot components consider registering themselves during the InitializeComponent() stage.
	 *  There is no unregister method at the moment; registration is currently for lifetime.
	 *  @param snapshotGroups	A list of names of the snapshot groups to which this component should be added. This list can be empty, in which case the
	 *							component can be reached only via operations that have their target snapshot group left to 'None'. */
	void RegisterSnapshotComponent( UDeepSnapshotBase * component, const TArray<FName> & snapshotGroups );


	/** The set of registered snapshot components that should receive commands from this manager, keyed by their snapshot group name.
	 *
	 *  Usage pattern: Fill on game start (does not need to be fast). During gameplay on snapshot commands, iterate over complete element sets that have the
	 *  same key (needs to be fast).
	 *  
	 *  Chosen container type: Split to TMap and TSet instead of using TMultiMap, because documentation on iteration complexity over the latter is lacking. */
	TMap< FName, TSet< TWeakObjectPtr<UDeepSnapshotBase> > > registeredSnapshotComponentsByGroup;




	/* utility */

private:

	/** Perform an operation on all components that have been registered with the given snapshot group. Prunes all encountered stale pointers.
	 *  @param	groupName	Name of the snapshot group. Can be 'None' (FName()), in which case the operation is performed on all registered components.
	 *  @param	function	The function to apply on the components, eg a lambda. Required signature: void function( UDeepSnapshotBase & component )
	 *  @return				Return true if the specified group was found and it has at least one non-stale pointer, false otherwise. */
	template<typename Function>
	bool ForEachInGroup( FName groupName, Function function );

	/** Perform an operation on all components in the given snapshot group container. Prunes all encountered stale pointers.
	 *  @param	group		The snapshot group.
	 *  @param	function	The function to apply on the components, eg a lambda. Required signature: void function( UDeepSnapshotBase & component )
	 *  @return				Return true if the specified group has at least one non-stale pointer, false otherwise. */
	template<typename Function>
	bool ForEachInGroup( TSet< TWeakObjectPtr<UDeepSnapshotBase> > & group, Function function );

};








/* inline and template method implementations */




template<typename Function>
bool ADeepSnapshotManager::ForEachInGroup( FName groupName, Function function )
{
	// was a group name provided?
	if( groupName.IsNone() )
	{
		// no: loop through all groups, always return success
		for( auto & groupEntry : registeredSnapshotComponentsByGroup )
		{
			ForEachInGroup( groupEntry.Value, function );
		}
		return true;
	}
	else
	{
		// yes: look up the specified group
		TSet< TWeakObjectPtr<UDeepSnapshotBase> > * groupPtr = registeredSnapshotComponentsByGroup.Find( groupName );
		if( groupPtr )
		{
			// group found -> apply and return success if there were non-stale pointers to call
			if( ForEachInGroup( *groupPtr, function ) ) return true;
		}

		// group not found or there were no non-stale pointers -> log and return failure
		UE_LOG( LogDeepSnapshotSystem, Warning, TEXT( "(%s) The specified snapshot group is empty! Group name: %s" ),
			TEXT( __FUNCTION__ ), *groupName.ToString() );
		return false;
	}
}


template<typename Function>
bool ADeepSnapshotManager::ForEachInGroup( TSet< TWeakObjectPtr<UDeepSnapshotBase> > & group, Function function )
{
	// collect a list of stale pointers so that we can remove them at the end
	TArray< TWeakObjectPtr<UDeepSnapshotBase> > stalePointers;

	// loop through components in this group
	for( TWeakObjectPtr<UDeepSnapshotBase> & componentWeakPtr : group )
	{
		// stale pointer? record for later erasing, then continue with next element
		if( !componentWeakPtr.IsValid() )
		{
			stalePointers.Emplace( componentWeakPtr );
			continue;
		}

		// call the function
		function( *componentWeakPtr.Get() );
	}

	// erase stale pointers
	for( auto stalePointer : stalePointers ) group.Remove( stalePointer );

	// stale pointers are now pruned out; return success if the group is still non-empty
	return group.Num() > 0;
}
