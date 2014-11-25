// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "RemoteControllable.h"

#include <PxTransform.h>
#include <PxVec3.h>

#include <array>

#include "ControlledRagdoll.generated.h"




/** Struct for representing data for a single PhysX joint. */
struct JointState
{

	// static data (not updated during Tick)

	/** Pointer to the FConstraintInstance of this joint */
	FConstraintInstance * Constraint;

	/** Pointers to the two bodies connected by the joint */
	std::array<FBodyInstance *, 2> Bodies;
	
	/** Bone indices for the two bodies connected by the joint */
	std::array<int32, 2> BoneInds;

	/** Rotations of the reference frames of the joint with respect to the two bodies connected by the joint */
	std::array<FQuat, 2> RefFrameRotations;


	// dynamic data (updated during Tick)

	/** Rotations of the connected bones (in global coordinates) */
	std::array<FQuat, 2> BoneGlobalRotations;

	/** PhysX joint rotation. The pitch, roll and yaw dimensions correspond to twist, swing1 and swing2 fields of the PhysX joint, respectively. */
	FRotator Rotation;
};




/**
* Replication-ready struct for holding the state of a single bone. Velocity information is commented out as it is not currently being used (PhysX
* seems to ignore set velocity if the pose is being set during the same tick; see AControlledRagdoll::sendPose).
*/
USTRUCT()
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


	/**
	* Verifies that the size of the data fields match the size of the local PhysX objects. In general, word size of the platform might affect this (although
	* the current contents of this struct probably aren't affected).
	*/
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
class RAGDOLLCONTROLLER45_API AControlledRagdoll :
	public AActor,
	public IRemoteControllable
{
	GENERATED_UCLASS_BODY()


	/** Last time that the pose was sent using sendPose(). */
	double lastSendPoseTime;


protected:

	/** The SkeletalMeshComponent of the actor to be controlled. */
	USkeletalMeshComponent * SkeletalMeshComponent;


	/** Joint names of the actor's SkeletalMeshComponent. When initialized, then JointNames.Num() == SkeletonState.Num(). */
	TArray<FName> JointNames;

	/** Data for all joints and associated bodies of the actor's SkeletalMeshComponent. Refresh errors are signaled by emptying the array: test validity with SkeletonState.Num() != 0. */
	TArray<JointState> JointStates;

	/** Data for all bodies of the SkeletalMeshComponent, for server-to-client pose replication. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Replicated, Category = RagdollController )
	TArray<FBoneState> BoneStates;


	/** True if the server runs on a little endian platform. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Replicated, Category = RagdollController )
	bool IsServerLittleEndian;

	
	/** If a remote controller is connected, then send pose data and receive joint motor command data. */
	void communicateWithRemoteController();

	/**
	* Recompute net update frequency, using the current frame rate estimate from our game mode instance. When using fixed time steps, this needs to be adjusted
	* for our real fps as UE interprets the AActor::NetUpdateFrequency parameter based on game time, not wall clock time.
	* */
	void recomputeNetUpdateFrequency( float gameDeltaTime );

	/** Store pose into the replicated BoneStates array. */
	void sendPose();

	/** Apply replicated pose from the BoneStates array. */
	void receivePose();


public:

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick( float deltaSeconds ) override;


	/** Refresh all static joint data structs. */
	void refreshStaticJointData();

	/** Refresh all dynamic joint data structs. */
	void refreshDynamicJointData();

};
