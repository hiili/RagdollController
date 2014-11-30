// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include <SharedPointer.h>

#include <string>

#include "RemoteControlInterface.generated.h"

class FSocket;
class LineFSocket;




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

	/** Connection sockets that have not yet been dispatched. Currently, there are no cleanup mechanisms for stalled connections. */
	TArray< TSharedPtr<LineFSocket> > PendingSockets;

	/** Effective receive buffer size of the listen socket */
	int32 RCIReceiveBufferSize;
	

	/** Create the main listen socket. */
	void CreateListenSocket();

	/** Check the listen socket for new incoming connection attempts. Accept and add them to pending connections. */
	void CheckForNewConnections();

	/** Check if any of the PendingSockets have received the necessary information for doing a dispatch. */
	void ManagePendingConnections();

	/** Try to dispatch the socket according to the command. Close and discard the socket upon errors. */
	void DispatchSocket( std::string command, const TSharedPtr<LineFSocket> & socket );


	/* commands */

	/** Connect to an actor */
	void CmdConnect( std::string args, const TSharedPtr<LineFSocket> & socket );


public:

	/** Initialize the remote control interface. */
	virtual void PostInitializeComponents() override;

	/** Check and dispatch new incoming connections. */
	virtual void Tick( float deltaSeconds ) override;
	
};
