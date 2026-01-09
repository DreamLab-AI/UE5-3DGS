// Copyright 2024 3DGS Research Team. All Rights Reserved.

#include "UE5_3DGS.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

#define LOCTEXT_NAMESPACE "FUE5_3DGSModule"

DEFINE_LOG_CATEGORY(LogUE5_3DGS);

void FUE5_3DGSModule::StartupModule()
{
	UE_LOG(LogUE5_3DGS, Log, TEXT("UE5 3D Gaussian Splatting Plugin starting up..."));

	// Register menu extensions for editor integration
	RegisterMenuExtensions();

	UE_LOG(LogUE5_3DGS, Log, TEXT("UE5 3DGS Plugin initialized successfully"));
	UE_LOG(LogUE5_3DGS, Log, TEXT("Plugin Base Directory: %s"), *GetPluginBaseDir());
	UE_LOG(LogUE5_3DGS, Log, TEXT("Default Output Directory: %s"), *GetDefaultOutputDir());
}

void FUE5_3DGSModule::ShutdownModule()
{
	UE_LOG(LogUE5_3DGS, Log, TEXT("UE5 3D Gaussian Splatting Plugin shutting down..."));

	UnregisterMenuExtensions();
}

FUE5_3DGSModule& FUE5_3DGSModule::Get()
{
	return FModuleManager::LoadModuleChecked<FUE5_3DGSModule>("UE5_3DGS");
}

bool FUE5_3DGSModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("UE5_3DGS");
}

FString FUE5_3DGSModule::GetPluginBaseDir() const
{
	return IPluginManager::Get().FindPlugin(TEXT("UE5_3DGS"))->GetBaseDir();
}

FString FUE5_3DGSModule::GetDefaultOutputDir() const
{
	FString OutputDir = FPaths::ProjectSavedDir() / TEXT("3DGS_Export");

	// Ensure directory exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*OutputDir))
	{
		PlatformFile.CreateDirectoryTree(*OutputDir);
	}

	return OutputDir;
}

void FUE5_3DGSModule::RegisterMenuExtensions()
{
	// Menu extensions will be registered here
	// Placeholder for Sprint 4 Editor UI implementation
}

void FUE5_3DGSModule::UnregisterMenuExtensions()
{
	// Menu extensions will be unregistered here
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUE5_3DGSModule, UE5_3DGS)
