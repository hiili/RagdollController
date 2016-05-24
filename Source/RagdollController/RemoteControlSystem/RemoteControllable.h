// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/SceneComponent.h"

#include "RemoteControllableHelper.h"
#include "RemoteControlHub.h"

#include <string>
#include <functional>

#include "RemoteControllable.generated.h"




/** Operations of a communication schedule of a RemoteControllable. Conceptually nested within FRemoteControllableSchedule (UHT disallows nested classes).
 *  @see URemoteControllable, FRemoteControllableSchedule */
UENUM()
enum class ERemoteControllableScheduleOperations : uint8
{
	/** Receive an inbound XML document (block until received), then call all receive callbacks. */
	Receive,

	/** Call all send callbacks, then send the constructed XML document. */
	Send,

	/** Let the tick methods of the users run at this point. There must be at most one YieldToUsers operation in the schedule for each engine tick. */
	YieldToUsers,

	/** Yield control until next engine tick. This operation contains an implicit YieldToUsers operation, in case that there has not been an explicit one for
	 *  the ongoing engine tick. */
	Yield,
};


/** A communication schedule for a RemoteControllable. Conceptually nested within RemoteControllable (UHT disallows nested classes).
 *  @see URemoteControllable, ERemoteControllableScheduleOperations */
USTRUCT()
struct FRemoteControllableSchedule
{
	GENERATED_BODY()

	/** The schedule by which communication operations take place. Various combinations can be used to achieve different useful communication patterns.
	 *  Usage examples:
	 *  
	 *  Schedule = { Send, Receive }, YieldsBeforeScheduleRestart = 1:
	 *		On each tick, send a state report, which the remote can then use to produce a command document. Block until the command document has
	 *		been received. Perform both operations before the users' tick method runs.
	 *		Note that the UE game thread will block for the whole time that it takes to complete the send-receive loop.
	 *	
	 *  Schedule = { Receive, YieldToUsers, Send }, YieldsBeforeScheduleRestart = 1:
	 *		On each tick, block until a command document has been received. Let the user's tick method run. Then send back a state report, which the remote can
	 *		then use to produce a command document for the next tick. This gives one tick's worth of time for the remote and the network to complete the loop
	 *		before starting to stall the game thread, with the price of introducing a delay of one tick to the control signal.
	 *
	 *  Schedule = { YieldToUsers, Send, Yield, Receive }, YieldsBeforeScheduleRestart = 5:
	 *		On engine tick n, let the users' tick methods run, then send a state report. The remote can use this report to produce a command document for the
	 *		next engine tick. Proceed to the next engine tick (n+1) and block until the command document has been received. The users' tick methods will run
	 *		after this receive operation. Then skip 5 engine ticks and restart the schedule (restart on n+6). The control loop will have one tick's worth of
	 *		time to complete before stalling the game thread, there will be a delay of one tick in the control signal, and the control frequency is 1/6 * fps
	 *		(10Hz, assuming a 60Hz tick rate). */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "RemoteControlSystem" )
	TArray<ERemoteControllableScheduleOperations> Schedule;

	/** How many times to repeatedly yield control between finishing the communication schedule and restarting it. After a yield, control is re-gained via the
	 *  next TickComponent() invocation. Consequently, the total number of ticks a full communication cycle takes is the number of explicit Yield operations in
	 *  the schedule + YieldsBeforeScheduleRestart. This field is simply for convenience; the same effect could be achieved by adding Yield commands to the end
	 *  of the schedule.
	 *  
	 *  There has to be at least one yield invocation during a cycle, either in the schedule or set here. Otherwise the game thread will block permanently. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "RemoteControlSystem", meta = (ClampMin = "0", UIMin = "0", UIMax = "60") )
	int32 YieldsBeforeScheduleRestart = 1;   // UBT has no uint32


	/** Verify that the schedule contains at least one Yield operation and that YieldsBeforeScheduleRestart >= 0. */
	bool verifySchedule();

	/** Index to the effective schedule that combines both the explicit schedule and the implicit Yield commands at its end. */
	class Index {
	public:
		explicit Index( const FRemoteControllableSchedule & schedule_ );

		/** Restart the schedule from the beginning. */
		void restart();

		/** Get the current index position. */
		operator int() const;

		/** Get the operation at the current index. */
		ERemoteControllableScheduleOperations operator*() const;

		/** Advance the schedule, restart if at end. */
		Index operator++(int);

		/** Back step the schedule, go to end if at beginning. */
		Index operator--(int);

	private:
		const FRemoteControllableSchedule * schedule = nullptr;
		int32 index = 0;

	public:
		/** Don't use! Needed for UE CDOs. */
		Index();
	};
};





