// Copyright 2024 3DGS Research Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUE5_3DGS, Log, All);

/**
 * UE5 3D Gaussian Splatting Plugin Module
 *
 * Provides functionality to export UE5 scenes to 3DGS training datasets:
 * - Scene Capture Module (SCM): Multi-view image capture with camera trajectories
 * - Data Extraction Module (DEM): Depth buffer extraction and point cloud generation
 * - Format Conversion Module (FCM): COLMAP and PLY format writers
 * - Training Module (TRN): Integration with 3DGS training pipelines
 */
class FUE5_3DGSModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Singleton-like access to this module's interface.
	 * @return Returns singleton instance, loading the module on demand if needed.
	 */
	static FUE5_3DGSModule& Get();

	/**
	 * Checks if this module is loaded and ready.
	 * @return True if the module is loaded.
	 */
	static bool IsAvailable();

	/** Get the plugin's base directory */
	FString GetPluginBaseDir() const;

	/** Get the default output directory for exports */
	FString GetDefaultOutputDir() const;

private:
	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();

	TSharedPtr<class FUICommandList> PluginCommands;
};
