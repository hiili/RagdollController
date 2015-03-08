// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <SharedPointer.h>

#include "RemoteControllable.generated.h"

class XmlFSocket;




UINTERFACE()
class URemoteControllable : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};




class IRemoteControllable
{
	GENERATED_IINTERFACE_BODY()


	/** Reference to this object as an AActor */
	AActor * ThisActor;


protected:

	/** Remote control socket */
	TSharedPtr<XmlFSocket> RemoteControlSocket;


public:

	IRemoteControllable( AActor * thisActor );

	virtual void ConnectWith( const TSharedPtr<XmlFSocket> & socket );

};
