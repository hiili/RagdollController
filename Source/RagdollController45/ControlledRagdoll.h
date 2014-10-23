// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include <PxTransform.h>

#include <array>
using std::array;

#include "ControlledRagdoll.generated.h"




/** Struct for representing data for a single PhysX joint. */
struct JointState
{

	// static data (not updated during Tick)

	/** Pointer to the FConstraintInstance of this joint */
	FConstraintInstance * Constraint;

	/** Pointers to the two bodies connected by the joint */
	array<FBodyInstance *, 2> Bodies;
	
	/** Bone indices for the two bodies connected by the joint */
	array<int32, 2> BoneInds;

	/** Rotations of the reference frames of the joint with respect to the two bodies connected by the joint */
	array<FQuat, 2> RefFrameRotations;


	// dynamic data (updated during Tick)

	/** Rotations of the connected bones (in global coordinates) */
	array<FQuat, 2> BoneGlobalRotations;

	/** PhysX joint rotation. The pitch, roll and yaw dimensions correspond to twist, swing1 and swing2 fields of the PhysX joint, respectively. */
	FRotator Rotation;
};




USTRUCT()
struct FPxTransform {

	GENERATED_USTRUCT_BODY()

private:  // todo make it a UCLASS?

	UPROPERTY()
	TArray<int8> Data;

public:

	FPxTransform()
	{
		Data.SetNumZeroed( sizeof(physx::PxTransform) );
	}

	physx::PxTransform & GetPxTransform()
	{
		return reinterpret_cast<physx::PxTransform &>( Data[0] );
	}

};


/**
 * 
 */
UCLASS()
class RAGDOLLCONTROLLER45_API AControlledRagdoll : public AActor
{
	GENERATED_UCLASS_BODY()

protected:

	// The SkeletalMeshComponent of the actor to be controlled
	USkeletalMeshComponent * SkeletalMeshComponent;


	/** Joint names of the actor's SkeletalMeshComponent. When initialized, then JointNames.Num() == SkeletonState.Num() */
	TArray<FName> JointNames;

	/** Data for all joints and associated bodies of the actor's SkeletalMeshComponent. Refresh errors are signaled by emptying the array: test validity with SkeletonState.Num() != 0 */
	TArray<JointState> JointStates;

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Replicated, Category = RagdollController )
	TArray<FPxTransform> PxBoneTransforms;


	//UPROPERTY( EditAnywhere, BlueprintReadWrite, Replicated, Category = RagdollController )
	TArray<float> foo;


public:

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick( float deltaSeconds ) override;


	/** Refresh all caches for static joint data. */
	void refreshStaticJointData();

	/** Refresh all dynamic joint data structs. */
	void refreshDynamicJointData();

};
