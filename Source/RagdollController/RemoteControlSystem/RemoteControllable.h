// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"

#include "RemoteControlHub.h"

#include <string>

#include "RemoteControllable.generated.h"




/**
 * An actor component for communicating with a remote controller over a TCP socket using XML. The connection is initiated by the remote controller by
 * contacting the RemoteControlHub actor. The remote can then request a connection dispatch to a RemoteControllable component based on the component's
 * NetworkName (the NetworkName can be set via the editor).
 * 
 * On the UE side, all users of the control link (components, actors, ...) must initially register with the RemoteControllable using the RegisterUser() method.
 * This must happen before the remote opens the connection; trying to register once a connection is already open is an error (will be rejected and logged).
 * 
 * Users can probe whether we have an operational connection and new data by calling HasConnectionAndValidData(). Once connected and as long as the connection
 * is operational (HasConnectionAndValidData() returns true), all communication will take place synchronously.
 * 
 * All registered users should perform the following steps during each tick:
 *   1. Call HasConnectionAndValidData(). Continue if it returns true, otherwise stop.
 *   2. Call OpenFrame(), which returns a UserFrame with two handles: one to a command XML tree and one to a response XML tree.
 *   3. If the UserFrame contains non-null handles, then read data from the command tree and write data to the response tree.
 *   4. Call CloseFrame(), even if OpenFrame() returned null handles.
 * 
 * Once a connection is established, the RemoteControllable will read exactly one XML document from the remote controller per tick. This happens before any of
 * the registered users' tick methods get called.
 * 
 * The response XML document is sent to the remote immediately after the last registered user has called CloseFrame(). The response document is not cleared
 * between ticks: each user continues with their response tree from the previous tick. This way the user does not need to re-create the tree layout on each tick
 * and typically needs to only overwrite node data.
 * 
 * @see RemoteControlHub, XmlFSocket, Mbml
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RAGDOLLCONTROLLER_API URemoteControllable : public UActorComponent
{
	/* Implementation details:
	 * 
	 * Upon a connection request, the hub actor calls the ConnectWith() method, which in turn stores the connection socket in the RemoteControlSocket field.
	 * 
	 * We could well send the response document during the next TickComponent() call. However, we try to send it as soon as possible, so as to leave more time
	 * for the network and the remote before we try to read in the next inbound command document (we do a blocking read at that point).
	 **/

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
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;




	/* The remote control interface */


public:

	/** Register a new user of the control link. This must be called before the remote controller opens the connection; trying to register after that will fail.
	 *  Once registered, the user is responsible to probe HasConnectionAndValidData() on each tick, and if it return true, then call OpenFrame() followed by a
	 *  call to CloseFrame() exactly once per each tick. There is no unregister method at the moment; registration is for lifetime. Registration will mark the
	 *  RemoteControllable component as a tick prerequisite for the user.
	 *  
	 *  @param	user			A pointer to the user, which must be an AActor (an overload exists for UActorComponent users).
	 *  @param	xmlTreeName		The name of the subtree in the command and response documents that the user wishes to use. This must be unique among all users
	 *							of this RemoteControllable component. @see OpenFrame()
	 *  @return					True on success, false otherwise. The call can fail for various reasons, including: the user is trying to register multiple
	 *							times, the xmlTreeName is already taken, or the control link is already open. */
	bool RegisterUser( AActor & user, const std::string & xmlTreeName );

	/** Registration method for UActorComponent users. See the AActor overload for documentation. */
	bool RegisterUser( UActorComponent & user, const std::string & xmlTreeName );


	/** Data struct that represents a specific user's portion of the current tick's network communication frame. */
	struct UserFrame
	{
		/** A handle to the command element. Modifications to this element are allowed but will be discarded when the frame is closed. Can be a null handle. */
		pugi::xml_node command;

		/** A handle to the response element. This element will be sent back to the remote once the frame is closed. Can be a null handle. */
		pugi::xml_node response;

		/** Conversion to boolean for easy validity testing. */
		explicit operator bool() const { return command && response; }
	};

	/** Test whether we have an operational connection with a remote controller, so that it is now safe to call OpenFrame(). */
	bool HasConnectionAndValidData() const;

	/** Opens the command and response XML trees corresponding to the current tick. Each user has provided a tree name during registration (xmlTreeName).
	 *  The root element of the command XML document should contain a child element with a matching name. If found, then a handle to this element will be
	 *  returned. The name of the root element is ignored. The response XML document will contain a single root element, under which a child element is placed
	 *  for every registered user. The names of these child elements equal the xmlTreeNames of the users. A handle to the calling user's response
	 *  element will be returned. Note that the response elements are not cleared between ticks.
	 *  
	 *  You should use HasConnectionAndValidData() to probe whether a connection has been established, as probing with OpenFrame() would flood the error log.
	 *  
	 *  @param	user			A pointer to the user.
	 *  @return					A UserFrame struct containing the command and response element handles. If the user's element is missing from the command XML
	 *							document, then both handles will be null handles. */
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

	/** The set of users that have not yet opened and closed their frames during this tick. This is filled with all registered users during prepareFrame()
	 *  once a new xml document has been successfully read, and gradually cleared by CloseFrame(). */
	TSet<const UObject *> nonfinishedUsers;





	/* RemoteControlHub integration */
	

private:

	/** Connect with a remote controller by accepting a remote control socket. Also creates the necessary response trees for each user. */
	void ConnectWith( std::unique_ptr<XmlFSocket> socket );


	/** The name by which remote controllers can contact this component via a RemoteControlHub. */
	UPROPERTY( EditAnywhere, Category = "RemoteControlSystem" )
	FString NetworkName;

	/** Remote control socket */
	std::unique_ptr<XmlFSocket> remoteControlSocket;




	/* internal operations */


private:

	/** Read in a new command xml document and marks all users as nonfinished. Called before any users get a chance to access our data during this tick;
	 ** this is ensured by the tick prerequisites that are added during each user registration. Checks that the previous tick was finalized and finalizes
	 ** it if necessary. */
	void prepareFrame();

	/** Send back the response xml document. */
	void finalizeFrame();

	/** Handle a network error: drop the connection and log the event. */
	void handleNetworkError( const std::string & description );

};
