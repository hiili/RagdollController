// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController45.h"
#include "RemoteControllable.h"




URemoteControllable::URemoteControllable( const class FPostConstructInitializeProperties& PCIP )
	: Super( PCIP )
{
}




IRemoteControllable::IRemoteControllable( AActor * thisActor_ ) :
	thisActor( thisActor_ )
{
	check( thisActor );
}


void IRemoteControllable::ConnectWith( TSharedPtr<FSocket> socket )
{
	UE_LOG( LogRcRci, Log, TEXT( "(%s) New remote controller connected. Actor: %s" ), TEXT( __FUNCTION__ ), *this->thisActor->GetName() );

	// store the new socket
	this->RemoteControlSocket = socket;
}
