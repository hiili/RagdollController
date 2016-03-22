// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "RemoteControllableHelper.h"

#include "RemoteControllable.h"


DECLARE_LOG_CATEGORY_EXTERN( LogRemoteControlSystem, Log, All );




// Sets default values for this component's properties
URemoteControllableHelper::URemoteControllableHelper()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsBeginPlay = false;
	PrimaryComponentTick.bCanEverTick = true;
}


// Called every frame
void URemoteControllableHelper::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	// if attached to a RemoteControllable, then call its postUserTick() method
	if( URemoteControllable * parent = dynamic_cast<URemoteControllable *>( GetAttachParent() ) )
	{
		parent->postUserTick();
	}
	else
	{
		UE_LOG( LogRemoteControlSystem, Error, TEXT( "(%s) Not attached to a RemoteControllable! Cannot run post-user-tick comms operations. Parent: %s, class=%s" ),
			TEXT( __FUNCTION__ ),
			GetAttachParent() ? *GetAttachParent()->GetPathName( GetAttachParent()->GetWorld() ) : TEXT( "N/A" ),
			GetAttachParent() && GetAttachParent()->GetClass() ? *GetAttachParent()->GetClass()->GetName() : TEXT( "(N/A)" ) );
	}
}
