// Fill out your copyright notice in the Description page of Project Settings.

/**
 * Implementation details
 * 
 * Upon a connection request, the hub actor calls the ConnectWith() method, which in turn stores the connection socket in the RemoteControlSocket field.
 * We do not accept connections if we do not have registered users.
 * 
 *   Invariant: If a connection exists, then we have at least one registered user.
 *              remoteControlSocket != nullptr -> registeredUsers.Num() > 0
 * 
 * We could well receive and send data during our TickComponent() call. However, we try to read data as late as possible and send data as soon as possible, so
 * as to leave more time for the network and the remote between receiving our data and having to send new data to us (we do a blocking read).
 */

#include "RagdollController.h"
#include "RemoteControllable.h"


DECLARE_LOG_CATEGORY_EXTERN( LogRemoteControlSystem, Log, All );




// Sets default values for this component's properties
URemoteControllable::URemoteControllable()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsBeginPlay = false;
	PrimaryComponentTick.bCanEverTick = true;
}


// BeginPlay() is disabled in constructor!
//// Called when the game starts
//void URemoteControllable::BeginPlay()
//{
//	Super::BeginPlay();
//
//}



// Called every frame
void URemoteControllable::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	// check for a new connection. transition from NoConnection to Closed if have one.
	if( networkFrameState == NetworkFrameState::NoConnection && remoteControlSocket ) networkFrameState = NetworkFrameState::Closed;

	// did all users properly complete their open-close cycle during the previous tick? force-complete if not.
	if( networkFrameState == NetworkFrameState::Open )
	{
		// no: someone forgot to do open-close or just close

		check( nonfinishedUsers.Num() > 0 );   // closing the frame for the last user should cause a transition out from Open

		// log a warning 
		UE_LOG( LogRemoteControlSystem, Warning, TEXT( "(%s) Some users did not perform a full OpenFrame()-CloseFrame() cycle during the previous tick! Culprit(s):" ), TEXT( __FUNCTION__ ) );
		for( const UObject * user : nonfinishedUsers )
		{
			check( user );
			UE_LOG( LogRemoteControlSystem, Warning, TEXT( "(%s)     %s (xmlTreeName=%s)" ), TEXT( __FUNCTION__ ), *user->GetPathName( user->GetWorld() ), *FString( registeredUsers[user].c_str() ) );
		}

		// finalize the communications for the previous tick
		nonfinishedUsers.Reset();
		closeNetworkFrame();
	}
}






void URemoteControllable::openNetworkFrame()
{
	// no-op if no remote controller
	if( !remoteControlSocket ) return;


	// read data from socket (leave the response document intact)
	if( !remoteControlSocket->GetXml() )
	{
		// read failed
		handleNetworkError( "failed to read XML data from the socket (" + std::string( remoteControlSocket->InXmlStatus.description() ) + ")" );
		return;
	}

	// mark all users as non-finished for this tick
	for( auto & userRecord : registeredUsers )
	{
		nonfinishedUsers.Emplace( userRecord.Key );
	}


	// invariant: we should now have valid data (XmlFSocket::GetXml should have returned false if not)
	check( HasConnectionAndValidData() );
}




void URemoteControllable::closeNetworkFrame()
{
	// invariant: we have a connection and valid inbound data available for this tick, otherwise we should not get here. We are called from CloseFrame():
	//   - no remote: CloseFrame() should be no-op
	//   - network error: openNetworkFrame() should have dropped the connection
	//   - a new connection established during this tick but openNetworkFrame() has not been called yet: CloseFrame should be no-op due to no valid data available
	check( HasConnectionAndValidData() );

	// invariant: all users should have finished by now (this is enforced internally)
	check( nonfinishedUsers.Num() == 0 );


	// send OutXml
	if( !remoteControlSocket->PutXml() ) handleNetworkError( "failed to send the XML response document" );
}




void URemoteControllable::handleNetworkError( const std::string & description )
{
	// drop the connection
	remoteControlSocket.reset();

	// mark the current xml document as invalid
	remoteControlSocket->InXmlStatus.status = pugi::status_no_document_element;

	// log
	UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s, %s) Remote controller connection failed: %s! Dropping the connection." ), TEXT( __FUNCTION__ ), *GetPathName( GetWorld() ), *FString( description.c_str() ) );
}








URemoteControllable::UserFrame URemoteControllable::OpenFrame( const UObject & user )
{
	// check the user and our own status
	if( !ValidateUserAccess( user ) ) return UserFrame{};

	// make sure that the user is still marked as non-finished
	// (we do not explicitly check for multiple OpenFrame calls per tick; we just make sure that CloseFrame hasn't been called yet)
	if( !nonfinishedUsers.Contains( &user ) )
	{
		// the user has already opened and closed the frame -> log and return failure
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) A repeated call was made! Each user must call OpenFrame exactly once per tick. User: %s" ), TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld() ) );
		return UserFrame{};
	}


	// find the user's tree name
	const std::string & treeName = registeredUsers[&user];

	// find the user's trees from the xml documents
	pugi::xml_node commandTree = remoteControlSocket->InXml.first_child().child( treeName.c_str() );
	pugi::xml_node responseTree = remoteControlSocket->OutXml.first_child().child( treeName.c_str() );
	check( responseTree );   // we created it ourselves so it should always exist

	// check whether there was a command tree for the user
	if( commandTree )
	{
		// yes, full success -> return the handles
		return UserFrame{ commandTree, responseTree };
	}
	else
	{
		// no, there is no command tree for the user -> log and return null handles
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) The received command XML document does not contain an element for the user %s! Expected element name: %s" ),
			TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld() ), *FString( treeName.c_str() ) );
		return UserFrame{};
	}
}


