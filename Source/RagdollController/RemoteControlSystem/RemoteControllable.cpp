// Fill out your copyright notice in the Description page of Project Settings.

/**
 * Implementation details:
 * 
 * Upon a connection request, the hub actor calls the ConnectWith() method, which in turn stores the connection socket in the RemoteControlSocket field.
 */


#include "RagdollController.h"
#include "RemoteControllable.h"

#include "Utility.h"


DECLARE_LOG_CATEGORY_EXTERN( LogRemoteControlSystem, Log, All );




// Sets default values for this component's properties
URemoteControllable::URemoteControllable() :
	scheduleIndex( CommunicationSchedule )
{
	// Set this component to be initialized when the game starts and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsBeginPlay = true;
	PrimaryComponentTick.bCanEverTick = true;

	// set the default schedule (TArray doesn't yet support list initialization)
	CommunicationSchedule.Schedule.Empty( 2 );
	CommunicationSchedule.Schedule.Add( ERemoteControllableScheduleOperations::Receive );
	CommunicationSchedule.Schedule.Add( ERemoteControllableScheduleOperations::Send );

	// create and attach a subcomponent for triggering post-user-tick comms operations
	URemoteControllableHelper * helper = CreateDefaultSubobject<URemoteControllableHelper>( TEXT( "PostUserTickHelper" ) );
	if( helper )
	{
		helper->AttachTo( this );
	}
	else
	{
		UE_LOG( LogRemoteControlSystem, Error,
			TEXT( "(%s) Failed to create a RemoteControllableHelper subcomponent! Post-user-tick comms operations will not run." ), TEXT( __FUNCTION__ ) );
	}
}




// Called when the game starts
void URemoteControllable::BeginPlay()
{
	Super::BeginPlay();

	// Make sure that RemoteControlHub ticks before we do, so that the schedule always starts predictably on the same tick as when the CONNECT command arrives.
	// This will become more useful once the RemoteControlHub supports delaying and connecting several connections in sync on a given tick.
	// (tolerate multiple hubs here, although there shouldn't be more than one)
	for( auto hub : Utility::FindActorsByClass<ARemoteControlHub>( *this ) )
	{
		AddTickPrerequisiteActor( hub );
		UE_LOG( LogRemoteControlSystem, Verbose, TEXT( "(%s) %s.AddTickPrerequisiteActor( %s )" ), TEXT( __FUNCTION__ ), *Utility::GetName( this ), *Utility::GetName( hub ) );
	}
}


// Called every frame
void URemoteControllable::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	// run the component's blueprint, if any
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	advanceSchedule( /*usersHaveTicked =*/ false );
}


void URemoteControllable::postUserTick()
{
	advanceSchedule( /*usersHaveTicked =*/ true );
}








FRemoteControllableSchedule::Index::Index() :
	schedule{}, index{}
{}


FRemoteControllableSchedule::Index::Index( const FRemoteControllableSchedule & schedule_ ) :
	schedule{ &schedule_ }
{}


void FRemoteControllableSchedule::Index::restart()
{
	index = 0;
}


FRemoteControllableSchedule::Index::operator int() const
{
	return index;
}


ERemoteControllableScheduleOperations FRemoteControllableSchedule::Index::operator*() const
{
	check( schedule );

	if( index < schedule->Schedule.Num() )
	{
		// we are within the explicit schedule: read and return the pointed op
		return schedule->Schedule[index];
	}
	else
	{
		// we are beyond the explicit schedule (or the schedule is empty): return Yield
		return ERemoteControllableScheduleOperations::Yield;
	}
}


FRemoteControllableSchedule::Index FRemoteControllableSchedule::Index::operator++(int)
{
	check( schedule );

	Index retval( *this );
	index = (index + 1) % (schedule->Schedule.Num() + schedule->YieldsBeforeScheduleRestart);
	return retval;
}


FRemoteControllableSchedule::Index FRemoteControllableSchedule::Index::operator--(int)
{
	check( schedule );

	Index retval( *this );
	if( index > 0 ) --index; else index = schedule->Schedule.Num() + schedule->YieldsBeforeScheduleRestart - 1;
	return retval;
}


