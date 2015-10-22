// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "StaticMeshComponentSnapshot.h"




void UStaticMeshComponentSnapshot::SerializeTarget( FArchive & archive, UActorComponent & target )
{
	Super::SerializeTarget( archive, target );
}




bool UStaticMeshComponentSnapshot::IsAcceptableTargetType( UActorComponent * targetCandidate ) const
{
	return dynamic_cast<UStaticMeshComponent *>(targetCandidate) != nullptr;
}
