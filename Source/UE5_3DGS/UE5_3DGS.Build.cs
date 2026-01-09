// Copyright 2024 3DGS Research Team. All Rights Reserved.

using UnrealBuildTool;

public class UE5_3DGS : ModuleRules
{
	public UE5_3DGS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"RenderCore",
				"RHI",
				"Renderer",
				"Projects",
				"Slate",
				"SlateCore",
				"ImageWrapper",
				"ImageWriteQueue"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorStyle",
				"EditorSubsystem",
				"LevelEditor",
				"PropertyEditor",
				"ToolMenus",
				"WorkspaceMenuStructure",
				"DesktopPlatform",
				"Json",
				"JsonUtilities"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);

		// Enable exceptions for file I/O operations
		bEnableExceptions = true;

		// C++20 for modern features
		CppStandard = CppStandardVersion.Cpp20;
	}
}
