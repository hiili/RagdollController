// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "RemoteControlHub.h"

#include "RemoteControllable.h"

#include "XmlFSocket.h"
#include "ScopeGuard.h"
#include "Utility.h"

#include <Networking.h>

#include <string>
#include <memory>
#include <algorithm>


// TCP send and receive buffer size
#define RCH_TCP_BUFFERS_SIZE (64 * 1024)

// handshake string: the remote client should send this in the beginning of the initial command line
#define RCH_HANDSHAKE_STRING "RagdollController RCH: "
#define RCH_ACK_STRING "OK"
#define RCH_ERROR_STRING "ERROR"

// command strings
#define RCH_COMMAND_CONNECT "CONNECT "

// size of the buffer for incoming dispatch command lines
#define LINE_BUFFER_SIZE 1024


DECLARE_LOG_CATEGORY_EXTERN( LogRemoteControlSystem, Log, All );
DEFINE_LOG_CATEGORY( LogRemoteControlSystem );




ARemoteControlHub::ARemoteControlHub()
{
	// enable ticking
	PrimaryActorTick.bCanEverTick = true;

	// create a dummy root component and a sprite for the editor
	Utility::AddDefaultRootComponent( this, "/Game/Assets/Gears128" );
}




void ARemoteControlHub::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// if authority, then create the main listen socket
	if( HasAuthority() )
	{
		CreateListenSocket();
	} else {
		UE_LOG( LogRemoteControlSystem, Warning, TEXT( "(%s) Not authority: listen socket not created." ), TEXT( __FUNCTION__ ) );
	}
}




void ARemoteControlHub::CreateListenSocket()
{
	// init the error cleanup scope guard
	auto sgError = MakeScopeGuard( [this](){
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Failed to create the listen socket!" ), TEXT( __FUNCTION__ ) );

		// reset everything on errors
		this->ListenSocket = nullptr;
	} );

	// create a listen socket
	FIPv4Endpoint endpoint( ListenOnlyOnLocalhost ? FIPv4Address( 127, 0, 0, 1 ) : FIPv4Address( 0, 0, 0, 0 ), ListenPort );
	this->ListenSocket = std::unique_ptr<FSocket>( FTcpSocketBuilder( "RemoteControlHub main listener" )
		//.AsReusable()
		.AsNonBlocking()
		.BoundToEndpoint( endpoint )
		.Listening( 256 )
		.Build() );

	// verify that we got a socket
	if( !this->ListenSocket ) return;
	
	// all ok, release the error cleanup scope guard
	UE_LOG( LogRemoteControlSystem, Log, TEXT( "(%s) Listen socket created successfully." ), TEXT( __FUNCTION__ ) );
	sgError.release();
}




void ARemoteControlHub::Tick( float deltaSeconds )
{
	Super::Tick( deltaSeconds );

	CheckForNewConnections();
	ManagePendingConnections();
}




void ARemoteControlHub::CheckForNewConnections()
{
	// check that we have a listen socket
	if( !this->ListenSocket ) return;

	// loop as long as we have new waiting connections
	bool hasNewConnections;
	while( this->ListenSocket->HasPendingConnection( hasNewConnections ) && hasNewConnections )
	{
		// try to create a new socket for the connection
		std::unique_ptr<FSocket> connectionSocket( this->ListenSocket->Accept( "Remote control interface connection" ) );

		// check whether we succeeded
		if( !connectionSocket )
		{
			// failed, log and keep looping
			UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Incoming connection attempt, accept failed!" ), TEXT( __FUNCTION__ ) );
			continue;
		}

		// set the send and receive buffer sizes
		int32 finalReceiveBufferSize = -1, finalSendBufferSize = -1;
		if( !connectionSocket->SetReceiveBufferSize( RCH_TCP_BUFFERS_SIZE, finalReceiveBufferSize ) |   // do not short-circuit
			!connectionSocket->SetSendBufferSize( RCH_TCP_BUFFERS_SIZE, finalSendBufferSize ) )
		{
			UE_LOG( LogRemoteControlSystem, Warning, TEXT( "(%s) Failed to set buffer sizes for a new connection!" ), TEXT( __FUNCTION__ ) );
		}

		// log
		UE_LOG( LogRemoteControlSystem, Log, TEXT( "(%s) Incoming connection accepted. Effective buffer sizes: %d (in), %d (out)" ), TEXT( __FUNCTION__ ),
			finalReceiveBufferSize, finalSendBufferSize );

		// wrap the socket into an XmlFSocket and store it to PendingSockets (check goodness later)
		this->PendingSockets.Add( std::make_unique<XmlFSocket>( std::move( connectionSocket ) ) );

	}
}