bool FRemoteControllableSchedule::verifySchedule()
{
	check(!( YieldsBeforeScheduleRestart < 0 ));   // should never be negative (UBT has no uint32)
	if( YieldsBeforeScheduleRestart > 0 ) return true;   // have implicit Yield(s) -> satisfied

	// need to find at least one explicit Yield
	for( auto elem : Schedule ) if( elem == ERemoteControllableScheduleOperations::Yield ) return true;

	// invalid schedule
	return false;
}


int32 URemoteControllable::GetNextOperationIndex() const
{
	return scheduleIndex;
}








bool URemoteControllable::RegisterUser( AActor & user, const std::string & xmlTreeName,
	const CommunicationCallback_t & receiveCallback, const CommunicationCallback_t & sendCallback )
{
	return
		enforceTimings<AActor, &UActorComponent::AddTickPrerequisiteActor>( user ) &&
		storeNewUser( user, xmlTreeName, receiveCallback, sendCallback );
}

bool URemoteControllable::RegisterUser( UActorComponent & user, const std::string & xmlTreeName,
	const CommunicationCallback_t & receiveCallback, const CommunicationCallback_t & sendCallback )
{
	return
		enforceTimings<UActorComponent, &UActorComponent::AddTickPrerequisiteComponent>( user ) &&
		storeNewUser( user, xmlTreeName, receiveCallback, sendCallback );
}


bool URemoteControllable::UnregisterUser( AActor & user )
{
	return removeUser( user ) && removeTimings<AActor, &UActorComponent::RemoveTickPrerequisiteActor>( user );
}

bool URemoteControllable::UnregisterUser( UActorComponent & user )
{
	return removeUser( user ) && removeTimings<UActorComponent, &UActorComponent::RemoveTickPrerequisiteComponent>( user );
}




template< typename ObjectType, typename void (UActorComponent::*AddTickPrerequisiteMethod)(ObjectType *) >
bool URemoteControllable::enforceTimings( ObjectType & user )
{
	// obtain our RemoteControllableHelper
	URemoteControllableHelper * helper = findRemoteControllableHelper();
	if( !helper ) return false;

	// make sure that we tick before the user
	user.AddTickPrerequisiteComponent( this );

	// make sure that our helper component ticks after the user
	(helper->*AddTickPrerequisiteMethod)(&user);

	// success
	return true;
}


template< typename ObjectType, typename void (UActorComponent::*RemoveTickPrerequisiteMethod)(ObjectType *) >
bool URemoteControllable::removeTimings( ObjectType & user )
{
	// obtain our RemoteControllableHelper
	URemoteControllableHelper * helper = findRemoteControllableHelper();
	if( !helper ) return false;

	// remove prerequisite: make sure that we tick before the user
	user.RemoveTickPrerequisiteComponent( this );

	// remove prerequisite: make sure that our helper component ticks after the user
	(helper->*RemoveTickPrerequisiteMethod)(&user);

	// success
	return true;
}




bool URemoteControllable::storeNewUser( UObject & user, const std::string & xmlTreeName,
	const std::function<void( pugi::xml_node )> & receiveCallback, const std::function<void( pugi::xml_node )> & sendCallback )
{
	// check that both the user id and the tree name are unique
	for( const RegisteredUser & elem : registeredUsers )
	{
		if( elem.user == &user || elem.xmlTreeName == xmlTreeName )
		{
			UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s, %s) Registration with an already registered user pointer or xmlTreeName attempted! Ignoring: %s, xmlTreeName=%s" ),
				TEXT( "URemoteControllable::RegisterUser" ), *GetPathName( GetWorld() ), *user.GetPathName( user.GetWorld() ), *FString( xmlTreeName.c_str() ) );
			return false;
		}
	}

	// store it
	registeredUsers.Emplace( RegisteredUser{ &user, xmlTreeName, receiveCallback, sendCallback } );

	// success: log and return true
	UE_LOG( LogRemoteControlSystem, Log, TEXT( "(%s, %s) New user successfully registered: %s, xmlTreeName=%s" ),
		TEXT( "URemoteControllable::RegisterUser" ), *GetPathName( GetWorld() ), *user.GetPathName( user.GetWorld() ), *FString( xmlTreeName.c_str() ) );
	return true;
}


