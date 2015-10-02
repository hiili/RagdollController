// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "SkeletalMeshComponentSnapshot.h"




void USkeletalMeshComponentSnapshot::Snapshot()
{
	UE_LOG( LogTemp, Error, TEXT( "SkelMeshSnap::Snapshot" ) );
}




void USkeletalMeshComponentSnapshot::Recall()
{
	UE_LOG( LogTemp, Error, TEXT( "SkelMeshSnap::Recall" ) );
}