void URemoteControllable::CloseFrame( const UObject & user )
{
	// check the user and our own status
	if( !ValidateUserAccess( user ) ) return;

	// make sure that the user is still marked as non-finished (we do not explicitly check whether the user has ever called OpenFrame)
	if( !nonfinishedUsers.Contains( &user ) )
	{
		// the user has already opened and closed the frame -> log and return failure
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Called although there is no frame open! User: %s" ), TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld() ) );
		return;
	}


	// mark the user as finished
	nonfinishedUsers.Remove( &user );

	// was this the last user? send the document if yes
	if( nonfinishedUsers.Num() == 0 )
	{
		closeNetworkFrame();
	}
}


bool URemoteControllable::ValidateUserAccess( const UObject & user )
{
	// do we have valid data?
	if( !HasConnectionAndValidData() )
	{
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Data access attempted while we do not have a connection or valid data! User: %s" ), TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld() ) );
		return false;
	}

	// is the user registered?
	if( !registeredUsers.Contains( &user ) )
	{
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Data access attempted by a non-registered user! User: %s" ), TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld() ) );
		return false;
	}

	// all ok
	return true;
}







void URemoteControllable::ConnectWith( std::unique_ptr<XmlFSocket> socket )
{
	// nullptr? -> error
	if( !socket )
	{
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) The socket pointer is null!" ), TEXT( __FUNCTION__ ) );
		return;
	}

	// no registered users? -> log and drop the connection
	if( registeredUsers.Num() == 0 )
	{
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) No registered users! Rejecting the connection attempt." ), TEXT( __FUNCTION__ ) );
		return;
	}


	// store the new socket
	remoteControlSocket = std::move( socket );

	// enforce synchronous mode: block with no timeout. we really want that data on each tick.
	remoteControlSocket->SetBlocking( true );

	// initialize the response xml trees for each user
	pugi::xml_node root = remoteControlSocket->OutXml.append_child( TCHAR_TO_UTF8( *NetworkName ) );
	for( auto & elem : registeredUsers )
	{
		root.append_child( elem.Value.c_str() );
	}

	// log
	UE_LOG( LogRemoteControlSystem, Log, TEXT( "(%s) New remote controller connected. Target component: %s" ), TEXT( __FUNCTION__ ), *GetPathName( GetWorld() ) );
}


/** Test that we have a socket *and* valid data awaiting in it. The latter test is to deal with the case that if a remote controller connected between our
 *  tick and our users' ticks, in which case we would have a valid socket but no data yet. In that case, it is better to wait until the next tick, so that we
 *  have everything nicely in sync. */
bool URemoteControllable::HasConnectionAndValidData() const
{
	return remoteControlSocket && remoteControlSocket->IsGood() && remoteControlSocket->InXmlStatus;
}








bool URemoteControllable::RegisterUser( AActor & user, const std::string & xmlTreeName )
{
	return RegisterUser<AActor>( user, xmlTreeName );
}


bool URemoteControllable::RegisterUser( UActorComponent & user, const std::string & xmlTreeName )
{
	return RegisterUser<UActorComponent>( user, xmlTreeName );
}


template<typename UserType>
bool URemoteControllable::RegisterUser( UObject & user, const std::string & xmlTreeName )
{
	// check that we do not have a socket yet
	if( remoteControlSocket )
	{
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) We already have a connection established! Registration is not possible anymore. Ignoring: %s, xmlTreeName=%s" ),
			TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld()), *FString( xmlTreeName.c_str() ) );
		return false;
	}

	// check that both the user id and the tree name are unique
	for( auto & elem : registeredUsers )
	{
		if( elem.Key == &user || elem.Value == xmlTreeName )
		{
			UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Registration with an already existing user pointer or xmlTreeName attempted! Ignoring: %s, xmlTreeName=%s" ),
				TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld()), *FString( xmlTreeName.c_str() ) );
			return false;
		}
	}


	// register
	registeredUsers.Emplace( &user, xmlTreeName );

	// ensure that we tick before our users do
	static_cast<UserType &>(user).AddTickPrerequisiteComponent( this );

	// log and return success
	UE_LOG( LogRemoteControlSystem, Log, TEXT( "(%s) New user successfully registered: %s, xmlTreeName=%s" ),
		TEXT( __FUNCTION__ ), *user.GetPathName( user.GetWorld()), *FString( xmlTreeName.c_str() ) );
	return true;
}