bool URemoteControllable::removeUser( UObject & user )
{
	// promote 'user' to weak pointer; UE would do this promotion implicitly and repeatedly for each raw pointer comparison (as of UE 4.9)
	TWeakObjectPtr<UObject> userWptr( &user );

	// find it
	auto ind = registeredUsers.IndexOfByPredicate( [&userWptr]( const RegisteredUser & candidateUser ){
		return candidateUser.user == userWptr;
	} );

	if( ind != INDEX_NONE )
	{
		// found: remove from 'registeredUsers' and return success
		registeredUsers.RemoveAtSwap( ind );
		return true;
	}
	else
	{
		// not found: return failure
		return false;
	}
}


void URemoteControllable::pruneStaleUsers()
{
	// do not use RemoveAllSwap(): it could create heisenbugs if the client code unintentionally depends on the callback order
	registeredUsers.RemoveAll( []( const RegisteredUser & registeredUser ){
		return !registeredUser.user.IsValid();
	} );
}




URemoteControllableHelper * URemoteControllable::findRemoteControllableHelper()
{
	URemoteControllableHelper * helper = nullptr;

	// try to find it
	for( auto subcomponent : AttachChildren )
	{
		helper = dynamic_cast<URemoteControllableHelper *>(subcomponent);
		if( helper ) break;
	}

	// log if not found
	if( !helper )
	{
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s, %s) Unable to find our RemoteControllableHelper subcomponent! Cannot (un)register users." ),
			TEXT( __FUNCTION__ ), *GetPathName( GetWorld() ) );
	}

	return helper;
}








void URemoteControllable::ConnectWith( std::unique_ptr<XmlFSocket> socket )
{
	// nullptr? -> error
	if( !socket )
	{
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) The socket pointer is null!" ), TEXT( __FUNCTION__ ) );
		return;
	}


	// dropping an existing connection? log
	if( remoteControlSocket )
	{
		UE_LOG( LogRemoteControlSystem, Warning, TEXT( "(%s) Dropping a connection so as to make room for a new incoming connection. Target component: %s" ),
			TEXT( __FUNCTION__ ), *GetPathName( GetWorld() ) );
	}

	// store the new socket
	remoteControlSocket = std::move( socket );

	// enforce synchronous mode: block with no timeout. we really want that data on each tick. (note that if you disable blocking, then EOF won't be detected
	// anymore due to a UE limitation; see XmlFSocket::IsGood() for details)
	remoteControlSocket->SetBlocking( true );

	// restart the schedule
	scheduleIndex.restart();

	// log
	UE_LOG( LogRemoteControlSystem, Log, TEXT( "(%s) New remote controller connected. Target component: %s" ), TEXT( __FUNCTION__ ), *GetPathName( GetWorld() ) );
}




bool URemoteControllable::IsConnectedAndGood() const
{
	return remoteControlSocket && remoteControlSocket->IsGood();
}




void URemoteControllable::handleNetworkError( const std::string & description )
{
	// drop the connection
	remoteControlSocket.reset();

	// log
	UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s, %s) Remote controller connection failed: %s! Dropping the connection." ), TEXT( __FUNCTION__ ), *GetPathName( GetWorld() ), *FString( description.c_str() ) );
}








