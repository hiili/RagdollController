// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "ObjectSelector.h"




const std::array<EComponentMobility::Type, 4> FComponentSelector::OurMobilityToUEMobilityMap = FComponentSelector::InitializeMobilityMap();

std::array<EComponentMobility::Type, 4> FComponentSelector::InitializeMobilityMap()
{
	static_assert(static_cast<std::size_t>(EObjectSelectorMobilityFilter::ENUM_LAST) - 1 < 4, "Mobility enumeration has too many enumerators!");

	std::array<EComponentMobility::Type, 4> result;
	result[static_cast<std::size_t>(EObjectSelectorMobilityFilter::Static)] = EComponentMobility::Static;
	result[static_cast<std::size_t>(EObjectSelectorMobilityFilter::Stationary)] = EComponentMobility::Stationary;
	result[static_cast<std::size_t>(EObjectSelectorMobilityFilter::Movable)] = EComponentMobility::Movable;
	return result;
}




/* blueprint helper implementations */


bool UComponentSelectorBlueprintHelpers::IsMatchingComponent( const FComponentSelector & ComponentSelector, const UActorComponent * Component )
{
	return Component ? ComponentSelector.IsMatching( *Component ) : false;
}

void UComponentSelectorBlueprintHelpers::FilterComponentArray( const FComponentSelector & ComponentSelector, UPARAM( ref ) TArray<UActorComponent *> & Array )
{
	ComponentSelector.FilterArray( Array );
}

void UComponentSelectorBlueprintHelpers::GetAllMatchingComponentsInWorld( const FComponentSelector & ComponentSelector, const UObject * WorldContextObject, TSubclassOf<UActorComponent> Class, TArray<UActorComponent *> & Out )
{
	if( !WorldContextObject ) return;

	check( GEngine );
	UWorld * world = GEngine->GetWorldFromContextObject( WorldContextObject );
	if( !world ) return;

	Out = ComponentSelector.GetAllMatchingComponents( *world );
}

void UComponentSelectorBlueprintHelpers::GetAllMatchingComponentsInActor( const FComponentSelector & ComponentSelector, const AActor * actor, TSubclassOf<UActorComponent> Class, TArray<UActorComponent *> & Out )
{
	if( !actor ) return;
	Out = ComponentSelector.GetAllMatchingComponents( *actor );
}

bool UActorSelectorBlueprintHelpers::IsMatchingActor( const FActorSelector & ActorSelector, const AActor * Actor )
{
	return Actor ? ActorSelector.IsMatching( *Actor ) : false;
}

void UActorSelectorBlueprintHelpers::FilterActorArray( const FActorSelector & ActorSelector, UPARAM( ref ) TArray<AActor *> & Array )
{
	ActorSelector.FilterArray( Array );
}

void UActorSelectorBlueprintHelpers::GetAllMatchingActors( const FActorSelector & ActorSelector, const UObject * WorldContextObject, TSubclassOf<AActor> Class, TArray<AActor *> & Out )
{
	if( !WorldContextObject ) return;

	check( GEngine );
	UWorld * world = GEngine->GetWorldFromContextObject( WorldContextObject );
	if( !world ) return;

	Out = ActorSelector.GetAllMatchingActors( *world );
}
