// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController45.h"
#include "RemoteControlInterface.h"

#include "RemoteControllable.h"

#include "ScopeGuard.h"
#include "Utility.h"

#include <Networking.h>
#include <SharedPointer.h>

#include <string>
#include <algorithm>


// remote control interface address (format: n, n, n, n) and port
#define RCI_ADDRESS 0, 0, 0, 0
#define RCI_PORT 7770

// handshake string: the remote client should send this in the beginning of the initial command line
#define RCI_HANDSHAKE_STRING "RagdollController RCI: "
#define RCI_HANDSHAKE_ACK_STRING "OK"

// command strings
#define RCI_COMMAND_CONNECT "CONNECT "

// size of the buffer for incoming dispatch command lines
#define LINE_BUFFER_SIZE 256




ARemoteControlInterface::ARemoteControlInterface(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// enable ticking
	PrimaryActorTick.bCanEverTick = true;
}




void ARemoteControlInterface::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// clean up the actor name
	Utility::UObjectNameCleanup( *this );

	// if authority, then create the main listen socket
	if( this->Role >= ROLE_Authority )
	{
		CreateListenSocket();
	} else {
		UE_LOG( LogRcRci, Warning, TEXT( "(%s) Not authority: listen socket not created." ), TEXT( __FUNCTION__ ) );
	}
}




void ARemoteControlInterface::CreateListenSocket()
{
	// init the error cleanup scope guard
	auto sgError = MakeScopeGuard( [this](){
		UE_LOG( LogRcRci, Error, TEXT( "(%s) Failed to create the listen socket!" ), TEXT( __FUNCTION__ ) );

		// reset everything on errors
		this->ListenSocket = 0;
	} );

	// create a listen socket
	FIPv4Endpoint endpoint( FIPv4Address(RCI_ADDRESS), RCI_PORT );
	this->ListenSocket = TSharedPtr<FSocket>( FTcpSocketBuilder( "Remote control interface main listener" )
		//.AsReusable()
		.AsNonBlocking()
		.BoundToEndpoint( endpoint )
		.Listening( 256 )
		.Build() );

	// verify that we got a socket
	if( !this->ListenSocket.IsValid() ) return;
	
	// set the receive buffer size
	if( !this->ListenSocket->SetReceiveBufferSize( 1000000, this->RCIReceiveBufferSize ) ) return;

	// all ok, release the error cleanup scope guard
	UE_LOG( LogRcRci, Log, TEXT("(%s) Listen socket created successfully."), TEXT( __FUNCTION__ ) );
	sgError.release();
}




void ARemoteControlInterface::Tick( float deltaSeconds )
{
	Super::Tick( deltaSeconds );

	CheckForNewConnections();
	ManagePendingConnections();
}




void ARemoteControlInterface::CheckForNewConnections()
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
			UE_LOG( LogRcRci, Error, TEXT( "(%s) Incoming connection attempt, accept failed!" ), TEXT( __FUNCTION__ ) );
			continue;
		}
		UE_LOG( LogRcRci, Log, TEXT( "(%s) Incoming connection accepted." ), TEXT( __FUNCTION__ ) );

		this->PendingSockets.Add( TSharedPtr<FSocket>( connectionSocket ) );

	}
}