/** Note that this method is called twice per tick: first from our own Tick(), then from our helper's Tick(). */
void URemoteControllable::advanceSchedule( bool usersHaveTicked )
{
	auto dropConnection = [this]( const char * function, const char * reason )
	{
		remoteControlSocket.reset();
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s, %s) %s Aborting communications." ),
			ANSI_TO_TCHAR( function ), *Utility::GetName( this ), ANSI_TO_TCHAR( reason ) );
	};


	// only proceed if IsConnectedAndGood() and the schedule is valid (check on every tick so as to permit editing the schedule programmatically)
	if( !IsConnectedAndGood() ) return;
	if( !CommunicationSchedule.verifySchedule() )
	{
		// invalid schedule -> abort comms and log
		remoteControlSocket.reset();
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s, %s) There must be at least one Yield operation in the schedule, " )
			TEXT( "either an implicit or an explicit one! Aborting communications." ), TEXT( __FUNCTION__ ), *Utility::GetName( this ) );
		return;
	}

	// prune stale users
	pruneStaleUsers();


	// advance and run operations until we get to a YieldToUsers or Yield operation
	while( true )
	{
		// switch by next op and increment the schedule index
		switch( *scheduleIndex++ )
		{
		case ERemoteControllableScheduleOperations::Receive:
			// try to receive(), drop the connection on errors
			if( !receive() )
			{
				dropConnection( __FUNCTION__, "Receive operation failed!" );
				return;
			}
			break;
		case ERemoteControllableScheduleOperations::Send:
			// try to send(), drop the connection on errors
			if( !send() )
			{
				dropConnection( __FUNCTION__, "Send operation failed!" );
				return;
			}
			break;
		case ERemoteControllableScheduleOperations::YieldToUsers:
			if( !usersHaveTicked )
			{
				// pre-user-tick phase: let the users tick, then return here via our helper subcomponent's tick
				return;
			}
			else
			{
				// post-user-tick phase: illegal operation -> log and ignore
				UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s, %s) There must be at most one YieldToUsers operation in the schedule for each engine tick! " )
					TEXT( "Ignoring a repeated YieldToUsers op at schedule position %d." ), TEXT( __FUNCTION__ ), *GetPathName( GetWorld() ), scheduleIndex - 1 );
				break;
			}
		case ERemoteControllableScheduleOperations::Yield:
			if( !usersHaveTicked )
			{
				// pre-user-tick phase: there is no explicit YieldToUsers op for this engine tick in the schedule. we need to make sure that also our helper
				// subcomponent sees this Yield -> backstep, then let control flow to our helper subcomponent's tick and then back here
				scheduleIndex--;
				return;
			}
			else
			{
				// post-user-tick phase: simply yield
				return;
			}
		}
	}
}




bool URemoteControllable::receive()
{
	UE_LOG( LogRemoteControlSystem, Error, TEXT( "********************************** RECEIVE OP" ) );

	// no-op if we do not have a socket
	if( !remoteControlSocket ) return false;

	// read a single xml document from the socket
	if( !remoteControlSocket->GetXml() )
	{
		// failed to receive a valid xml document -> log and abort comms
		UE_LOG( LogRemoteControlSystem, Error,
			TEXT( "(%s, %s) Failed to read and parse an xml document from the socket! Reason: %s" ),
			TEXT( __FUNCTION__ ), *Utility::GetName( this ), *FString( remoteControlSocket->InXmlStatus.description() ) );
		return false;
	}

	// invariant: got a complete and valid xml document, which should now reside in remoteControlSocket->InXml
	check( remoteControlSocket->InXmlStatus );

	// find the root element of the received document (check name, ignore possible additional roots)
	pugi::xml_node root = remoteControlSocket->InXml.child( TCHAR_TO_ANSI( *NetworkName ) );
	if( !root )
	{
		UE_LOG( LogRemoteControlSystem, Error,
			TEXT( "(%s, %s) The received xml document did not contain a root element that matches our NetworkName! NetworkName: %s" ),
			TEXT( __FUNCTION__ ), *Utility::GetName( this ), *NetworkName );
		return false;
	}

	// loop through registered users and deliver the contents of InXml to them
	for( const RegisteredUser & registeredUser : registeredUsers )
	{
		// skip stale users (pruned elsewhere)
		if( !registeredUser.user.IsValid() ) continue;

		// find the user's xml tree
		pugi::xml_node node = root.child( registeredUser.xmlTreeName.c_str() );
		if( !node )
		{
			// no xml tree found -> log and continue with a null node
			UE_LOG( LogRemoteControlSystem, Warning,
				TEXT( "(%s, %s) The received xml document did not contain a node for a registered user! User: %s, xmlTreeName=%s" ),
				TEXT( __FUNCTION__ ), *Utility::GetName( this ),
				*Utility::GetName( registeredUser.user.Get() ), *FString( registeredUser.xmlTreeName.c_str() ) );
		}

		// call the receive callback, if not empty
		if( registeredUser.receiveCallback ) registeredUser.receiveCallback( node );
	}

	// no fatal errors (could have warnings) -> return success
	return true;
}