/**
 * An actor component for communicating with a remote controller over a TCP socket using XML. The connection is initiated by the remote controller by contacting
 * the RemoteControlHub actor. The remote can then request a connection dispatch to a RemoteControllable component based on the component's NetworkName (the
 * NetworkName can be set via the editor).
 * 
 * On the UE side, all users of the control link (components, actors, ...) must initially register with the RemoteControllable using the RegisterUser() method.
 * Users provide two callbacks during this registration: one for receiving a network communication frame and one for filling out and sending a network
 * communication frame. The order and frequency by which the callbacks will be called (send-receive-tick, receive-tick-send, receive-tick-send-tick*n, ...)
 * depends on the CommunicationSchedule field of the RemoteControllable object.
 * 
 * Each inbound XML document should have a single root element that contains a child element for each registered user. The name of these elements should match
 * the xmlTreeName that each user specified during registration. Each outbound XML document will contain a single root element, under which a child element is
 * placed for every registered user, again with names matching the xmlTreeName strings of each user.
 *
 * The schedule starts to run from the beginning once a connection has been established. Exactly one XML document will be read from the remote controller for
 * each Receive operation. The game thread blocks with no timeout until the document has been received. Exactly one XML document will be sent back for each Send
 * operation. No data is read or sent and no callbacks are called during consecutive Yield operations.
 * 
 * The remote controller can close the connection by simply closing the TCP socket; EOF (socket shutdown) or any network error will cause potential blocking
 * reads to cancel and the schedule to stop running. A dropped and re-established connection will cause the schedule to restart from the beginning. Opening a
 * new connection to a RemoteControllable while a connection already exists will cause the old connection to be dropped and replaced with the new one (the
 * schedule restarts also in this case).
 * 
 * @see RemoteControlHub, XmlFSocket, Mbml
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RAGDOLLCONTROLLER_API URemoteControllable : public USceneComponent
{
	/*
	 * Why don't we inherit from an UActorComponent? We create the post-user-tick helper component as our default subcomponent, so as to not pollute the
	 * component space of the owning actor.
	 *
	 *
	 * Inversion of control:
	 *
	 * The RemoteControllable class uses callbacks to communicate with users, which results in inversion of control. Reasons for this design choice:
	 *   - We might want special communication schedules, so as to minimize stalls of the game thread and to limit the communication rate.
	 *   - We might have several users that all communicate through the same socket and XML documents.
	 *   - This requires coordination: all users must adhere to the same communication schedule. Leaving control to the users would also leave them the burden
	 *     of keeping track of the communication schedule.
	 */
	GENERATED_BODY()

	/** Permit the remote control hub to have private access, so that it can connect the remote controller to us.
	 *  Members intended to be accessed: ConnectWith() */
	friend class ARemoteControlHub;




	/* constructors/destructor */


public:

	// Sets default values for this component's properties
	URemoteControllable();




	/* UE interface overrides and ticking */


public:

	// Called when the game starts
	virtual void BeginPlay() override;

	/** Called every frame. Advances the schedule until the next YieldToUsers or Yield operation. The component's blueprint is run first. Post-user-tick
	 *  operations are run via the postUserTick() method, which is triggered by our RemoteControllableHelper subcomponent. */
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

	/** Called every frame by our RemoteControllableHelper subcomponent after our users have ticked. Advances the schedule until the next Yield operation. */
	void postUserTick();




	/* communication schedule */


public:

	/** The schedule by which communication operations take place. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "RemoteControlSystem" )
	FRemoteControllableSchedule CommunicationSchedule;

	/** Return the index of the position in the schedule that will be executed next. Yields originating from YieldsBeforeScheduleRestart are treated implicitly
	 *  as residing at the end of the explicit schedule. Consequently, the returned index has the range [0,n), where n = CommunicationSchedule.Schedule.Num() +
	 *  CommunicationSchedule.YieldsBeforeScheduleRestart. */
	UFUNCTION( BlueprintCallable, Category = "RemoteControlSystem" )
	int32 GetNextOperationIndex() const;


private:

	/** Current index position in the effective schedule that combines both the explicit schedule and the implicit Yield commands at its end. */
	FRemoteControllableSchedule::Index scheduleIndex;




	/* user registration */


