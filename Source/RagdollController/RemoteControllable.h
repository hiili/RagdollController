// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <SharedPointer.h>

#include "RemoteControllable.generated.h"

class LineFSocket;




UINTERFACE()
class URemoteControllable : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};




class IRemoteControllable
{
	GENERATED_IINTERFACE_BODY()

protected:

	/** Reference to this object as an AActor */
	AActor * ThisActor;

	/** Remote control socket */
	TSharedPtr<LineFSocket> RemoteControlSocket;


public:

	IRemoteControllable( AActor * thisActor );

	virtual void ConnectWith( const TSharedPtr<LineFSocket> & socket );

};
