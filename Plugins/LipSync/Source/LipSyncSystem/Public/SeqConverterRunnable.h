// Copyright 2022 Stendhal Syndrome Studio. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "LipSyncFrameSequence.h"
#include "HAL/Runnable.h"
#include "Containers/Queue.h"

struct AudioData
{
	TArray<uint8> RawPcmData;
	int32 NumChannels;
	float SampleRate;
	unsigned long long PcmDataSize;
};

class FSequenceConverterRunnable : public FRunnable
{
public:
	FSequenceConverterRunnable();
	virtual ~FSequenceConverterRunnable() override;
	
	void PutAudioData(const TArray<uint8>& AudioRawData);

	ULipSyncFrameSequence* GetSeq();

	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;
private:
	FRunnableThread* Thread_;
	bool bThreadInProcess_;
	
	TQueue<TArray<uint8>> InputAudioDataQueue_;
	TQueue<ULipSyncFrameSequence*> ResultsSeqQueue_;
};
