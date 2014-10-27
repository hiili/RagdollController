// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController45.h"
#include "MyGameEngine.h"


UMyGameEngine::UMyGameEngine(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

}




float UMyGameEngine::GetMaxTickRate( float DeltaTime, bool bAllowFrameRateSmoothing )
{
	return 1.e+9;
}
