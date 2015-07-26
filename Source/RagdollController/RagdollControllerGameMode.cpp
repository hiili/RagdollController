// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "RagdollControllerGameMode.h"




ARagdollControllerGameMode::ARagdollControllerGameMode( const FObjectInitializer & ObjectInitializer )
	: Super( ObjectInitializer )
{
}




void ARagdollControllerGameMode::InitGame( const FString & MapName, const FString & Options, FString & ErrorMessage )
{
	Super::InitGame( MapName, Options, ErrorMessage );
}




void ARagdollControllerGameMode::Tick( float DeltaSeconds )
{
	Super::Tick( DeltaSeconds );

	// handle remotely sent level commands
	HandleRemoteCommands();
}




void ARagdollControllerGameMode::HandleRemoteCommands()
{
	++tmp;

	if( tmp == 25 )
	{
		UE_LOG( LogTemp, Error, TEXT( "*********************************** SNAPSHOT ********************************************************" ) );
	}

	if( tmp % 100 == 0 )
	{
		UE_LOG( LogTemp, Error, TEXT( "*********************************** RESET ********************************************************" ) );
		//ALevelScriptActor::LevelReset();
	}
}
