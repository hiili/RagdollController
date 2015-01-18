// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "Utility.h"




void Utility::UObjectNameCleanup( UObject & object )
{
	// get the current name of the object
	FString name( object.GetName() );

	// find the first underscore
	int32 firstUnderscoreInd = name.Find( TEXT( "_" ), ESearchCase::IgnoreCase, ESearchDir::FromStart );

	// if found, then crop starting from it (inclusive) and assign to object
	if( firstUnderscoreInd != -1 )
	{
		object.Rename( *name.Left( firstUnderscoreInd ) );
	}
}
