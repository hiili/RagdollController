// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include <SharedPointer.h>

#include "RemoteControlInterface.generated.h"

class FSocket;




/**
 * 
 */
UCLASS(Blueprintable)
class RAGDOLLCONTROLLER45_API ARemoteControlInterface : public AActor
{
	GENERATED_UCLASS_BODY()

	
protected:

	/** Main listen socket */
	TSharedPtr<FSocket> ListenSocket;

	/** Connection sockets that have not yet been dispatched */
	//TArray< TSharedPtr<FSocket> > PendingSockets;

	/** Effective receive buffer size of the listen socket */
	int32 RCIReceiveBufferSize;


public:

	/** Initialize the remote control interface. */
	virtual void PostInitializeComponents() override;

	/** Check and dispatch new incoming connections. */
	virtual void Tick( float deltaSeconds ) override;
	
};
