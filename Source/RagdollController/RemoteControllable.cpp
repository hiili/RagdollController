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
	FString name( "(N/A)" );
	if( AActor *thisActor = dynamic_cast<AActor *>(this) )
	{
		name = Utility::CleanupName( thisActor->GetName() );
	}
	UE_LOG( LogRcRch, Log, TEXT( "(%s) New remote controller connected. Actor: %s" ), TEXT( __FUNCTION__ ), *name );

	// store the new socket
	this->RemoteControlSocket = std::move( socket );
}
