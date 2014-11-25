// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "RemoteControllable.generated.h"




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
	AActor * thisActor;

	/** Remote control socket */
	TSharedPtr<FSocket> RemoteControlSocket;


public:

	IRemoteControllable( AActor * newThisActor );

	virtual void ConnectWith( TSharedPtr<FSocket> socket );

};
