// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "XmlFSocket.h"

#include <memory>

#include "RemoteControllable.generated.h"




/** Helper class need by the engine. */
UINTERFACE()
class URemoteControllable : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};




/**
 * Interface class for actors that wish to be reachable via a RemoteControlHub actor.
 * 
 * Once your class inherits from this interface, it can be reached via a RemoteControlHub actor by its UObject name. Note that the provided name is matched
 * against UObject names that are cleaned up with Utility::CleanupName, which effectively removes all underscore-delimited suffixes from the name. 
 * 
 * Upon a connection request, the hub actor calls the ConnectWith() method, which in turn stores the connection socket in the RemoteControlSocket field.
 * You can hook into the virtual ConnectWith() method in case that a connection notification is needed.
 * 
 * @see RemoteControlHub, XmlFSocket, Mbml
 */
class IRemoteControllable
{
	GENERATED_IINTERFACE_BODY()


protected:

	/** Remote control socket */
	std::unique_ptr<XmlFSocket> RemoteControlSocket;


public:

	IRemoteControllable();

	virtual void ConnectWith( std::unique_ptr<XmlFSocket> socket );

};
