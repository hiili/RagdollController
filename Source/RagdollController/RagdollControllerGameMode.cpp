// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "RagdollControllerGameMode.h"

#include "Utility.h"




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
		UE_LOG( LogTemp, Warning, TEXT( "   *** Archive initial size: %d, sizeof(FTransform): %d" ), mArchive.TotalSize(), sizeof(FTransform) );

		// find all actors tagged with tag "snapshot"
		check( GetWorld() );
		for( TActorIterator<AActor> iter( GetWorld() ); iter; ++iter )
		{
			if( iter->ActorHasTag( "snapshot" ) )
			{
				UE_LOG( LogTemp, Warning, TEXT( "   *** Found actor with tag 'snapshot', actor name: %s" ), *iter->GetName() );
				UE_LOG( LogTemp, Warning, TEXT( "   *** Taking snapshot from it.." ) );

				mProxyArchive << Utility::as_lvalue( iter->GetTransform() );

				UE_LOG( LogTemp, Warning, TEXT( "   *** Done! archive size: %d" ), mArchive.TotalSize() );
			}
		}
	}

	if( tmp % 100 == 0 )
	{
		UE_LOG( LogTemp, Error, TEXT( "*********************************** RESET ********************************************************" ) );
		//ALevelScriptActor::LevelReset();

		FMemoryReader archiveReader( mArchive, /*bIsPersistent =*/ false );
		FTransform t;

		// find all actors tagged with tag "snapshot"
		check( GetWorld() );
		for( TActorIterator<AActor> iter( GetWorld() ); iter; ++iter )
		{
			if( iter->ActorHasTag( "snapshot" ) )
			{
				UE_LOG( LogTemp, Warning, TEXT( "   *** Found actor with tag 'snapshot', actor name: %s" ), *iter->GetName() );
				UE_LOG( LogTemp, Warning, TEXT( "   *** Restoring from snapshot.." ) );

				archiveReader << t;
				iter->SetActorTransform( t );

				UE_LOG( LogTemp, Warning, TEXT( "   *** Done! archive size: %d" ), mArchive.TotalSize() );
			}
		}
	}
}
