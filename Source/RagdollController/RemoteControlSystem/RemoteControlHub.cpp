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


// remote control interface address (format: n, n, n, n) and port
#define RCH_ADDRESS 0, 0, 0, 0
#define RCH_PORT 7770

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
		UE_LOG( LogRcRch, Warning, TEXT( "(%s) Not authority: listen socket not created." ), TEXT( __FUNCTION__ ) );
	}
}




void ARemoteControlHub::CreateListenSocket()
{
	// init the error cleanup scope guard
	auto sgError = MakeScopeGuard( [this](){
		UE_LOG( LogRcRch, Error, TEXT( "(%s) Failed to create the listen socket!" ), TEXT( __FUNCTION__ ) );

		// reset everything on errors
		this->ListenSocket = nullptr;
	} );

	// create a listen socket
	FIPv4Endpoint endpoint( FIPv4Address(RCH_ADDRESS), RCH_PORT );
	this->ListenSocket = std::unique_ptr<FSocket>( FTcpSocketBuilder( "Remote control interface main listener" )
		//.AsReusable()
		.AsNonBlocking()
		.BoundToEndpoint( endpoint )
		.Listening( 256 )
		.Build() );

	// verify that we got a socket
	if( !this->ListenSocket ) return;
	
	// all ok, release the error cleanup scope guard
	UE_LOG( LogRcRch, Log, TEXT( "(%s) Listen socket created successfully." ), TEXT( __FUNCTION__ ) );
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
			UE_LOG( LogRcRch, Error, TEXT( "(%s) Incoming connection attempt, accept failed!" ), TEXT( __FUNCTION__ ) );
			continue;
		}

		// set the send and receive buffer sizes
		int32 finalReceiveBufferSize = -1, finalSendBufferSize = -1;
		if( !connectionSocket->SetReceiveBufferSize( RCH_TCP_BUFFERS_SIZE, finalReceiveBufferSize ) |   // do not short-circuit
			!connectionSocket->SetSendBufferSize( RCH_TCP_BUFFERS_SIZE, finalSendBufferSize ) )
		{
			UE_LOG( LogRcRch, Warning, TEXT( "(%s) Failed to set buffer sizes for a new connection!" ), TEXT( __FUNCTION__ ) );
		}

		// log
		UE_LOG( LogRcRch, Log, TEXT( "(%s) Incoming connection accepted. Effective buffer sizes: %d (in), %d (out)" ), TEXT( __FUNCTION__ ),
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
				UE_LOG( LogRcRch, Error, TEXT( "(%s) Pending connection read error! Closing the socket." ), TEXT( __FUNCTION__ ) );

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
		UE_LOG( LogRcRch, Error, TEXT( "(%s) Invalid handshake string: %s" ), TEXT( __FUNCTION__ ), *FString( command.c_str() ) );
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
		UE_LOG( LogRcRch, Error, TEXT( "(%s) Invalid command: %s" ), TEXT( __FUNCTION__ ), *FString( command.c_str() ) );
		socket->PutLine( RCH_ERROR_STRING );
	}
}




void ARemoteControlHub::CmdConnect( std::string args, std::unique_ptr<XmlFSocket> socket )
{
	UE_LOG( LogRcRch, Log, TEXT( "(%s) Processing CONNECT command. Target pattern: %s" ), TEXT( __FUNCTION__ ), *FString( args.c_str() ) );

	// find the target actor based on its FName
	check( GetWorld() );
	for( TActorIterator<AActor> iter( GetWorld() ); iter; ++iter )
	{
		if( Utility::CleanupName( iter->GetName() ) == FString( args.c_str() ) )
		{
			/* target actor found */

			UE_LOG( LogRcRch, Log, TEXT( "(%s) Target actor found, forwarding the connection. Target: %s" ), TEXT( __FUNCTION__ ), *FString( args.c_str() ) );

			// check that the actor is RemoteControllable
			IRemoteControllable * target = Cast<IRemoteControllable>( *iter );
			if( !target )
			{
				// no: log and let the connection drop
				UE_LOG( LogRcRch, Error, TEXT( "(%s) Target actor is not RemoteControllable! Target: %s" ), TEXT( __FUNCTION__ ), *FString( args.c_str() ) );
				socket->PutLine( RCH_ERROR_STRING );
				return;
			}

			// send ack to socket
			if( !socket->PutLine( RCH_ACK_STRING ) )
			{
				// failed: log and let the connection drop (no point in sending an error string to the already failed TCP stream)
				UE_LOG( LogRcRch, Error, TEXT( "(%s) Failed to send ACK string to remote!" ), TEXT( __FUNCTION__ ) );
				return;
			}

			// forward the connection and return
			target->ConnectWith( std::move( socket ) );
			return;
		}
	}

	// target not found, log and let the connection drop
	UE_LOG( LogRcRch, Error, TEXT( "(%s) Target actor not found: %s" ), TEXT( __FUNCTION__ ), *FString( args.c_str() ) );
	socket->PutLine( RCH_ERROR_STRING );
}
