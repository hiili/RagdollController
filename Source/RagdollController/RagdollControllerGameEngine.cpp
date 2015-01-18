// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "RagdollControllerGameEngine.h"


URagdollControllerGameEngine::URagdollControllerGameEngine()
	: Super( FObjectInitializer() )
{

}




float URagdollControllerGameEngine::GetMaxTickRate( float DeltaTime, bool bAllowFrameRateSmoothing ) const
{
	return 1.e+9;
}
