// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "XmlFSocket.h"

#include <Networking.h>

#include <string>
#include <memory>

#include "RemoteControlHub.generated.h"


#define DEFAULT_LISTENPORT 7770




/**
 * A hub entity for parsing and forwarding incoming connection requests from remote controllers to components of type RemoteControllable.
 * 
 * The hub is activated by simply dropping one into the Unreal world. It listens on a TCP port (7770 by default, which can be changed via the editor).
 * Once a TCP connection is received, the remote controller can send command lines to the hub. Incoming lines can be terminated with LF or CRLF; all
 * outgoing lines are terminated with LF. Currently, only the CONNECT command is supported, and it must be preceded with a proper handshake string.
 * The usage is as follows:
 *
 *   <remote> RagdollController RCH: CONNECT Owen
 *      <hub> OK
 * 
 * The connection has now been forwarded to the RemoteControllable component that has a matching NetworkName set for it (@see RemoteControllable::NetworkName).
 * In this case, a RemoteControllable with the network name Owen will be selected. The target name can be also a ?*-pattern. In case of an error, the hub
 * responds with the line ERROR. The actual reason for the error is logged in the engine logs. All log output is placed in the LogRemoteControlSystem log.
 * 
 * From this point on, the communication link is handled by the RemoteControllable component; the hub does not intervene in the connection in any further way.
 * 
 * On timing: The RemoteControlHub always ticks before any RemoteControllable components. Consequently, a target RemoteControllable will start to run its
 * schedule already during the same tick on which the corresponding CONNECT command is received and processed by the hub.
 * 
 * @see RemoteControllable
 */
UCLASS(Blueprintable)
class RAGDOLLCONTROLLER_API ARemoteControlHub : public AActor
{
	GENERATED_BODY()

	
public:

	ARemoteControlHub();

	/** Initialize the remote control hub and start listening for incoming connections. */
	virtual void PostInitializeComponents() override;

	/** Check and dispatch new incoming connections. */
	virtual void Tick( float deltaSeconds ) override;




private:
	
	/** TCP listen port. */
	UPROPERTY( EditAnywhere, Category = "Remote Control System" )
	int32 ListenPort = DEFAULT_LISTENPORT;

	/** If true, then binds to 127.0.0.1, effectively making the hub reachable only on localhost.
	 *  Otherwise bind to 0.0.0.0, effectively granting world access. */
	UPROPERTY( EditAnywhere, Category = "Remote Control System" )
	bool ListenOnlyOnLocalhost = true;


	/** Create the main listen socket. */
	void CreateListenSocket();

	/** Check the listen socket for new incoming connection attempts. Accept and add them to pending connections. */
	void CheckForNewConnections();

	/** Check if any of the PendingSockets have received the necessary information for doing a dispatch. */
	void ManagePendingConnections();

	/** Try to dispatch the socket according to the command. Close and discard the socket upon errors. */
	void DispatchSocket( std::string command, std::unique_ptr<XmlFSocket> socket );


	/** Main listen socket */
	std::unique_ptr<FSocket> ListenSocket;

	/** Connection sockets that have not yet been dispatched. Currently, there are no cleanup mechanisms for stalled connections. */
	TArray< std::unique_ptr<XmlFSocket> > PendingSockets;


	/* commands */

	/** Connect directly to an object that implements the RemoteControllable interface */
	void CmdConnect( std::string args, std::unique_ptr<XmlFSocket> socket );

};
