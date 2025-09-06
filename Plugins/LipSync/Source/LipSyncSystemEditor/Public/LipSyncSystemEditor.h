// Copyright 2023 Stendhal Syndrome Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FLipSyncSystemEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
