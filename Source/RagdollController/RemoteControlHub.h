// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

#include <SharedPointer.h>

#include <string>

#include "RemoteControlHub.generated.h"

class FSocket;
class XmlFSocket;




/**
 * A communications hub for remote controllers.
 * 
 * All communication is performed over TCP sockets using XML. The hub itself acts as a proxy between remote controllers and local actors (or components).
 * Communication uses a master/slave protocol, in which remote controllers act as masters and local actors act as slaves.
 * 
 * Communication with a remote controller is initiated when the remote creates a TCP connection via the hub's listen socket. After that, the remote can send
 * XML documents(*) through the connection, and the hub parses and forwards them to the targeted actors. Whenever a remote sends data to a specific actor,
 * the actor can return data immediately back to that specific remote.
 * 
 * All local actors that wish to communicate with a remote need to first register themselves with the hub and provide a name by which remotes can address them
 * (for example, actor->GetName(), or Utility::CleanupName( actor->GetName() )). This registration is performed using the ARemoteControlHub::Register() method.
 * Remotes address specific actors by placing the data under a root's child XML element that has the same name as the targeted actor.
 * The subtree below that element is then passed to the actor using the IRemoteControllable interface's Communicate() method
 * (which the receiving actor must implement). The actor can return data back to the originating remote by returning an XML subtree from that call.
 * This subtree is then placed by the hub into a single return XML document as a root's child element with the name of the actor. When all actors contacted
 * by a specific remote have been processed, the accumulated return XML document is serialized and sent back to the remote through the TCP socket.
 * 
 * Blocking: A TCP connection can be opened by a remote in synchronous or asynchronous mode. In synchronous mode, exactly one new XML document is expected on
 * each tick, and the game thread is halted until one has been received. Possible excess documents are left to subsequent ticks. In asynchronous mode, incoming
 * XML documents are processed whenever they arrive: if nothing has been received when the hub ticks, then no communication takes place during that tick,
 * and if there are several documents waiting in the socket, then all of them are processed immediately (one return document is sent back per each incoming
 * document, together with an optional serial number provided by the remote in the inbound document so as to match inbound documents with corresponding outbound
 * return documents).
 * 
 * (*) Note that all inbound XML documents must be preceded by a proper block header in the TCP stream, and all outbound XML documents will be preceded by
 * such a header; see XmlFSocket documentation for the header syntax.
 * 
 * 
 * TCP protocol:   (check the #defines in RemoteControlHub.cpp for relevant up-to-date string literals)
 *   1. The remote makes a connection to the listen server's listen socket (the default port is 7770)
 *   2. The remote sends the handshake string and the initial command. For initiating XML communications as described above, this is:
 *        RagdollController RCH: START_GLOBAL_XML <synchronous|asynchronous>
 *      where <synchronous|asynchronous> is either of the two string literals.
 *   3. For each inbound communication call:
 *      1. The remote sends a proper XML block header (see XmlFSocket), followed by an XML document with the following root-level structure:
 *         <RemoteControlCall>
 *           [<id>(an arbitrary id string that the hub adds to the associated return document)</id>]
 *           <(actor-name)>(an arbitrary XML subtree that will we forwarded to the actor registered with the name (actor-name))</(actor-name)>
 *           <(actor2-name)>...
 *         </RemoteControlCall>
 *      2. The hub calls the named actor's IRemoteControllable::Communicate() method and provides the associated XML subtree as an argument. The Communicate()
 *         method can return another XML subtree.
 *      3. The hub collects all subtrees returned from the addressed actor's into a single XML return document with the following root-level structure:
 *         <RemoteControlResponse>
 *           [<id>(the id from the corresponding call document, if one was provided)</id>]
 *           <(actor-name)>(an arbitrary XML subtree that will was returned by the actor registered with the name (actor-name))</(actor-name)>
 *           <(actor2-name)>...
 *         </RemoteControlResponse>
 *   4. Go to step 3 and wait for the next inbound communication call.
 */
UCLASS(Blueprintable)
class RAGDOLLCONTROLLER_API ARemoteControlHub : public AActor
{
	GENERATED_BODY()

	
protected:
	
	/** Main listen socket */
	TSharedPtr<FSocket> ListenSocket;

	/** Connection sockets that have not yet been dispatched. Currently, there are no cleanup mechanisms for stalled connections. */
	TArray< TSharedPtr<XmlFSocket> > PendingSockets;
	

	/** Create the main listen socket. */
	void CreateListenSocket();

	/** Check the listen socket for new incoming connection attempts. Accept and add them to pending connections. */
	void CheckForNewConnections();

	/** Check if any of the PendingSockets have received the necessary information for doing a dispatch. */
	void ManagePendingConnections();

	/** Try to dispatch the socket according to the command. Close and discard the socket upon errors. */
	void DispatchSocket( std::string command, const TSharedPtr<XmlFSocket> & socket );


	/* commands */

	/** Connect directly to an actor that implements the RemoteControllable interface */
	void CmdConnect( std::string args, const TSharedPtr<XmlFSocket> & socket );


public:

	ARemoteControlHub();

	/** Initialize the remote control hub and start listening for incoming connections. */
	virtual void PostInitializeComponents() override;

	/** Check and dispatch new incoming connections. */
	virtual void Tick( float deltaSeconds ) override;
	
};
