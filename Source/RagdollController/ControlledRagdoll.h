// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "RemoteControllable.h"

#include <PxTransform.h>
#include <PxVec3.h>

#include <array>

#include "ControlledRagdoll.generated.h"




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
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = RagdollController )
	FVector JointAngles;

	FVector JointAnglesMy;


	/* Dynamic controller data (updated during the 1st half of Tick, before the actor's Blueprint is run) */

	/** Joint motor command for the current tick. The X, Y and Z dimensions correspond to twist, swing1 and swing2 in the PhysX joint, respectively. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = RagdollController )
	FVector MotorCommand;

};




/**
* Replication-ready struct for holding the state of a single bone. Velocity information is commented out as it is not currently being used (PhysX
* seems to ignore the set velocity if the pose is being set during the same tick; see AControlledRagdoll::SendPose).
*/
USTRUCT( Blueprintable )
struct FBoneState {
	GENERATED_USTRUCT_BODY()


private:

	UPROPERTY()
	TArray<int8> TransformData;

	//UPROPERTY()
	//TArray<int8> LinearVelocityData;

	//UPROPERTY()
	//TArray<int8> AngularVelocityData;


public:

	FBoneState()
	{
		TransformData.SetNumZeroed( sizeof(physx::PxTransform) );
		//LinearVelocityData.SetNumZeroed( sizeof(physx::PxVec3) );
		//AngularVelocityData.SetNumZeroed( sizeof(physx::PxVec3) );
	}


	/** Verifies that the size of the data fields match the size of the local PhysX objects. In general, word size of the platform might affect this, although
	 ** the current contents of this struct probably aren't affected. */
	bool DoDataSizesMatch()
	{
		return
			TransformData.Num() == sizeof(physx::PxTransform); // &&
			//LinearVelocityData.Num() == sizeof(physx::PxVec3) &&
			//AngularVelocityData.Num() == sizeof(physx::PxVec3);
	}


	physx::PxTransform & GetPxTransform()
	{
		return reinterpret_cast<physx::PxTransform &>(TransformData[0]);
	}

	//physx::PxVec3 & GetPxLinearVelocity()
	//{
	//	return reinterpret_cast<physx::PxVec3 &>(LinearVelocityData[0]);
	//}

	//physx::PxVec3 & GetPxAngularVelocity()
	//{
	//	return reinterpret_cast<physx::PxVec3 &>(AngularVelocityData[0]);
	//}

};




/**
 * 
 */
UCLASS()
class RAGDOLLCONTROLLER_API AControlledRagdoll :
	public AActor,
	public IRemoteControllable
{
	GENERATED_BODY()


	/** Last time that the pose was sent using SendPose(). */
	double lastSendPoseTime;


protected:

	/** The SkeletalMeshComponent of the actor to be controlled. */
	USkeletalMeshComponent * SkeletalMeshComponent;

	/** Server's float interpretation of 0xdeadbeef, for checking float representation compatibility (eg, float endianness). Assume that UE replicates
	 ** floats always correctly. */
	UPROPERTY( Replicated )
	float ServerInterpretationOfDeadbeef;


	/* State data */

	/** Cached joint names of the actor's SkeletalMeshComponent. When initialized, then JointNames.Num() == JointStates.Num(). */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = RagdollController )
	TArray<FName> JointNames;

	/** Data for all joints and associated bodies of the actor's SkeletalMeshComponent. Refresh errors are signaled by emptying the array: test validity with
	 ** JointStates.Num() != 0. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = RagdollController )
	TArray<FJointState> JointStates;

	/** Data for all bodies of the SkeletalMeshComponent, mainly for server-to-client pose replication. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Replicated, Category = RagdollController )
	TArray<FBoneState> BoneStates;


	/** Initialize the state data structs (read static data from the game engine, etc.) */
	void InitState();

	/** Tick hook that is called from Tick() after internal data structs have been updated but before sending anything out to PhysX and the remote controller.
	 ** This is called between the 1st and the 2nd half of Tick(), and just before running the actor's Blueprint. */
	virtual void TickHook( float deltaSeconds ) {};

	/** Run a sanity check on all Blueprint-writable data. */
	void ValidateBlueprintWritables();


	/* Inbound data flow, 1st half of Tick() */
	
	/** Read data from the game engine (PhysX etc). Called during the first half of each tick. */
	void ReadFromSimulation();

	/** If a remote controller is connected, then receive joint motor command and other data. */
	void ReadFromRemoteController();


	/* Outbound data flow, 2nd half of Tick() */

	/** Write data to the game engine (PhysX etc). Called during the second half of each tick. */
	void WriteToSimulation();

	/** If a remote controller is connected, then send pose and other data. */
	void WriteToRemoteController();


	/* Client-server replication */

	/** Recompute net update frequency, using the current frame rate estimate from our game mode instance. When using fixed time steps, this needs to be adjusted
	 ** for our real fps as UE interprets the AActor::NetUpdateFrequency parameter based on game time, not wall clock time. */
	void RecomputeNetUpdateFrequency( float gameDeltaTime );

	/** Store pose into the replicated BoneStates array. */
	void SendPose();

	/** Apply replicated pose from the BoneStates array. */
	void ReceivePose();


public:

	AControlledRagdoll();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	/** Ticking is performed in two stages. During the first stage, inbound data from the game engine and the remote controller is read and stored to internal
	 ** data structs. During the second stage, outbound data is sent back to the game engine and to the remote controller. TickHook() and the actor's Blueprint
	 ** are called between these stages. TickHook() is called just before the Blueprint. */
	virtual void Tick( float deltaSeconds ) override;

};
