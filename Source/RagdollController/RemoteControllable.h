// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "XmlFSocket.h"

#include <memory>

#include "RemoteControllable.generated.h"




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
	std::unique_ptr<XmlFSocket> RemoteControlSocket;


public:

	IRemoteControllable( AActor * thisActor );

	virtual void ConnectWith( std::unique_ptr<XmlFSocket> socket );

};
