// Copyright 2022 Stendhal Syndrome Studio. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FLipSyncSystemModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
