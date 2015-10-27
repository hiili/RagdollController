// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "XmlFSocket.h"

#include <Networking.h>

#include <string>
#include <memory>

#include "RemoteControlHub.generated.h"




/**
 * A hub entity for parsing and forwarding incoming connection requests from remote controllers to RemoteControllable actors.
 * 
 * The hub is activated by simply dropping one into the Unreal world. It listens on a TCP port (7770, hard-coded at the moment). Once a connection is
 * received, the remote controller can send command lines to the hub. All incoming lines must be terminated with LF or CRLF; all outgoing lines are terminated
 * with LF. Currently, only the CONNECT command is supported, and it must be preceded with a proper handshake string. The usage is as follows:
 *
 *   <remote> RagdollController RCH: CONNECT Owen
 *   <hub> OK
 * 
 * The connection has now been forwarded to the AActor with the provided name (Owen, in this case). The target name can be a ?*-pattern. Keep in mind that
 * UE occasionally adds an underscore-delimited suffix to actor names. You can mostly avoid this by using actor names that do not collide with existing class
 * names. The target actor must inherit from RemoteControllable. In case of an error, the hub responds with the line ERROR. The actual reason for the error is
 * logged in the engine logs. All log output is placed in the LogRcRch log.
 * 
 * From this point on, the communication protocol depends on the target actor; the hub does not intervene in the connection in any further way. However, the
 * hub wraps the connection socket into an XmlFSocket object, thus providing the basic tools for line-based and XML-based communications.
 * If you are going to communicate with Matlab, then you might want to use also the Mbml helper class.
 * 
 * @see RemoteControllable, XmlFSocket, Mbml
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
