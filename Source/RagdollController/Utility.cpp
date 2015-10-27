// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "Utility.h"




void Utility::AddDefaultRootComponent( AActor * actor, FString spriteName )
{
	// create and set a billboard root
	UBillboardComponent * root = actor->CreateDefaultSubobject<UBillboardComponent>( TEXT( "DefaultRoot" ) );
	actor->SetRootComponent( root );

	// set sprite
	ConstructorHelpers::FObjectFinder<UTexture2D> spriteFinder( *spriteName );
	if( spriteFinder.Succeeded() ) root->SetSprite( spriteFinder.Object );
}
