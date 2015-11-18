// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"

#include "RemoteControlHub.h"

#include <string>

#include "RemoteControllable.generated.h"




/**
 * An actor component for communicating with a remote controller over a TCP socket using XML. The connection is initiated by the remote controller by
 * contacting the RemoteControlHub actor. The remote can then request a connection dispatch to a RemoteControllable component based on the component's
 * NetworkName (the NetworkName can be set via the editor). The connection will be rejected if the component has no registered users (see below).
 * 
 * On the UE side, all users of the control link (components and actors) must initially register with the RemoteControllable using the RegisterUser() method.
 * This must happen before the remote opens the connection; trying to register once a connection is already open is an error (will be rejected and logged).
 * 
 * All communication takes place synchronously. All registered users should perform the following steps during every tick:
 *   1. Call OpenFrame(), which returns a UserFrame with two handles: one to a command XML tree and one to a response XML tree.
 *   2. If the UserFrame contains non-null handles, then read data from the command tree and write data to the response tree.
 *   3. Call CloseFrame(), even if OpenFrame() returned null handles.
 * 
 * Once a connection is established, the RemoteControllable will read exactly one XML document from the remote controller per tick. This happens when the first
 * user calls OpenFrame(). The response XML document is sent to the remote immediately after the last registered user has called CloseFrame().
 * 
 * The response document is not cleared between ticks: each user continues with their response tree from the previous tick. This way the user does not need to
 * re-create the tree layout on each tick and typically needs to only overwrite node data.
 * 
 * @see RemoteControlHub, XmlFSocket, Mbml
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RAGDOLLCONTROLLER_API URemoteControllable : public UActorComponent
{
	GENERATED_BODY()

	/** Permit the remote control hub to have private access, so that it can connect us with the remote controller.
	 *  Members intended to be accessed: ConnectWith(), NetworkName */
	friend class ARemoteControlHub;




	/* constructors/destructor */


public:

	// Sets default values for this component's properties
	URemoteControllable();




	/* UE interface overrides */


public:

	// Called when the game starts
	//virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;




	/* The remote control interface */


public:

	/** Register a new user of the control link. This must be called before the remote controller opens the connection; trying to register after that will fail.
	 *  Once registered, the user is responsible to call OpenFrame() followed by a call to CloseFrame() exactly once per each tick. There is no unregister
	 *  method at the moment; registration is for lifetime. Registration will create a tick prerequisite between the RemoteControllable component and the user.
	 *  
	 *  @param	user			A pointer to the user, which must be an AActor (an overload exists for UActorComponent users).
	 *  @param	xmlTreeName		The name of the subtree in the command and response XML documents that the user wishes to use. This must be unique among all
	 *							users of this RemoteControllable component. @see OpenFrame()
	 *  @return					True on success, false otherwise. The call can fail for various reasons, including: the user is trying to register multiple
	 *							times, the xmlTreeName is already taken, or the control link is already open. */
	bool RegisterUser( AActor & user, const std::string & xmlTreeName );

	/** Registration method for UActorComponent users. See the AActor overload for documentation. */
	bool RegisterUser( UActorComponent & user, const std::string & xmlTreeName );


	/** Data struct that represents a specific user's portion of the current tick's network communication frame. Note that you can test the validity of the
	 *  handles as if they were booleans, just like with regular pointers. */
	struct UserFrame
	{
		/** A handle to the command element. Modifications to this element are allowed but will be discarded when the frame is closed. Can be a null handle. */
		pugi::xml_node command;

		/** A handle to the response element. This element will be sent back to the remote once the frame is closed. Can be a null handle. */
		pugi::xml_node response;
	};

	/** Opens the command and response XML trees corresponding to the current tick. Each user has provided a tree name during registration (xmlTreeName).
	 *  The root element of the command XML document should contain a child element with a matching name. If found, then a handle to this element will be
	 *  returned. The name of the root element is ignored. The response XML document will contain a single root element, under which a child element is placed
	 *  for every registered user. The names of these child elements equal the xmlTreeNames of the users. A handle to the calling user's response
	 *  element will be returned. Note that the response elements are not cleared between ticks.
	 *  
	 *  @param	user			A pointer to the user.
	 *  @return					A UserFrame struct containing the command and response element handles. Both handles will be null handles in case of an error.
	 *							This can happen for various reasons, including: no connection has been established yet, the connection has failed, or the user's
	 *							element is missing from the command XML document. */
	UserFrame OpenFrame( const UObject & user );

	/** Closes the frame opened via OpenFrame(). This should be called even if OpenFrame() returned a UserFrame with null handles.
	 *  @param	user			A pointer to the user. */
	void CloseFrame( const UObject & user );