void ARemoteControlHub::ManagePendingConnections()
{
	// iterate over our pending connections
	for( auto iterPendingSocket = this->PendingSockets.CreateIterator(); iterPendingSocket; ++iterPendingSocket )
	{
		// try to get a full command line, continue with next if fail (also drop bad connections)
		if( !(*iterPendingSocket)->GetLine() )
		{
			// bad connection? drop it
			if( !(*iterPendingSocket)->IsGood() )
			{
				UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Pending connection read error! Closing the socket." ), TEXT( __FUNCTION__ ) );

				this->PendingSockets.RemoveAt( iterPendingSocket.GetIndex() );
				break;   // play safe and don't touch the iterator anymore
			}

			continue;
		}

		// try to dispatch the connection, then remove the socket from the pending sockets list
		std::string command = (*iterPendingSocket)->Line;   // do this before move
		DispatchSocket( command, std::move( *iterPendingSocket ) );
		this->PendingSockets.RemoveAt( iterPendingSocket.GetIndex() );

		// play safe and don't touch the iterator anymore
		break;
	}
}




void ARemoteControlHub::DispatchSocket( std::string command, std::unique_ptr<XmlFSocket> socket )
{
	// verify and remove handshake
	if( command.compare( 0, std::strlen( RCH_HANDSHAKE_STRING ), RCH_HANDSHAKE_STRING ) != 0 )
	{
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Invalid handshake string: %s" ), TEXT( __FUNCTION__ ), *FString( command.c_str() ) );
		socket->PutLine( RCH_ERROR_STRING );   // don't care about errors
		return;
	}
	command.erase( 0, std::strlen( RCH_HANDSHAKE_STRING ) );

	// switch on command
	if( command.compare( 0, std::strlen( RCH_COMMAND_CONNECT ), RCH_COMMAND_CONNECT ) == 0 )
	{
		CmdConnect( command.substr( std::strlen( RCH_COMMAND_CONNECT ) ), std::move( socket ) );
	}
	else
	{
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Invalid command: %s" ), TEXT( __FUNCTION__ ), *FString( command.c_str() ) );
		socket->PutLine( RCH_ERROR_STRING );
	}
}




void ARemoteControlHub::CmdConnect( std::string args, std::unique_ptr<XmlFSocket> socket )
{
	UE_LOG( LogRemoteControlSystem, Log, TEXT( "(%s) Processing CONNECT command. NetworkName pattern: %s" ), TEXT( __FUNCTION__ ), *FString( args.c_str() ) );

	// find the target RemoteControllable component based on its NetworkName
	FWildcardString pattern( args.c_str() );
	URemoteControllable * selected = nullptr;
	ForEachObjectOfClass( URemoteControllable::StaticClass(), [this, &pattern, &selected]( UObject * candidateObject ) -> void {

		// downcast: should be always possible because we are looping only through objects of this type
		URemoteControllable * candidate = static_cast<URemoteControllable *>(candidateObject);

		// match names; also make sure that the component is from our world
		if( candidate->GetWorld() == GetWorld() && pattern.IsMatch( candidate->NetworkName ) )
		{
			/* component with a matching name found */

			// have already found one?
			if( selected )
			{
				// target has been already found; we have multiple matches -> log and ignore
				UE_LOG( LogRemoteControlSystem, Warning, TEXT( "(ARemoteControlHub::CmdConnect) Multiple matching components found for network name pattern '%s'! Ignoring: %s, NetworkName=%s" ),
					*pattern, *candidate->GetPathName( candidate->GetWorld() ), *candidate->NetworkName );
			}
			else
			{
				// first hit: store and log
				selected = candidate;
				UE_LOG( LogRemoteControlSystem, Log, TEXT( "(ARemoteControlHub::CmdConnect) Target component found: %s, NetworkName=%s" ),
					*selected->GetPathName( selected->GetWorld() ), *selected->NetworkName );
			}
		}

	} );

	// did we find the target object?
	if( selected )
	{
		// yes: send ack to socket
		if( !socket->PutLine( RCH_ACK_STRING ) )
		{
			// failed: log and let the connection drop (no point in sending an error string to the already failed TCP stream)
			UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Failed to send ACK string to remote!" ), TEXT( __FUNCTION__ ) );
			return;
		}

		// forward the connection and return
		selected->ConnectWith( std::move( socket ) );
		return;
	}
	else
	{
		// no target found

		// invariant: we should still have the socket unmoved at this point
		check( socket );

		// log, send error to socket, and let the connection drop
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Target component not found: %s" ), TEXT( __FUNCTION__ ), *pattern );
		socket->PutLine( RCH_ERROR_STRING );
		return;
	}
}
