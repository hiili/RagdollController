// Fill out your copyright notice in the Description page of Project Settings.

#pragma once




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

};
