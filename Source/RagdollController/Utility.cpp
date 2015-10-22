// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "Utility.h"




FString Utility::CleanupName( FString name )
{
	// find the first underscore
	int32 firstUnderscoreInd = name.Find( TEXT( "_" ), ESearchCase::IgnoreCase, ESearchDir::FromStart );

	// if found, then crop starting from it (inclusive)
	if( firstUnderscoreInd != -1 )
	{
		//object.Rename( *name.Left( firstUnderscoreInd ) );
		name.RemoveAt( firstUnderscoreInd, name.Len() - firstUnderscoreInd );
	}

	// return the possibly cropped name
	return name;
}




void Utility::AddDefaultRootComponent( AActor * actor, FString spriteName )
{
	// create and set a billboard root
	UBillboardComponent * root = actor->CreateDefaultSubobject<UBillboardComponent>( TEXT( "DefaultRoot" ) );
	actor->SetRootComponent( root );

	// set sprite
	ConstructorHelpers::FObjectFinder<UTexture2D> spriteFinder( *spriteName );
	if( spriteFinder.Succeeded() ) root->SetSprite( spriteFinder.Object );
}
