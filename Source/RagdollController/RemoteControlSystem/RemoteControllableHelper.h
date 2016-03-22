// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/SceneComponent.h"
#include "RemoteControllableHelper.generated.h"




/**
 * Helper class for triggering post-user-tick comms operations of a RemoteControllable component. Used only by RemoteControllable; do not instantiate directly!
 * 
 * The sole function of this class is to call the host RemoteControllable's postUserTick() method whenever it ticks. This should happen after the users of the
 * host RemoteControllable have ticked, which is meant to be enforced by tick prerequisites set by the host.
 */
UCLASS( ClassGroup=(Custom) )
class RAGDOLLCONTROLLER_API URemoteControllableHelper : public USceneComponent
{
	GENERATED_BODY()

public:

	// Sets default values for this component's properties
	URemoteControllableHelper();

	// Called every frame
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

};
