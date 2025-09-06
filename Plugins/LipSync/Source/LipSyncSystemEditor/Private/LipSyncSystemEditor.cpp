// Copyright 2023 Stendhal Syndrome Studio. All Rights Reserved.

#include "LipSyncSystemEditor.h"

#include "AudioDecompress.h"
#include "AudioDevice.h"
#include "ContentBrowserModule.h"
#include "LipSyncFrameSequence.h"
#include "LipSyncWrapper.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "FLipSyncSystemEditorModule"

DECLARE_LOG_CATEGORY_EXTERN(LogLssEditor, Log, All);
DEFINE_LOG_CATEGORY(LogLssEditor);

namespace
{
	constexpr auto LipSyncSequenceUpdateFrequency = 100;
	constexpr auto LipSyncSequenceDuration = 1.0f / LipSyncSequenceUpdateFrequency;
	
	TArray<uint8> SoundWaveToBytes(const USoundWave* Audio)
	{
		TArray<uint8> BytesArr;
		if (IsValid(Audio))
		{
			TArray<uint8> AudioBytes;
			TArray<uint8> AudioHeader;
			uint32 SampleRate;
			uint16 Channels;

			if (Audio->GetImportedSoundWaveData(AudioBytes, SampleRate, Channels))
			{
				BytesArr.Append(AudioBytes);
			}
		}
		return BytesArr;
	}
	
	bool LipSyncProcessSoundWave(const FAssetData &SoundWaveAsset)
	{
		auto SoundWave = Cast<USoundWave>(SoundWaveAsset.GetAsset());
		if (!SoundWave)
		{
			UE_LOG(LogLssEditor, Error, TEXT("Can't get SoundWave data"));
			return false;
		}
		if (SoundWave->NumChannels > 2)
		{
			UE_LOG(LogLssEditor, Error, TEXT("Can't process SoundWave: only mono and stereo streams are supported"));
			return false;
		}

		auto SequenceName = FString::Printf(TEXT("%s_LipSyncSequence"), *SoundWaveAsset.AssetName.ToString());
		auto SequencePath = FString::Printf(TEXT("%s_LipSyncSequence"), *SoundWaveAsset.PackageName.ToString());
		auto SequencePackage = CreatePackage(*SequencePath);
		auto Sequence = NewObject<ULipSyncFrameSequence>(SequencePackage, *SequenceName, RF_Public | RF_Standalone);

		auto SoundData = SoundWaveToBytes(SoundWave);
		
		auto NumChannels = SoundWave->NumChannels;
		auto SampleRate = SoundWave->GetSampleRateForCurrentPlatform();
		auto PCMDataSize = SoundData.Num() / sizeof(int16_t);
		auto PCMData = reinterpret_cast<int16_t *>(SoundData.GetData());
		auto ChunkSizeSamples = static_cast<int>(SampleRate * LipSyncSequenceDuration);
		auto ChunkSize = NumChannels * ChunkSizeSamples;
		
		const FString ModelPath = FPaths::ConvertRelativePathToFull(
				FPaths::Combine(
				FPaths::ProjectContentDir(),
				TEXT("3rdparty"),
				TEXT("LSS"),
				TEXT("lipsync_model.pb")
			));
		ULipSyncWrapper Context;
		if (!Context.Init(LipSyncContextProvider::Original, SampleRate, 4096, ModelPath))
		{
			return false;
		}

		float LaughterScore = 0.0f;
		int32_t FrameDelayInMs = 0;
		TArray<float> Visemes;

		TArray<int16_t> Samples;
		Samples.SetNumZeroed(ChunkSize);
		Context.ProcessFrame(Samples.GetData(), ChunkSizeSamples, Visemes, LaughterScore, FrameDelayInMs, NumChannels > 1);

		int32 FrameOffset = (int32)(FrameDelayInMs * SampleRate / 1000 * NumChannels);

		FScopedSlowTask SlowTask(
			PCMDataSize + FrameOffset,
			FText::Format(
				NSLOCTEXT("NSLT_LipSyncPlugin", "GeneratingLipSyncSequence", "Generating LipSync sequence for {0}..."),
				FText::FromName(SoundWaveAsset.AssetName)
				));
		SlowTask.MakeDialog();
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
					NumChannels > 1);
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
					NumChannels > 1);
			}

			SlowTask.EnterProgressFrame(ChunkSize);
			if (SlowTask.ShouldCancel())
			{
				return false;
			}
			if (Offs >= FrameOffset)
			{
				Sequence->Add(Visemes, LaughterScore);
			}
		}
		FAssetRegistryModule::AssetCreated(Sequence);
		return Sequence->MarkPackageDirty();
	}
	
	void LipSyncSystemCreateSequence(const TArray<FAssetData> SelectedSoundAssets)
	{
		for (auto &SoundWaveAsset : SelectedSoundAssets)
		{
			LipSyncProcessSoundWave(SoundWaveAsset);
		}
	}

	void LipSyncSystemContextMenuExtension(FMenuBuilder &MenuBuilder, const TArray<FAssetData> SelectedSoundWavesPath)
    {
    	MenuBuilder.AddMenuEntry(
    		NSLOCTEXT("NSLT_LipSyncSystemPlugin", "CreateLipSyncSequence_Menu",
    				  "LipSyncSystem. Generate LipSyncSequence"),
    		NSLOCTEXT("NSLT_LipSyncSystemPlugin", "CreateLipSyncSequence_Tooltip",
    				  "LipSyncSystem. Creates sequence asset that could be used by LipSyncSystem"),
    		FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(LipSyncSystemCreateSequence, SelectedSoundWavesPath)));
    }

	TSharedRef<FExtender> LipSyncContextMenuExtender(const TArray<FAssetData> &SelectedAssets)
	{
		TSharedRef<FExtender> Extender(new FExtender());
		TArray<FAssetData> SelectedSoundWaveAssets;
		for (auto &Asset : SelectedAssets)
		{
			if (Asset.AssetClassPath.ToString().Contains(TEXT("SoundWave")))
			{
				SelectedSoundWaveAssets.Add(Asset);
			}
		}
		if (SelectedSoundWaveAssets.Num() > 0)
		{
			Extender->AddMenuExtension(
				"GetAssetActions", EExtensionHook::After, TSharedPtr<FUICommandList>(),
				FMenuExtensionDelegate::CreateStatic(LipSyncSystemContextMenuExtension, SelectedSoundWaveAssets));
		}
		return Extender;
	}
}

void FLipSyncSystemEditorModule::StartupModule()
{
	auto &ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	auto &ContextMenuExtenders = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	ContextMenuExtenders.Add(
		FContentBrowserMenuExtender_SelectedAssets::CreateStatic(LipSyncContextMenuExtender));
}

void FLipSyncSystemEditorModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FLipSyncSystemEditorModule, LipSyncSystemEditor)