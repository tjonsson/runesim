// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class RuneSim : ModuleRules
{
	public RuneSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;
	
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore",
			"Json",
            "JsonUtilities",
            "WebSockets"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
            "CesiumRuntime",
            "AnimationCore",
            "HeadMountedDisplay",
            "AirSim",
            "UMG",
            "Slate",
            "SlateCore",
            "AssetRegistry",
            "Niagara",
            "PixelStreaming2",
            "PixelStreaming2Core",
            "PixelCapture",
            "RHI",
            "RenderCore",
            "ImageWrapper"
        });

        if (Target.bBuildEditor)
            PrivateDependencyModuleNames.Add("NiagaraEditor");

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
