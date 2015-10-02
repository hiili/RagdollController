// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "DeepSnapshotBase.h"


// Sets default values for this component's properties
UDeepSnapshotBase::UDeepSnapshotBase()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UDeepSnapshotBase::InitializeComponent()
{
	Super::InitializeComponent();

	// try to select a suitable target component
	if( InitialTargetComponentName.IsNone() )
	{
		SelectTargetComponentByType();
	}
	else
	{
		SelectTargetComponentByName();
	}

	UE_LOG( LogTemp, Error, TEXT("target ptr: %p, target name: %s"), TargetComponent, TargetComponent ? *TargetComponent->GetName() : TEXT("") );
}


// Called every frame
void UDeepSnapshotBase::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	// ...
}




bool UDeepSnapshotBase::SelectTargetComponentByName()
{
	// no-op if name is not set
	if( InitialTargetComponentName.IsNone() ) return false;

	// try to find the component with a matching name from the owning actor
	return SelectTargetComponentByPredicate( [this]( UActorComponent * candidate ){
		return candidate && candidate->GetFName() == InitialTargetComponentName;
	} );
}


bool UDeepSnapshotBase::SelectTargetComponentByType()
{
	// try to find a type-matching component from the owning actor
	return SelectTargetComponentByPredicate( [this]( UActorComponent * candidate ){
		return candidate && IsAcceptableTargetType( candidate );
	} );
}




bool UDeepSnapshotBase::SelectTargetComponentByPredicate( std::function<bool( UActorComponent * )> pred )
{
	// try to find an accepted component from the owning actor
	check( GetOwner() );
	UActorComponent * const * result = GetOwner()->GetComponents().FindByPredicate( pred );

	// return false if not found (double-check that the found component pointer is not null)
	if( !result || !*result ) return false;

	// target found; assign to TargetComponent and return true
	TargetComponent = *result;
	return true;
}
