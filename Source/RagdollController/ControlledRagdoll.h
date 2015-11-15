// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/GameMode.h"

#include "RCLevelScriptActor.h"
#include "RemoteControllable.h"

#include <PxTransform.h>
#include <PxVec3.h>

#include <array>

#include "ControlledRagdoll.generated.h"


class URemoteControllable;




/** Struct for representing data for a single PhysX joint. */
USTRUCT( Blueprintable )
struct FJointState
{
	GENERATED_USTRUCT_BODY()

	/* Static PhysX data (not updated during Tick) */

	/** Pointer to the FConstraintInstance of this joint. Make sure to always check that this is not null: it is possible to cause all non-Blueprintable
	 ** fields to become zeroed by the actor's Blueprint (eg, by changing struct fields by breaking and re-making the struct). */
	FConstraintInstance * Constraint;

	/** Pointers to the two bodies connected by the joint */
	std::array<FBodyInstance *, 2> Bodies;
	
	/** Bone indices for the two bodies connected by the joint */
	std::array<int32, 2> BoneInds;

	/** Rotations of the reference frames of the joint with respect to the two bodies connected by the joint */
	std::array<FQuat, 2> RefFrameRotations;


	/* Dynamic PhysX data (updated during the 1st half of Tick, before the actor's Blueprint is run) */

	/** Rotations of the connected bones in global coordinates (needed for applying motor forces via AddTorque()) */
	std::array<FQuat, 2> BoneGlobalRotations;

	/** PhysX joint angles. The X, Y and Z dimensions correspond to twist, swing1 and swing2 fields of the PhysX joint, respectively. */
	UPROPERTY( VisibleInstanceOnly, Category = RagdollController )
	FVector JointAngles;


	/* Dynamic controller data (updated during the 1st half of Tick, before the actor's Blueprint is run) */

	/** Joint motor command for the current tick. The X, Y and Z dimensions correspond to twist, swing1 and swing2 in the PhysX joint, respectively. */
	UPROPERTY( VisibleInstanceOnly, Category = RagdollController )
	FVector MotorCommand;

};




/**
 * Main class for a controlled ragdoll.
 */
UCLASS()
class RAGDOLLCONTROLLER_API AControlledRagdoll : public AActor
{
	GENERATED_BODY()




	/* constructors/destructor */


public:

	AControlledRagdoll();




	/* UE interface overrides */


public:

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	/** Ticking is performed in two stages. During the first stage, inbound data from the game engine and the remote controller is read and stored to internal
	 ** data structs. During the second stage, outbound data is sent back to the game engine and to the remote controller. TickHook() and the actor's Blueprint
	 ** are called between these stages. TickHook() is called just before the Blueprint. */
	virtual void Tick( float deltaSeconds ) override;




	/* ragdoll Blueprint interface */


public:

	/** Get the number of joints. */
	UFUNCTION( BlueprintCallable, Category = RagdollController )
	int32 GetNumJoints() const;

	/** Get an array containing the names of all joints.
	 *  @return		Returns a reference to the internal joint name array. Treat this as const (UE reflection does not permit const return types). */
	UFUNCTION( BlueprintCallable, BlueprintPure, Category = RagdollController )
	TArray<FName> & GetJointNames();

	/** Find the index of a joint by name.
	 *  @param JointName		Name of the joint to look up.
	 *  @return Index of the named joint. Will return INDEX_NONE if joint not found. */
	UFUNCTION( BlueprintCallable, Category = RagdollController )
	int32 GetJointIndex( FName JointName ) const;


	/** Get the current rotation angles of a given joint.
	 *  The X, Y and Z dimensions correspond to twist, swing1 and swing2 fields of the PhysX joint, respectively. */
	UFUNCTION( BlueprintCallable, Category = RagdollController )
	FVector GetJointAngles( int32 JointIndex ) const;

	/** Get the current motor command of a given joint.
	 *  The X, Y and Z dimensions correspond to twist, swing1 and swing2 fields of the PhysX joint, respectively. */
	UFUNCTION( BlueprintCallable, Category = RagdollController )
	FVector GetJointMotorCommand( int32 JointIndex ) const;

	/** Set the motor command of a given joint.
	 *  The X, Y and Z dimensions correspond to twist, swing1 and swing2 fields of the PhysX joint, respectively. */
	UFUNCTION( BlueprintCallable, Category = RagdollController )
	void SetJointMotorCommand( int32 JointIndex, const FVector & MotorCommand );




	/* ragdoll C++ interface */
	

public:

	/** Get a const reference to the internal joint name array. */
	const TArray<FName> & getJointNames() const { return JointNames; }

	/** Get a const reference to the interla joint state array. */
	const TArray<FJointState> & getJointStates() const { return JointStates; }

	/** Get a non-const reference to the interla joint state array. */
	TArray<FJointState> & getJointStates() { return JointStates; }




	/* internal */


protected:

	/** Tick hook that is called from Tick() after internal data structs have been updated but before sending anything out to PhysX and the remote controller.
	 ** This is called between the 1st and the 2nd half of Tick() (see below), and just before running the actor's Blueprint. */
	virtual void TickHook( float deltaSeconds ) {};


private:

	/** Initialize the state data structs (read static data from the game engine, etc.) */
	void InitState();

	/** Run a sanity check on all Blueprint-writable data. */
	void ValidateBlueprintWritables();


	/* Inbound data flow, 1st half of Tick() */
	
	/** If xml data was received from a remote controller, then handle all commands with inbound data (setters). */
	URemoteControllable::UserFrame ReadFromRemoteController();

	/** Read data from the game engine (PhysX etc). Called during the first half of each tick. */
	void ReadFromSimulation();


	/* Outbound data flow, 2nd half of Tick() */

	/** Write data to the game engine (PhysX etc). Called during the second half of each tick. */
	void WriteToSimulation();

	/** If xml data was received from a remote controller, then handle all commands that request outbound data (getters). */
	void WriteToRemoteController( URemoteControllable::UserFrame frame );


	/** The SkeletalMeshComponent of the actor to be controlled. This is guaranteed to be always valid. */
	USkeletalMeshComponent * SkeletalMeshComponent{};

	/** Our custom LevelScriptActor. Note that this is always null during an editor session! */
	ARCLevelScriptActor * RCLevelScriptActor{};

	/** The GameMode. Note that this is always null on non-authority, and probably also during an editor session! */
	AGameMode * GameMode{};

	/** The RemoteControllable component. This can be null so check before use! */
	URemoteControllable * RemoteControllable{};


	/* ragdoll state */

	/** Cached joint names of the actor's SkeletalMeshComponent. When initialized, then JointNames.Num() == JointStates.Num(). */
	UPROPERTY( VisibleInstanceOnly, Category = RagdollController )
	TArray<FName> JointNames;

	/** Data for all joints and associated bodies of the actor's SkeletalMeshComponent. Refresh errors are signaled by emptying the array: test validity with
	 ** JointStates.Num() != 0. */
	UPROPERTY( VisibleInstanceOnly, Category = RagdollController )
	TArray<FJointState> JointStates;


	// Temporary, remove when done testing
	int tickCounter = -1;

};