bool URemoteControllable::send()
{
	UE_LOG( LogRemoteControlSystem, Error, TEXT( "********************************** SEND OP" ) );

	// no-op if we do not have a socket
	if( !remoteControlSocket ) return false;

	// find our root element in OutXml
	pugi::xml_node root = remoteControlSocket->OutXml.first_child();
	if( !root )
	{
		// no root yet -> create one
		root = remoteControlSocket->OutXml.append_child( TCHAR_TO_ANSI( *NetworkName ) );
		if( !root )
		{
			// log and abort comms if unable to create new nodes (pugi rejected the name? OutXml is invalid?)
			UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s, %s) Failed to create the xml root element!" ), TEXT( __FUNCTION__ ), *Utility::GetName( this ) );
			return false;
		}
	}

	// loop through registered users and have them fill in their OutXml elements
	for( const RegisteredUser & registeredUser : registeredUsers )
	{
		// try to find the user's xml tree (might exist from previous send ops)
		pugi::xml_node node = root.child( registeredUser.xmlTreeName.c_str() );
		if( !node )
		{
			// no tree -> create one
			node = root.append_child( registeredUser.xmlTreeName.c_str() );
			if( !node )
			{
				// log and abort comms if unable to create new nodes (pugi rejected the name? OutXml is invalid?)
				UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s, %s) Failed to create a new xml node! User: %s, xmlTreeName=%s" ),
					TEXT( __FUNCTION__ ), *Utility::GetName( this ),
					*Utility::GetName( registeredUser.user.Get() ), *FString( registeredUser.xmlTreeName.c_str() ) );
				return false;
			}
		}

		// invariant: we should now have a valid node
		check( node );

		// call the send callback, if not empty
		if( registeredUser.sendCallback ) registeredUser.sendCallback( node );
	}

	// send OutXml
	if( !remoteControlSocket->PutXml() )
	{
		// failed to send the xml document -> log and abort comms
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s, %s) Failed to send the xml document to the socket!" ),
			TEXT( __FUNCTION__ ), *Utility::GetName( this ) );
		return false;
	}

	// no fatal errors (could have warnings) -> return success
	return true;
}








//void URemoteControllable::prepareFrame()
//{
//	// no-op if no remote controller
//	if( !remoteControlSocket ) return;
//
//	// did all users properly complete their open-close cycle during the previous tick? force-complete if not.
//	if( nonfinishedUsers.Num() > 0 )
//	{
//		// no: someone forgot to do open-close or just close
//
//		// log a warning 
//		UE_LOG( LogRemoteControlSystem, Warning, TEXT( "(%s) Some users did not perform a full OpenFrame()-CloseFrame() cycle during the previous tick! Culprit(s):" ), TEXT( __FUNCTION__ ) );
//		for( const UObject * user : nonfinishedUsers )
//		{
//			check( user );
//			UE_LOG( LogRemoteControlSystem, Warning, TEXT( "(%s)     %s (xmlTreeName=%s)" ), TEXT( __FUNCTION__ ), *user->GetPathName( user->GetWorld() ), *FString( registeredUsers[user].c_str() ) );
//		}
//
//		// finalize the communications for the previous tick
//		nonfinishedUsers.Reset();
//		finalizeFrame();
//	}
//
//	// we need to force-complete also if there are no users, because finalizeFrame() is called from CloseFrame(), which in turn is called by users.
//	// just make sure that we have valid data (ie, this is not the first call to prepareFrame() after a remote has connected)
//	if( HasConnectionAndValidData() && registeredUsers.Num() == 0 ) finalizeFrame();
//
//
//	// read data from socket (leave the response document intact)
//	if( !remoteControlSocket->GetXml() )
//	{
//		// read failed
//		handleNetworkError( "failed to read XML data from the socket (" + std::string( remoteControlSocket->InXmlStatus.description() ) + ")" );
//		return;
//	}
//
//	// mark all users as non-finished for this tick
//	for( auto & userRecord : registeredUsers )
//	{
//		nonfinishedUsers.Emplace( userRecord.Key );
//	}
//
//
//	// invariant: we should now have valid data (XmlFSocket::GetXml should have returned false if not)
//	check( HasConnectionAndValidData() );
//}
//
//
//
//
//void URemoteControllable::finalizeFrame()
//{
//	// invariant: we have a connection and valid inbound data available for this tick, otherwise we should not get here. We are called from CloseFrame():
//	//   - no remote: CloseFrame() should be no-op
//	//   - network error: prepareFrame() should have dropped the connection
//	//   - a new connection established during this tick but prepareFrame() has not been called yet: CloseFrame should be no-op due to no valid data available
//	check( HasConnectionAndValidData() );
//
//	// invariant: all users should have finished by now (this is enforced internally)
//	check( nonfinishedUsers.Num() == 0 );
//
//
//	// send OutXml
//	if( !remoteControlSocket->PutXml() ) handleNetworkError( "failed to send the XML response document" );
//}








