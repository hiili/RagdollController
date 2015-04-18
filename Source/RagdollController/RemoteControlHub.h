// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "XmlFSocket.h"

#include <Networking.h>

#include <string>
#include <memory>

#include "RemoteControlHub.generated.h"




/**
 * 
 */
UCLASS(Blueprintable)
class RAGDOLLCONTROLLER_API ARemoteControlHub : public AActor
{
	GENERATED_BODY()

	
protected:
	
	/** Main listen socket */
	std::unique_ptr<FSocket> ListenSocket;

	/** Connection sockets that have not yet been dispatched. Currently, there are no cleanup mechanisms for stalled connections. */
	TArray< std::unique_ptr<XmlFSocket> > PendingSockets;
	

	/** Create the main listen socket. */
	void CreateListenSocket();

	/** Check the listen socket for new incoming connection attempts. Accept and add them to pending connections. */
	void CheckForNewConnections();

	/** Check if any of the PendingSockets have received the necessary information for doing a dispatch. */
	void ManagePendingConnections();

	/** Try to dispatch the socket according to the command. Close and discard the socket upon errors. */
	void DispatchSocket( std::string command, std::unique_ptr<XmlFSocket> socket );


	/* commands */

	/** Connect directly to an actor that implements the RemoteControllable interface */
	void CmdConnect( std::string args, std::unique_ptr<XmlFSocket> socket );


public:

	ARemoteControlHub();

	/** Initialize the remote control hub and start listening for incoming connections. */
	virtual void PostInitializeComponents() override;

	/** Check and dispatch new incoming connections. */
	virtual void Tick( float deltaSeconds ) override;
	
};
