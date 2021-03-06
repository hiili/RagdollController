// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class RagdollController : ModuleRules
{
	public RagdollController(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "PhysX", "Sockets", "Networking" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

        PublicIncludePaths.AddRange(new string[] { "Engine/Source/ThirdParty/PhysX/PhysX-3.3/include" });

        PrivateIncludePaths.AddRange(new string[] { "RagdollController/ThirdParty/pugixml-1.5", "../ThirdParty/boost-1.57.0" });
        
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");
		// if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
		// {
		//		if (UEBuildConfiguration.bCompileSteamOSS == true)
		//		{
		//			DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
		//		}
		// }
	}
}
