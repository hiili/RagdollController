// Fill out your copyright notice in the Description page of Project Settings.

#pragma once




class Utility
{
public:

	/** Remove all underscore-delimited suffixes from the name of the object. This can be useful for some editor-placed actors that get some automatically
	* generated suffixes during startup. For example, "OwenBP" can become "OwenBP_C_1" during startup. Calling this method on such an actor would return the
	* string "OwenBP". When naming actors, note that you can sometimes prevent the engine from adding suffixes by avoiding existing class names.
	*
	* WARNING: During a Play in editor (PIE) session, you must specify the world when iterating over actors:
	*     TActorIterator<AActor> iter( GetWorld() )
	* Otherwise you will get two instances for most actors (the other one is probably from the editor world), and this method will return identical names
	* for them!
	*/
	static FString CleanupName( FString name );

	/** Reinterpret the argument as an lvalue (if it isn't already). Use with care! From https://stackoverflow.com/questions/3731089/ */
	template<typename T>
	static T & as_lvalue( T && t )
	{
		return t;
	}

	static void AddDefaultRootComponent( AActor * actor, FString spriteName );

};
