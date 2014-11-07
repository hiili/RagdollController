// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController45.h"
#include "RemoteControlInterface.h"

#include <Networking.h>
//#include <TcpListener.h>
//#include <TcpSocketBuilder.h>

#include <string>
using std::string;

#include "ScopeGuard.h"


// remote control interface address (format: n, n, n, n) and port
#define RCI_ADDRESS 0, 0, 0, 0
#define RCI_PORT 7770




ARemoteControlInterface::ARemoteControlInterface(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// enable ticking
	PrimaryActorTick.bCanEverTick = true;
}




void ARemoteControlInterface::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// init the error cleanup scope guard
	auto sgError = MakeScopeGuard( [this](){
		UE_LOG( LogTemp, Error, TEXT( "(%s) Scope guard 'sgError' triggered!" ), TEXT( __FUNCTION__ ) );

		// reset everything if any errors
		this->ListenSocket = 0;
	} );

	// create a listen socket
	FIPv4Endpoint endpoint( FIPv4Address(RCI_ADDRESS), RCI_PORT );
	this->ListenSocket = TSharedPtr<FSocket>( FTcpSocketBuilder( "Remote control interface main listener" )
		.AsReusable()
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

		//this->PendingSockets.Add( TSharedPtr<FSocket>( connectionSocket ) );

		uint32 bytesPending;
		int32 bytesRead;
		uint8 buffer[256];
		buffer[0] = 'A';
		while( buffer[0] != 'X' )
		{
			bool ok = connectionSocket->HasPendingData( bytesPending );
			UE_LOG( LogTemp, Error, TEXT( "HasPendingData ok = %d, num = %d" ), ok, bytesPending );

			ok = connectionSocket->Recv( buffer, 10, bytesRead, ESocketReceiveFlags::None );
			UE_LOG( LogTemp, Error, TEXT( "  Recv ok = %d, num = %d, data = %c" ), ok, bytesRead, TCHAR(buffer[0]) );
			for( int x = 0; x < 100000000; ++x );
		}

	}
}
