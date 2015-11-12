// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "RCLevelScriptActor.h"

#include <PhysicsPublic.h>

#include <PxPhysics.h>
#include <PxScene.h>
#include <extensions/PxVisualDebuggerExt.h>




ARCLevelScriptActor::ARCLevelScriptActor( const FObjectInitializer & ObjectInitializer ) :
	Super( ObjectInitializer )
{
}




void ARCLevelScriptActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}




void ARCLevelScriptActor::BeginPlay()
{
	Super::BeginPlay();

	// consider connecting to physics debugger
	if( this->ConnectToPhysXVisualDebugger )
	{
		ConnectToPVD();
	}
}


void ARCLevelScriptActor::Tick( float deltaSeconds )
{
	Super::Tick( deltaSeconds );
}




/** Implementation adapted from https://physx3.googlecode.com/svn/trunk/PhysX-3.2_PC_SDK_Core/Samples/SampleBase/PhysXSample.cpp */
void ARCLevelScriptActor::ConnectToPVD()
{
	if( GetWorld() && GetWorld()->GetPhysicsScene() && GetWorld()->GetPhysicsScene()->GetPhysXScene( 0 ) )
	{
		physx::PxPhysics & physics = GetWorld()->GetPhysicsScene()->GetPhysXScene( 0 )->getPhysics();
		physx::PxVisualDebuggerConnection * theConnection = 0;
		if( physics.getPvdConnectionManager() && physics.getVisualDebugger() )
		{
			theConnection = physx::PxVisualDebuggerExt::createConnection( physics.getPvdConnectionManager(), "127.0.0.1", 5425, 10000,
				physx::PxVisualDebuggerExt::getAllConnectionFlags() );
			physics.getVisualDebugger()->setVisualDebuggerFlags( physx::PxVisualDebuggerFlag::eTRANSMIT_CONTACTS |
				physx::PxVisualDebuggerFlag::eTRANSMIT_SCENEQUERIES | physx::PxVisualDebuggerFlag::eTRANSMIT_CONSTRAINTS );
		}
		if( theConnection )
		{
			UE_LOG( LogRcSystem, Log, TEXT( "(%s) PhysX Visual Debugger connection initialized succesfully." ), TEXT( __FUNCTION__ ) );
		}
		else
		{
			UE_LOG( LogRcSystem, Error, TEXT( "(%s) PhysX Visual Debugger: Failed to initialize connection!" ), TEXT( __FUNCTION__ ) );
		}
	}
	else
	{
		UE_LOG( LogRcSystem, Error, TEXT( "(%s) PhysX Visual Debugger: Failed to initialize connection: Failed to access the PhysX scene!" ), TEXT( __FUNCTION__ ) );
	}
}
