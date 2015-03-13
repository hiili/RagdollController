// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "RemoteControlHub.h"

#include "RemoteControllable.h"

#include "LineFSocket.h"
#include "ScopeGuard.h"
#include "Utility.h"

#include <Networking.h>
#include <SharedPointer.h>

#include <string>
#include <algorithm>


// remote control interface address (format: n, n, n, n) and port
#define RCH_ADDRESS 0, 0, 0, 0
#define RCH_PORT 7770

// TCP send and receive buffer size
#define RCH_TCP_BUFFERS_SIZE (64 * 1024)

// handshake string: the remote client should send this in the beginning of the initial command line
#define RCH_HANDSHAKE_STRING "RagdollController RCH: "
#define RCH_HANDSHAKE_ACK_STRING "OK"

// command strings
#define RCH_COMMAND_CONNECT "CONNECT "

// size of the buffer for incoming dispatch command lines
#define LINE_BUFFER_SIZE 1024




ARemoteControlHub::ARemoteControlHub()
{
	// enable ticking
	PrimaryActorTick.bCanEverTick = true;
}




void ARemoteControlHub::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// if authority, then create the main listen socket
	if( this->Role >= ROLE_Authority )
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
		this->ListenSocket = 0;
	} );

	// create a listen socket
	FIPv4Endpoint endpoint( FIPv4Address(RCH_ADDRESS), RCH_PORT );
	this->ListenSocket = TSharedPtr<FSocket>( FTcpSocketBuilder( "Remote control interface main listener" )
		//.AsReusable()
		.AsNonBlocking()
		.BoundToEndpoint( endpoint )
		.Listening( 256 )
		.Build() );

	// verify that we got a socket
	if( !this->ListenSocket.IsValid() ) return;
	
	// set the send and receive buffer sizes
	int32 finalReceiveBufferSize, finalSendBufferSize;
	if( !this->ListenSocket->SetReceiveBufferSize( RCH_TCP_BUFFERS_SIZE, finalReceiveBufferSize ) ) return;
	if( !this->ListenSocket->SetSendBufferSize( RCH_TCP_BUFFERS_SIZE, finalSendBufferSize ) ) return;

	// all ok, release the error cleanup scope guard
	UE_LOG( LogRcRch, Log, TEXT("(%s) Listen socket created successfully. Effective buffer sizes: %d (in), %d (out)"), TEXT( __FUNCTION__ ),
		finalReceiveBufferSize, finalSendBufferSize );
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
	if( !this->ListenSocket.IsValid() ) return;

	// loop as long as we have new waiting connections
	bool hasNewConnections;
	while( this->ListenSocket->HasPendingConnection( hasNewConnections ) && hasNewConnections )
	{
		// try to create a new socket for the connection
		FSocket * connectionSocket = this->ListenSocket->Accept( "Remote control interface connection" );

		// check whether we succeeded
		if( !connectionSocket )
		{
			UE_LOG( LogRcRch, Error, TEXT( "(%s) Incoming connection attempt, accept failed!" ), TEXT( __FUNCTION__ ) );
			continue;
		}
		UE_LOG( LogRcRch, Log, TEXT( "(%s) Incoming connection accepted." ), TEXT( __FUNCTION__ ) );

		// wrap the socket into a LineFSocket and store it to PendingSockets (check goodness later)
		this->PendingSockets.Add( TSharedPtr<LineFSocket>( new LineFSocket( TSharedPtr<FSocket>( connectionSocket ) ) ) );

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
		DispatchSocket( (*iterPendingSocket)->Line, *iterPendingSocket );
		this->PendingSockets.RemoveAt( iterPendingSocket.GetIndex() );

		// play safe and don't touch the iterator anymore
		break;

	}
}




void ARemoteControlHub::DispatchSocket( std::string command, const TSharedPtr<LineFSocket> & socket )
{
	// verify and remove handshake
	if( command.find( RCH_HANDSHAKE_STRING ) != 0 )
	{
		UE_LOG( LogRcRch, Error, TEXT( "(%s) Invalid handshake string: %s" ), TEXT( __FUNCTION__ ), *FString( command.c_str() ) );
		return;
	}
	command.erase( 0, std::strlen( RCH_HANDSHAKE_STRING ) );

	// switch on command
	if( command.find( RCH_COMMAND_CONNECT ) == 0 )
	{
		CmdConnect( command.substr( std::strlen( RCH_COMMAND_CONNECT ) ), socket );
	}
	else
	{
		UE_LOG( LogRcRch, Error, TEXT( "(%s) Invalid command: %s" ), TEXT( __FUNCTION__ ), *FString( command.c_str() ) );
	}
}




void ARemoteControlHub::CmdConnect( std::string args, const TSharedPtr<LineFSocket> & socket )
{
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
				return;
			}

			// send ack to socket
			if( !socket->PutLine( RCH_HANDSHAKE_ACK_STRING ) )
			{
				// failed: log and let the connection drop
				UE_LOG( LogRcRch, Error, TEXT( "(%s) Failed to send ACK string to remote!" ), TEXT( __FUNCTION__ ), *FString( args.c_str() ) );
				return;
			}

			// forward the connection and return
			target->ConnectWith( socket );
			return;
		}
	}

	// target not found, log and let the connection drop
	UE_LOG( LogRcRch, Error, TEXT( "(%s) Target actor not found: %s" ), TEXT( __FUNCTION__ ), *FString( args.c_str() ) );
}
