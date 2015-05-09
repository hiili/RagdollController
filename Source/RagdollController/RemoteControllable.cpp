// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "RemoteControllable.h"

#include "XmlFSocket.h"
#include "Utility.h"

#include <SharedPointer.h>




URemoteControllable::URemoteControllable( const FObjectInitializer & ObjectInitializer )
	: Super( ObjectInitializer )
{
}




IRemoteControllable::IRemoteControllable()
{
}


void IRemoteControllable::ConnectWith( std::unique_ptr<XmlFSocket> socket )
{
	// store the new socket
	this->RemoteControlSocket = std::move( socket );

	// log
	AActor * thisActor = dynamic_cast<AActor *>(this);
	UE_LOG( LogRcRch, Log, TEXT( "(%s) New remote controller connected. Actor: %s" ), TEXT( __FUNCTION__ ),
		thisActor ? *Utility::CleanupName( thisActor->GetName() ) : TEXT( "(N/A)" ) );
}