private:

	/** Actual local registration for both AActor and UActorComponent users. */
	template<typename UserType>
	bool RegisterUser( UObject & user, const std::string & xmlTreeName );

	/** Check that the user is registered and we are in a valid state for user access. */
	bool ValidateUserAccess( const UObject & user );


	/** The set of registered users and their associated xml tree names, with a mapping to the associated xml tree names. */
	TMap<const UObject *, std::string> registeredUsers;

	/** The set of users that have not yet opened and closed their frames during this tick. This is filled with all registered users during openNetworkFrame()
	 *  once a new xml document has been successfully read, and gradually cleared by CloseFrame(). */
	TSet<const UObject *> nonfinishedUsers;





	/* RemoteControlHub integration */
	

private:

	/** Connect with a remote controller by accepting a remote control socket. Also creates the necessary response trees for each user. */
	void ConnectWith( std::unique_ptr<XmlFSocket> socket );

	/** The name by which remote controllers can contact this component via a RemoteControlHub. */
	UPROPERTY( EditAnywhere, Category = "RemoteControlSystem" )
	FString NetworkName;




	/* internal */


private:

	/** State machine representation of the state of our internal network communications frame for the ongoing tick. */
	class enum NetworkFrameState
	{
		/**
		 * We do not have an operational connection. Either the remote has not connected yet, or an established connection has been lost.
		 * While in this state, all calls to OpenFrame will keep returning null handles until the next tick, even if we get a new connection socket.
		 * 
		 * A new connection can arrive via ConnectWith() at any moment. However, we transition out from this state only in TickComponent (which runs before our
		 * users' tick methods, due to the prerequisites set during registration). This way we force all users to step in to the synchronized communication
		 * loop during the same tick.
		 * 
		 * Also, we transition into this state only as a result of a network error, and because there is network activity only during the first OpenFrame()
		 * call and the last CloseFrame() call for a given tick, we cannot transition into this state between these calls.
		 *
		 *   Invariant: We cannot transition into or out from the NoConnection state in the middle of user activity for a tick
		 *              (ie, between the first call to OpenFrame() and the last call to CloseFrame() for a tick)
		 * 
		 * Events and transitions:
		 *		TickComponent(): If we have a connection (IsGood() state is irrelevant), then transition to Closed
		 */
		NoConnection,

		/**
		 * We have a connection. The network frame for the previous tick has been already closed (or did not exist if we are in the first tick), but the network
		 * frame for the ongoing tick has not yet been opened.
		 *
		 * Invariant: There are no non-finished users
		 *            nonfinishedUsers.Num() == 0
		 * 
		 * Events and transitions:
		 *		TickComponent(): no transition
		 *		A user calls OpenFrame(): call openNetworkFrame()
		 *			On success, transition to Open
		 *			On failure, transition to NoConnection
		 *		A user calls CloseFrame(): no transition (log an error)
		 */
		Closed,

		/**
		 * The network frame for the ongoing tick has been successfully opened, but not yet closed. InXml and OutXml are in valid and initialized state.
		 * 
		 * Invariant: remoteControlSocket.InXml contains a successfully received XML document (it might or might not contain entries for all users).
		 *            remoteControlSocket.InXmlStatus == true.
		 *            remoteControlSocket.OutXml contains an initialized XML document (it might contain writes from users)
		 *            
		 * Invariant: There are non-finished users
		 *            nonfinishedUsers.Num() > 0
		 * 
		 * Events and transitions:
		 *		TickComponent(): clear nonfinishedUsers and log an error, call closeNetworkFrame(), then transition to Closed
		 *		A user calls OpenFrame(): no transition
		 *		A user calls CloseFrame(): remove the user from nonfinishedUsers
		 *			If not last user: no transition
		 *			If last user (nonfinishedUsers.Num() becomes 0): call closeNetworkFrame()
		 *				On success, transition to Closed
		 *				On failure, transition to NoConnection
		 */
		Open,
	} networkFrameState;


	/** Read in a new command xml document and mark all users as non-finished. Checks that the previous tick was finalized and finalizes it if necessary. */
	void openNetworkFrame();

	/** Send back the response xml document. */
	void closeNetworkFrame();

	/** Handle a network error: drop the connection and log the event. */
	void handleNetworkError( const std::string & description );


	/** The remote control socket */
	std::unique_ptr<XmlFSocket> remoteControlSocket;

};
