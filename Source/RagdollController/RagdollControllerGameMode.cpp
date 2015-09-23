// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "RagdollControllerGameMode.h"

#include "Utility.h"
#include "ControlledRagdoll.h"




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

		mProxyArchive.ArIsSaveGame = true;

		// find all actors tagged with tag "snapshot"
		check( GetWorld() );
		for( TActorIterator<AActor> iter( GetWorld() ); iter; ++iter )
		{
			if( iter->ActorHasTag( "snapshot" ) )
			{
				UE_LOG( LogTemp, Warning, TEXT( "   *** Found actor with tag 'snapshot', actor name: %s" ), *iter->GetName() );
				UE_LOG( LogTemp, Warning, TEXT( "   *** Taking snapshot from it.." ) );

				mProxyArchive << Utility::as_lvalue( iter->GetTransform() );
				
				//// doesn't work without the cast either
				//mProxyArchive << Utility::as_lvalue( dynamic_cast<AControlledRagdoll *>(*iter) );

				AActor * currentActor = *iter;
				const TArray<UActorComponent *> & components = currentActor->GetComponents();
				for( auto componentIter = components.CreateConstIterator(); componentIter; ++componentIter )
				{
					UActorComponent * currentComponent = *componentIter;
					int x = 0;   // add a breakpoint here to inspect each 'snapshot'-tagged actor and their components
				}


				UE_LOG( LogTemp, Warning, TEXT( "   *** Done! archive size: %d" ), mArchive.TotalSize() );
			}
		}
	}

	if( tmp % 100 == 0 )
	{
		UE_LOG( LogTemp, Error, TEXT( "*********************************** RESET ********************************************************" ) );
		//ALevelScriptActor::LevelReset();

		FMemoryReader archiveReader{ mArchive, /*bIsPersistent =*/ false };
		FObjectAndNameAsStringProxyArchive proxyArchiveReader{ archiveReader, /*bInLoadIfFindFails =*/ false };
		FTransform t;

		// find all actors tagged with tag "snapshot"
		check( GetWorld() );
		for( TActorIterator<AActor> iter( GetWorld() ); iter; ++iter )
		{
			if( iter->ActorHasTag( "snapshot" ) )
			{
				UE_LOG( LogTemp, Warning, TEXT( "   *** Found actor with tag 'snapshot', actor name: %s" ), *iter->GetName() );
				UE_LOG( LogTemp, Warning, TEXT( "   *** Restoring from snapshot.." ) );

				proxyArchiveReader << t;
				iter->SetActorTransform( t );

				//proxyArchiveReader << Utility::as_lvalue( dynamic_cast<AControlledRagdoll *>(*iter) );

				UE_LOG( LogTemp, Warning, TEXT( "   *** Done! archive size: %d" ), mArchive.TotalSize() );
			}
		}
	}
}