void ARemoteControlInterface::ManagePendingConnections()
{
	// iterate over our pending connections
	for( auto iterPendingSocket = this->PendingSockets.CreateIterator(); iterPendingSocket; ++iterPendingSocket )
	{
		char lineBuffer[LINE_BUFFER_SIZE];
		bool success;
		uint32 bytesPending;
		int32 bytesRead;


		/* see if we have received a full command line */

		// if no new data, then continue iterating
		if( !(*iterPendingSocket)->HasPendingData( bytesPending ) ) continue;

		// peek data
		success = (*iterPendingSocket)->Recv( (uint8 *)lineBuffer, std::min<int>( LINE_BUFFER_SIZE, bytesPending ), bytesRead, ESocketReceiveFlags::Peek );

		// if failed or buffer overflowed, then close the socket
		if( !success || bytesRead == LINE_BUFFER_SIZE )
		{
			UE_LOG( LogRcRci, Error, TEXT( "(%s) Pending connection read error or line buffer overflow! Closing the socket." ), TEXT( __FUNCTION__ ) );

			this->PendingSockets.RemoveAt( iterPendingSocket.GetIndex() );
			break;   // play safe and don't touch the iterator anymore
		}

		// if no new data, then continue iterating
		if( bytesRead == 0 ) continue;

		// convert the buffer into a C++ string 
		std::string commandLine( lineBuffer, bytesRead );

		// look for an CR or LF, continue iterating if not found
		std::size_t posLF = commandLine.find_first_of( "\r\n" );
		if( posLF == std::string::npos ) continue;


		/* we have a full command line -> remove it from the socket stream, and remove the socket from the pending buffer and dispatch it */

		// remove the command line from the socket stream (on failure, kill the socket and break)
		success = (*iterPendingSocket)->Recv( (uint8 *)lineBuffer, posLF + 1, bytesRead, ESocketReceiveFlags::None );
		if( !success || bytesRead != posLF + 1 )
		{
			UE_LOG( LogRcRci, Error, TEXT( "(%s) Pending connection read error! Closing the socket." ), TEXT( __FUNCTION__ ) );

			this->PendingSockets.RemoveAt( iterPendingSocket.GetIndex() );
			break;   // play safe and don't touch the iterator anymore
		}

		// cut the command line at LF (cut away also the LF)
		commandLine.erase( posLF );

		// try to dispatch the connection, then remove the socket from the pending buffer
		DispatchSocket( commandLine, *iterPendingSocket );
		this->PendingSockets.RemoveAt( iterPendingSocket.GetIndex() );

		// play safe and don't touch the iterator anymore
		break;

	}
}




void ARemoteControlInterface::DispatchSocket( std::string command, TSharedPtr<FSocket> socket )
{
	// verify and remove handshake
	if( command.find( RCI_HANDSHAKE_STRING ) != 0 )
	{
		UE_LOG( LogRcRci, Error, TEXT( "(%s) Invalid handshake string: %s" ), TEXT( __FUNCTION__ ), *FString( command.c_str() ) );
		return;
	}
	command.erase( 0, std::strlen( RCI_HANDSHAKE_STRING ) );

	// switch on command
	if( command.find( RCI_COMMAND_CONNECT ) == 0 )
	{
		CmdConnect( command.substr( std::strlen( RCI_COMMAND_CONNECT ) ), socket );
	}
	else
	{
		UE_LOG( LogRcRci, Error, TEXT( "(%s) Invalid command: %s" ), TEXT( __FUNCTION__ ), *FString( command.c_str() ) );
	}
}




void ARemoteControlInterface::CmdConnect( std::string args, TSharedPtr<FSocket> socket )
{
	// find the target actor based on its FName
	for( TActorIterator<AActor> iter( GetWorld() ); iter; ++iter )
	{
		if( iter->GetName() == FString( args.c_str() ) )
		{
			/* target actor found */

			UE_LOG( LogRcRci, Log, TEXT( "(%s) Target actor found, forwarding the connection. Target: %s" ), TEXT( __FUNCTION__ ), *FString( args.c_str() ) );

			// check that the actor is RemoteControllable
			IRemoteControllable * target = InterfaceCast<IRemoteControllable>( *iter );
			if( !target )
			{
				// no: log and stop
				UE_LOG( LogRcRci, Error, TEXT( "(%s) Target actor is not RemoteControllable! Target: %s" ), TEXT( __FUNCTION__ ), *FString( args.c_str() ) );
				return;
			}

			// send ack to socket
			int32 bytesSent;
			socket->Send( (const uint8 *)(RCI_HANDSHAKE_ACK_STRING "\n"), std::strlen( RCI_HANDSHAKE_ACK_STRING "\n" ), bytesSent );
			if( bytesSent != std::strlen( RCI_HANDSHAKE_ACK_STRING "\n" ) )
			{
				UE_LOG( LogRcRci, Error, TEXT( "(%s) Failed to send ACK string to remote!" ), TEXT( __FUNCTION__ ), *FString( args.c_str() ) );
				return;
			}

			// forward the connection and return
			target->ConnectWith( socket );
			return;
		}
	}

	UE_LOG( LogRcRci, Error, TEXT( "(%s) Target actor not found: %s" ), TEXT( __FUNCTION__ ), *FString( args.c_str() ) );
}
