// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"




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

	/** Search through the components owned by the provided actor and return a component with a type that matches the return type.
	 *
	 *  @return		If there is exactly one component with a matching type, then return a pointer to it. Otherwise return a nullptr. */
	template<typename ComponentType>
	static ComponentType * FindUniqueComponentByClass( const AActor & actor );

};




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