public:

	/** Type of the communication callback functions. */
	typedef std::function<void( pugi::xml_node )> CommunicationCallback_t;

	/** Register a new user of the control link.
	 *  
	 *  @param user					A reference to the user, which must be either an Actor or ActorComponent object (see overloads). This reference is used to
	 *								enforce the timing of pre-user-tick and post-user-tick operations in the schedule. It is also used for detecting if the user
	 *								has been destroyed, in which case the associated user entry is removed from the user list and the associated callbacks will
	 *								not be called anymore.
	 *  @param	xmlTreeName			The name of the subtree in the command and response documents that the user wishes to use. This must be unique among all
	 *								users of this RemoteControllable component.
	 *  @param receiveCallback		Called when it is time to process the next inbound XML document. The argument of the callback is a handle to the XML tree
	 *								in the inbound XML document whose name matches the user's xmlTreeName, or a null handle if no such tree was found. The
	 *								callback function can be an empty std::function.
	 *  @param sendCallback			Called when it is time to generate the next outbound XML document. The argument of the callback is a handle to an XML tree
	 *								in the outbound XML document whose name has been set to the user's xmlTreeName. This tree is not cleared between ticks,
	 *								which permits users to create their response structure only once and then just overwrite values on each subsequent tick. The
	 *								sendCallback function is never called with a null handle. The callback function can be an empty std::function.
	 *  @return						True on success, false otherwise. The call can fail for various reasons, including: the user is trying to register multiple
	 *								times, the user is of wrong type, or the xmlTreeName is already taken. */
	bool RegisterUser( AActor & user, const std::string & xmlTreeName,
		const CommunicationCallback_t & receiveCallback, const CommunicationCallback_t & sendCallback );
	bool RegisterUser( UActorComponent & user, const std::string & xmlTreeName,
		const CommunicationCallback_t & receiveCallback, const CommunicationCallback_t & sendCallback );

	/** Unregister a user of the control link. Callbacks associated with this user will not be called anymore.
	 *
	 *	@param user		A reference to the user that was passed earlier to RegisterUser().
	 *	@return			True on success, false if the provided user is not a registered user of this RemoreControllable. */
	bool UnregisterUser( AActor & user );
	bool UnregisterUser( UActorComponent & user );


private:

	/** Add tick prerequisites, so that we tick before the user and our post-user-tick helper subcomponent ticks after it. Return true on success. */
	template< typename ObjectType, typename void (UActorComponent::*AddTickPrerequisiteMethod)(ObjectType *) >
	bool enforceTimings( ObjectType & user );

	/** Remove tick prerequisites. Return true on success. */
	template< typename ObjectType, typename void (UActorComponent::*RemoveTickPrerequisiteMethod)(ObjectType *) >
	bool removeTimings( ObjectType & user );


	/** Store a new user to the registeredUsers array while enforcing uniqueness constraints. Return true on success. */
	bool storeNewUser( UObject & user, const std::string & xmlTreeName,
		const std::function<void( pugi::xml_node )> & receiveCallback, const std::function<void( pugi::xml_node )> & sendCallback );

	/** Remove a user from the registeredUsers array. Return true on success. */
	bool removeUser( UObject & user );

	/** Prune all stale users from the registeredUsers array. */
	void pruneStaleUsers();


	/** Find our RemoteControllableHelper component. Log and return nullptr on failure. */
	URemoteControllableHelper * findRemoteControllableHelper();


	/** A registered user entry. */
	struct RegisteredUser
	{
		/** A pointer to the user object. Must be unique within this component. Will become null if the object is destroyed (set by UE). */
		TWeakObjectPtr<UObject> user;

		/** The xmlTreeName of this user. Must be unique within this component. */
		std::string xmlTreeName;

		/** Callback function for receiving data. Can be an empty function. */
		CommunicationCallback_t receiveCallback;

		/** Callback function for sending data. Can be an empty function. */
		CommunicationCallback_t sendCallback;
	};

	/** The set of registered users. Uniqueness is enforced in storeNewUser(). */
	TArray<RegisteredUser> registeredUsers;




	/* network functionality */
	

public:

	/** The name by which remote controllers can contact this component via a RemoteControlHub. */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "RemoteControlSystem" )
	FString NetworkName;


private:

	/** Connect with a remote controller by accepting a remote control socket. The current connection is dropped, if one exists. The schedule is restarted. */
	void ConnectWith( std::unique_ptr<XmlFSocket> socket );

	/** Test whether we have an operational connection with a remote controller. If true, then all registered callbacks are being called according to the
	 *  selected schedule and timings. */
	bool IsConnectedAndGood() const;

	/** Handle a network error: drop the connection and log the event. */
	void handleNetworkError( const std::string & description );

	/** The remote control socket */
	std::unique_ptr<XmlFSocket> remoteControlSocket;




	/* schedule operations and helpers */


private:

	/** If IsConnectedAndGood(), then advance the schedule up to the next YieldToUsers or Yield operation. */
	void advanceSchedule( bool usersHaveTicked );

	/** Perform a receive operation with all registered users: receive exactly one xml document from the remote, then call the receive callback of every
	 *  registered user with the user's xml element. Return true on success. */
	bool receive();

	/** Perform a send operation with all registered users: call the send callback of every registered user, then send the accumulated xml document to the
	 *  remote. Return true on success. */
	bool send();
};
