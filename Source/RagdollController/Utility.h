// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include <utility>




class Utility
{
public:

	/** Reinterpret the argument as an lvalue (if it isn't already). Use with care! From https://stackoverflow.com/questions/3731089/ */
	template<typename T>
	static T & as_lvalue( T && t )
	{
		return t;
	}

	static void AddDefaultRootComponent( AActor * actor, FString spriteName );

	/** Get a pretty-printed name of the given object, or "(null)" if a nullptr was provided. */
	static FString GetName( const UObject * object )
	{
		return object ? *object->GetPathName( object->GetWorld() ) : FString( "(null)" );
	}

	/** Find all actors that are of the specified type. This is a convenience wrapper around UGameplayStatics::GetAllActorsOfClass().
	 *
	 * @param ActorType				The type of the target actor
	 * @param worldContextObject	An object that defines the world from which to look for the target actor
	 * @return						An array of pointers to the found actors */
	template<typename ActorType>
	static TArray<ActorType *> FindActorsByClass( const UObject & worldContextObject );

	/** Find a unique actor from the world by type.
	 *
	 * @param ActorType				The type of the target actor
	 * @param worldContextObject	An object that defines the world from which to look for the target actor
	 * @return						Pair.first is a pointer to the found actor, or nullptr if no suitable actor was found or there were several ones.
	 *								Pair.second is the number of found suitable actors. */
	template<typename ActorType>
	static std::pair<ActorType *, int> FindUniqueActorByClass( const UObject & worldContextObject );

	/** Search through the components owned by the provided actor and return a component with a type that matches the return type.
	 *
	 *  @return		If there is exactly one component with a matching type, then return a pointer to it. Otherwise return a nullptr. */
	template<typename ComponentType>
	static ComponentType * FindUniqueComponentByClass( const AActor & actor );

};




template<typename ActorType>
TArray<ActorType *> Utility::FindActorsByClass( const UObject & worldContextObject )
{
	static_assert(TPointerIsConvertibleFromTo<ActorType, const AActor>::Value,
		"'ActorType' template parameter to Utility::FindUniqueActorByClass must be derived from AActor");

	TArray<AActor *> actors;
	UGameplayStatics::GetAllActorsOfClass( const_cast<UObject *>(&worldContextObject), ActorType::StaticClass(), actors );
		// const cast: GetAllActorsOfClass forwards the context object to GEngine->GetWorldFromContextObject, which takes it as a const (as of UE 4.9)

	// cast results
	TArray<ActorType *> castActors;
	castActors.Reserve( actors.Num() );
	for( auto elem : actors ) castActors.Emplace( dynamic_cast<ActorType *>(elem) );
	return castActors;
}


template<typename ActorType>
std::pair<ActorType *, int> Utility::FindUniqueActorByClass( const UObject & worldContextObject )
{
	static_assert(TPointerIsConvertibleFromTo<ActorType, const AActor>::Value,
		"'ActorType' template parameter to Utility::FindUniqueActorByClass must be derived from AActor");

	TArray<AActor *> actors;
	UGameplayStatics::GetAllActorsOfClass( const_cast<UObject *>(&worldContextObject), ActorType::StaticClass(), actors );
		// const cast: GetAllActorsOfClass forwards the context object to GEngine->GetWorldFromContextObject, which takes it as a const (as of UE 4.9)

	// no matches or multiple matches? return failure
	if( actors.Num() != 1 ) return{ nullptr, actors.Num() };

	// exactly one match -> return it
	ActorType * actor = dynamic_cast<ActorType *>(actors[0]);
	check( actor );   // the cast should always succeed, because we searched only actors of this class
	return{ actor, 1 };
}


template<typename ComponentType>
ComponentType * Utility::FindUniqueComponentByClass( const AActor & actor )
{
	static_assert(TPointerIsConvertibleFromTo<ComponentType, const UActorComponent>::Value,
		"'ComponentType' template parameter to Utility::FindUniqueComponentByClass must be derived from UActorComponent");

	ComponentType * selected = 0;
	for( UActorComponent * const candidate : actor.GetComponents() )
	{
		ComponentType * castCandidate = dynamic_cast<ComponentType *>(candidate);
		if( castCandidate )
		{
			// multiple matches? return failure
			if( selected ) return nullptr;

			// first hit -> store
			selected = castCandidate;
		}
	}

	return selected;
}
