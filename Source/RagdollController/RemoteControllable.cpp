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




IRemoteControllable::IRemoteControllable( AActor * thisActor ) :
	ThisActor( thisActor )
{
	check( ThisActor );
}


void IRemoteControllable::ConnectWith( const TSharedPtr<XmlFSocket> & socket )
{
	UE_LOG( LogRcRch, Log, TEXT( "(%s) New remote controller connected. Actor: %s" ), TEXT( __FUNCTION__ ),
		*Utility::CleanupName( this->ThisActor->GetName() ) );

	// store the new socket
	this->RemoteControlSocket = socket;
}
