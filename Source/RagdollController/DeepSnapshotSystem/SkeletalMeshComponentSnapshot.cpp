// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "SkeletalMeshComponentSnapshot.h"




void USkeletalMeshComponentSnapshot::SerializeTarget( FArchive & archive, UActorComponent & target )
{
	Super::SerializeTarget( archive, target );

	UE_LOG( LogTemp, Error, TEXT( "SkelMeshSnap::SerializeTarget. IsLoading: %d, IsSaving: %d" ), archive.IsLoading(), archive.IsSaving() );
}