//URemoteControllable::UserFrame URemoteControllable::OpenFrame( const UObject & user )
//{
//	// check the user and our own status
//	if( !ValidateUserAccess( user ) ) return UserFrame{};
//
//	// make sure that the user is still marked as non-finished
//	// (we do not explicitly check for multiple OpenFrame calls per tick; we just make sure that CloseFrame hasn't been called yet)
//	if( !nonfinishedUsers.Contains( &user ) )
//	{
//		// the user has already opened and closed the frame -> log and return failure
//		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) A repeated call was made! Each user must call OpenFrame exactly once per tick. User: %s" ), TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld() ) );
//		return UserFrame{};
//	}
//
//
//	// find the user's tree name
//	const std::string & treeName = registeredUsers[&user];
//
//	// find the user's trees from the xml documents
//	pugi::xml_node commandTree = remoteControlSocket->InXml.first_child().child( treeName.c_str() );
//	pugi::xml_node responseTree = remoteControlSocket->OutXml.first_child().child( treeName.c_str() );
//	check( responseTree );   // we created it ourselves so it should always exist
//
//	// check whether there was a command tree for the user
//	if( commandTree )
//	{
//		// yes, full success -> return the handles
//		return UserFrame{ commandTree, responseTree };
//	}
//	else
//	{
//		// no, there is no command tree for the user -> log and return null handles
//		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) The received command XML document does not contain an element for the user %s! Expected element name: %s" ),
//			TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld() ), *FString( treeName.c_str() ) );
//		return UserFrame{};
//	}
//}
//
//
//void URemoteControllable::CloseFrame( const UObject & user )
//{
//	// check the user and our own status
//	if( !ValidateUserAccess( user ) ) return;
//
//	// make sure that the user is still marked as non-finished (we do not explicitly check whether the user has ever called OpenFrame)
//	if( !nonfinishedUsers.Contains( &user ) )
//	{
//		// the user has already opened and closed the frame -> log and return failure
//		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Called although there is no frame open! User: %s" ), TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld() ) );
//		return;
//	}
//
//
//	// mark the user as finished
//	nonfinishedUsers.Remove( &user );
//
//	// was this the last user? send the document if yes
//	if( nonfinishedUsers.Num() == 0 )
//	{
//		finalizeFrame();
//	}
//}
//
//
//bool URemoteControllable::ValidateUserAccess( const UObject & user )
//{
//	// do we have valid data?
//	if( !HasConnectionAndValidData() )
//	{
//		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Data access attempted while we do not have a connection or valid data! User: %s" ), TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld() ) );
//		return false;
//	}
//
//	// is the user registered?
//	if( !registeredUsers.Contains( &user ) )
//	{
//		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Data access attempted by a non-registered user! User: %s" ), TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld() ) );
//		return false;
//	}
//
//	// all ok
//	return true;
//}







