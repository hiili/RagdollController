// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController45.h"
#include "RemoteControlInterface.h"

#include <Networking.h>
//#include <TcpListener.h>
//#include <TcpSocketBuilder.h>

#include <string>
#include <algorithm>

#include "ScopeGuard.h"


// remote control interface address (format: n, n, n, n) and port
#define RCI_ADDRESS 0, 0, 0, 0
#define RCI_PORT 7770

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

	CreateListenSocket();
}




void ARemoteControlInterface::CreateListenSocket()
{
	// init the error cleanup scope guard
	auto sgError = MakeScopeGuard( [this](){
		UE_LOG( LogTemp, Error, TEXT( "(%s) RCI: Failed to create the listen socket!" ), TEXT( __FUNCTION__ ) );

		// reset everything on errors
		this->ListenSocket = 0;
	} );

	// create a listen socket
	FIPv4Endpoint endpoint( FIPv4Address(RCI_ADDRESS), RCI_PORT );
	this->ListenSocket = TSharedPtr<FSocket>( FTcpSocketBuilder( "Remote control interface main listener" )
		.AsReusable()
		.AsNonBlocking()
		.BoundToEndpoint( endpoint )
		.Listening( 256 )
		.Build() );

	// verify that we got a socket
	if( !this->ListenSocket.IsValid() ) return;
	
	// set the receive buffer size
	if( !this->ListenSocket->SetReceiveBufferSize( 1000000, this->RCIReceiveBufferSize ) ) return;

	// all ok, release the error cleanup scope guard
	UE_LOG( LogTemp, Error, TEXT("RCI: Listen socket created successfully.") );
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
			UE_LOG( LogTemp, Error, TEXT( "RCI: Incoming connection attempt, accept failed!" ) );
			continue;
		}
		UE_LOG( LogTemp, Error, TEXT( "RCI: Incoming connection accepted." ) );

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
			UE_LOG( LogTemp, Error, TEXT( "RCI: Pending connection read error or line buffer overflow! Closing the socket." ) );

			this->PendingSockets.RemoveAt( iterPendingSocket.GetIndex() );
			break;   // play safe and don't touch the iterator anymore
		}

		// if no new data, then continue iterating
		if( bytesRead == 0 ) continue;

		// convert the buffer into a C++ string 
		std::string commandLine( lineBuffer, bytesRead );

		// look for an LF, continue iterating if not found
		std::size_t posLF = commandLine.find_first_of( '\n' );
		if( posLF == std::string::npos ) continue;


		/* we have a full command line -> remove it from the socket stream, and remove the socket from the pending buffer and dispatch it */

		// remove the command line from the socket stream (on failure, kill the socket and break)
		success = (*iterPendingSocket)->Recv( (uint8 *)lineBuffer, posLF + 1, bytesRead, ESocketReceiveFlags::None );
		if( !success || bytesRead != posLF + 1 )
		{
			UE_LOG( LogTemp, Error, TEXT( "RCI: Pending connection read error! Closing the socket." ) );

			this->PendingSockets.RemoveAt( iterPendingSocket.GetIndex() );
			break;   // play safe and don't touch the iterator anymore
		}

		// cut the command line at LF (cut away also the LF)
		commandLine.erase( posLF );

		// dispatch the connection and remove the socket from the pending buffer
		DispatchSocket( commandLine, *iterPendingSocket );
		this->PendingSockets.RemoveAt( iterPendingSocket.GetIndex() );

		// play safe and don't touch the iterator anymore
		break;

	}
}




void ARemoteControlInterface::DispatchSocket( const std::string & command, TSharedPtr<FSocket> socket )
{
	UE_LOG( LogTemp, Error, TEXT( "RCI: HAVE DISPATCH COMMAND: %s" ), *FString( command.c_str() ) );
}
