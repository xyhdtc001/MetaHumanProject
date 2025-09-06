// Copyright 2022 Stendhal Syndrome Studio. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

typedef enum {
	Original,
	Enhanced,
	EnhancedWithLaughter
} LipSyncContextProvider;

class LIPSYNCSYSTEM_API ULipSyncWrapper
{
public:
	bool Init(LipSyncContextProvider Provider,
		int SampleRate = 48000,
		int BufferSize = 4096,
		FString ModelPath = FString(), 
		bool bAccelerate = true);
	~ULipSyncWrapper();

	void ProcessFrame(
		const int16_t *Data,
		int32 DataSize, 
		TArray<float> &Visemes, 
		float &LaughterScore,
		int32_t &FrameDelay,
		bool bStereo = false);

	static bool LoadDll();
	static void UnloadDll();

private:
	uint32 LipSyncContext{EnhancedWithLaughter};
	inline static void* LibraryHandle{nullptr};
	inline static bool bIsDllLoaded{false};
};
