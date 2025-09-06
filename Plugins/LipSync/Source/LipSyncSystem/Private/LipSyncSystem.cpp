// Copyright 2022 Stendhal Syndrome Studio. All Rights Reserved.

#include "LipSyncSystem.h"

#include "LipSyncWrapper.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

#define LOCTEXT_NAMESPACE "FLipSyncSystemModule"

void FLipSyncSystemModule::StartupModule()
{
	ULipSyncWrapper::LoadDll();
}

void FLipSyncSystemModule::ShutdownModule()
{
	ULipSyncWrapper::UnloadDll();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLipSyncSystemModule, LipSyncSystem)
