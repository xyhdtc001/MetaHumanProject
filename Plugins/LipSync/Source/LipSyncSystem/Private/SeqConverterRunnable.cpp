// Copyright 2022 Stendhal Syndrome Studio. All Rights Reserved.

#include "SeqConverterRunnable.h"

#include "AudioDecompress.h"
#include "AudioDevice.h"
#include "LipSyncWrapper.h"
#include "Templates/UniquePtr.h"
#include "Misc/Paths.h"
#include "SequencePostprocessing.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLss, Log, All);
DEFINE_LOG_CATEGORY(LogLss);

namespace
{
	ULipSyncFrameSequence *MakePlaybackSequence(
		uint8* RawAudioData,
		int32 NumChannels,
		float SampleRate,
		unsigned long long PCMDataSize,
		ULipSyncWrapper &Context
		)
	{
		const auto LipSyncSequenceUpdateFrequency = 100;
		const auto LipSyncSequenceDuration = 1.0f / LipSyncSequenceUpdateFrequency;
		
		auto Sequence = NewObject<ULipSyncFrameSequence>();
		auto PCMData = reinterpret_cast<int16_t *>(RawAudioData);
		PCMDataSize = PCMDataSize / sizeof(int16_t);
		auto ChunkSizeSamples = static_cast<int32>(SampleRate * LipSyncSequenceDuration);
		auto ChunkSize = NumChannels * ChunkSizeSamples;

		float LaughterScore = 0.0f;
		int32_t FrameDelayInMs = 0;
		TArray<float> Visemes;

		TArray<int16_t> Samples;
		Samples.SetNumZeroed(ChunkSize);
		Context.ProcessFrame(Samples.GetData(), ChunkSizeSamples, Visemes, LaughterScore, FrameDelayInMs, NumChannels > 1);

		int32 FrameOffset = (int32)(FrameDelayInMs * SampleRate / 1000 * NumChannels);

		for (int32 Offs = 0; Offs < PCMDataSize + FrameOffset; Offs += ChunkSize)
		{
			int32 RemainingSamples = PCMDataSize - Offs;
			if (RemainingSamples >= ChunkSize)
			{
				Context.ProcessFrame(
					PCMData + Offs,
					ChunkSizeSamples,
					Visemes,
					LaughterScore,
					FrameDelayInMs,
					NumChannels > 1
					);
			}
			else
			{
				if (RemainingSamples > 0)
				{
					memcpy(Samples.GetData(), PCMData + Offs, sizeof(int16_t) * RemainingSamples);
					memset(Samples.GetData() + RemainingSamples, 0, ChunkSize - RemainingSamples);
				}
				else
				{
					memset(Samples.GetData(), 0, ChunkSize);
				}
				Context.ProcessFrame(
					Samples.GetData(),
					ChunkSizeSamples,
					Visemes,
					LaughterScore,
					FrameDelayInMs,
					NumChannels > 1
					);
			}
			if (Offs >= FrameOffset)
			{
				Sequence->Add(Visemes, LaughterScore);
			}
		}
		return Sequence;
	}
}

FSequenceConverterRunnable::FSequenceConverterRunnable()
{
	Thread_ = FRunnableThread::Create( this, TEXT( "LSS thread" ) );
}

FSequenceConverterRunnable::~FSequenceConverterRunnable()
{
	if (Thread_ != nullptr) {
		bThreadInProcess_ = false;
		Thread_->Kill();
		delete Thread_;
		Thread_ = nullptr;
	}
}

void FSequenceConverterRunnable::PutAudioData(const TArray<uint8>& AudioRawData)
{
	InputAudioDataQueue_.Enqueue(AudioRawData);
}

ULipSyncFrameSequence *FSequenceConverterRunnable::GetSeq()
{
	ULipSyncFrameSequence* result = nullptr; 
	if (ResultsSeqQueue_.Dequeue(result))
	{
		return result;
	}
	return nullptr;
}

uint32 FSequenceConverterRunnable::Run()
{
	constexpr int32 ERROR_CODE{-1};
	constexpr float TIMEOUT_SEC{0.05f};
	bThreadInProcess_ = true;
	const FString ModelPath = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
			FPaths::ProjectContentDir(),
			TEXT("3rdparty"),
			TEXT("LSS"),
			TEXT("lipsync_model.pb")
		));
	if (!FPaths::FileExists(ModelPath))
	{
		UE_LOG(LogLss, Error, TEXT("File %s not found!"), *ModelPath);
		return ERROR_CODE;
	}
	TUniquePtr<ULipSyncWrapper> Context;
	while (bThreadInProcess_)
	{
		TArray<uint8> AudioData;
		if (InputAudioDataQueue_.Dequeue(AudioData))
		{
			FWaveModInfo WaveInfo;
			if (WaveInfo.ReadWaveInfo(AudioData.GetData(), AudioData.Num()))
			{
				const uint32 SampleRate = *WaveInfo.pSamplesPerSec;
				if (!Context.IsValid())
				{
					Context = MakeUnique<ULipSyncWrapper>();
					if (!Context->Init(Original, SampleRate,4096, ModelPath))
					{
						return ERROR_CODE;
					}
				}
				auto Sequence = MakePlaybackSequence(
					const_cast<uint8 *>(WaveInfo.SampleDataStart),
					*WaveInfo.pChannels,
					SampleRate,
					WaveInfo.SampleDataSize,
					*Context);
				if (Sequence->Num())
				{
					ResultsSeqQueue_.Enqueue(Sequence);	
				}
				else
				{
					UE_LOG(LogLss, Error, TEXT("FSequenceConverterRunnable::Run. Sequence is null!!!"))
				}
			}
			else
			{
				UE_LOG(LogLss, Error, TEXT("Can't read wave info! %d"), AudioData.Num());
			}
		}
		else
		{
			FPlatformProcess::Sleep( TIMEOUT_SEC );
		}
	}
	return 0;
}

void FSequenceConverterRunnable::Stop()
{
	FRunnable::Stop();
}

void FSequenceConverterRunnable::Exit()
{
	FRunnable::Exit();
}
