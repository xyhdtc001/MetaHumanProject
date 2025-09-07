// Copyright 2022 Stendhal Syndrome Studio. All Rights Reserved.
#include "LipSystemComponent.h"

#include "Engine.h"
#include "AudioDevice.h"
#include "SequencePostprocessing.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLssComponent, Log, All);
DEFINE_LOG_CATEGORY(LogLssComponent);

ULipSystemComponent::ULipSystemComponent():
	LaughterScore{0.0f},
	bAdditionalFramesAdded{false},
	bAudioFinished{false},
	IntPos{0}
{
	PrimaryComponentTick.bCanEverTick = true;
	Visemes.Init(0.0f, VisemeNames.Num());
}

const TArray<float>& ULipSystemComponent::GetVisemes() const
{
	return Visemes;
}

const TArray<FString>& ULipSystemComponent::GetVisemeNames() const
{
	return VisemeNames;
}

float ULipSystemComponent::GetLaughterScore() const
{
	return LaughterScore;
}

void ULipSystemComponent::AssignVisemesToMorphTargets(
	USkeletalMeshComponent* Mesh,
	const TArray<FString>& InMorphTargetNames)
{
	auto MorphTargetNames = InMorphTargetNames.Num() > 0 ? InMorphTargetNames : VisemeNames;
	if (Mesh == nullptr)
	{
		Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	}
	if (Mesh == nullptr)
	{
		UE_LOG(LogLssComponent, Error, TEXT("Mesh is NULL"));
		return;
	}
	for (int32 Idx = 0; Idx < MorphTargetNames.Num(); Idx++)
	{
		Mesh->SetMorphTarget(FName(*MorphTargetNames[Idx]), Visemes[Idx]);
	}
}

void ULipSystemComponent::Start(UAudioComponent* InAudioComponent, ULipSyncFrameSequence* InSequence)
{
	bAdditionalFramesAdded = false;
	if (!InSequence)
	{
		UE_LOG(LogLssComponent, Error, TEXT("InSequence is null!"))
		return;
	}
	if (InSequence->FrameSequence.IsEmpty())
	{
		UE_LOG(LogLssComponent, Error, TEXT("InSequence is empty!"))
		return;
	}
	Sequence = InSequence;
	if (AudioComponent != InAudioComponent)
	{
		if (AudioComponent)
		{
			AudioComponent->OnAudioPlaybackPercentNative.Remove(PlaybackPercentHandle);
			AudioComponent->OnAudioFinishedNative.Remove(PlaybackFinishedHandle);
		}
		
		
		AudioComponent = InAudioComponent;

		PlaybackPercentHandle = AudioComponent->OnAudioPlaybackPercentNative.AddUObject(
			this,
			&ULipSystemComponent::OnAudioPlaybackPercent);
	
		PlaybackFinishedHandle = AudioComponent->OnAudioFinishedNative.AddUObject(
			this,
			&ULipSystemComponent::OnAudioPlaybackFinished);
	}
	AdditionalFrames.Empty();
	bAudioFinished = false;
	IntPos = 0;
	AudioComponent->Play();
}

void ULipSystemComponent::Stop()
{
	if (!AudioComponent)
	{
		return;
	}
	AudioComponent->OnAudioPlaybackPercentNative.Remove(PlaybackPercentHandle);
	AudioComponent->OnAudioFinishedNative.Remove(PlaybackFinishedHandle);
	AudioComponent = nullptr;
	InitNeutralPose();
}

void ULipSystemComponent::SetPlaybackSequence(ULipSyncFrameSequence* InSequence)
{
	Sequence = InSequence;
}

void ULipSystemComponent::OnAudioPlaybackPercent(const UAudioComponent*, const USoundWave* SoundWave, float Percent)
{
	if (bAudioFinished)
	{
		return;
	}
	if (!Sequence)
	{
		UE_LOG(LogLssComponent, Error, TEXT("OnAudioPlaybackPercent. Sequence is null - InitNeutralPose"))
		InitNeutralPose();
		return;
	}
	if (IntPos == 0 && static_cast<unsigned>(roundf(Percent)) == 1)
	{
		Percent = 0.0f;	
	}
	CurrentPercent = Percent;
	
	const auto PlayPos = SoundWave->Duration * Percent;
	const auto IntPosTmp = static_cast<unsigned>(roundf(PlayPos * 100.f));
	if (IntPosTmp <= IntPos && IntPosTmp != 0)
	{
		IntPos++;
	}
	else
	{
		IntPos = IntPosTmp;
	}
#if UE_BUILD_DEVELOPMENT
	UE_LOG(LogLssComponent, Verbose, TEXT("--> %d/%d %f"), IntPos, Sequence->Num(), Percent);
#endif
	if (IntPos >= Sequence->Num())
	{
		UE_LOG(LogLssComponent, Verbose, TEXT("OnAudioPlaybackPercent. IntPos >= Sequence->Num - InitNeutralPose"))
		InitNeutralPose();
		return;
	}
	const auto &Frame = (*Sequence)[IntPos];
	LaughterScore = Frame.LaughterScore;
	Visemes = Frame.VisemeScores;
	OnVisemesReady.Broadcast();

	if (static_cast<int32>(Percent) == 1)
	{
		bAudioFinished = true;
	}
}

void ULipSystemComponent::BeginPlay()
{
	Super::BeginPlay();
}

void ULipSystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Stop();
	Super::EndPlay(EndPlayReason);
}

void ULipSystemComponent::InitNeutralPose()
{
	if (Visemes[0] == 1.0f)
	{
		return;
	}

	LaughterScore = 0.0f;
	Visemes[0] = 1.0f;
	for (int idx = 1; idx < Visemes.Num(); ++idx)
	{
		Visemes[idx] = 0.0f;
	}
	OnVisemesReady.Broadcast();
}

void ULipSystemComponent::AppendShutYourMouthSeq(int32 LastIndex)
{
	if (LastIndex < 0)
	{
		return;
	}
	const FLipSyncFrame &LastFrame = (*Sequence)[LastIndex];
	int32 Pos = -1;
	if (SequencePostprocessing::VisemesScoresValidator(LastFrame, LastIndex, Pos))
	{
		TArray<FLipSyncFrame> AdditionalSequenceFrames;
		constexpr int32 AdditionalFramesCount{10};
		constexpr int32 AdditionalInterval{AdditionalFramesCount - 1};
		for (int32 i = 0; i < AdditionalFramesCount; ++i)
		{
			AdditionalSequenceFrames.Add(FLipSyncFrame({
				1.f,
				0.f,
				0.f,
				0.f,
				0.f,
				0.f,
				0.f,
				0.f,
				0.f,
				0.f,
				0.f,
				0.f,
				0.f,
				0.f,
				0.f
			}));
		}
		AdditionalSequenceFrames[0] = LastFrame;
		SequencePostprocessing::Postprocessing(0, AdditionalInterval, AdditionalInterval, AdditionalSequenceFrames);
		for (int32 i = 0; i < AdditionalFramesCount; ++i)
		{
			AdditionalFrames.Enqueue(AdditionalSequenceFrames[i]);
		}
	}
	bAdditionalFramesAdded = true;
}

void ULipSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bAudioFinished)
	{
		if (IntPos < Sequence->Num())
		{
			const auto &Frame = (*Sequence)[IntPos];
			if (!Frame.VisemeScores.IsEmpty() && Frame.VisemeScores[0] != 1.f && !bAdditionalFramesAdded)
			{
				AppendShutYourMouthSeq(IntPos);
			}
			FLipSyncFrame AdditionalFrame;
			if (AdditionalFrames.Dequeue(AdditionalFrame))
			{
				LaughterScore = AdditionalFrame.LaughterScore;
				Visemes = AdditionalFrame.VisemeScores;
				OnVisemesReady.Broadcast();				
			}	
		}
	}
}

UAudioComponent* ULipSystemComponent::FindAutoplayAudioComponent() const
{
	TArray<UAudioComponent *> AudioComponents;
	GetOwner()->GetComponents<UAudioComponent>(AudioComponents);

	if (AudioComponents.Num() == 0)
	{
		return nullptr;
	}
	for (const auto &AudioComponentCandidate : AudioComponents)
	{
		if (AudioComponentCandidate->bAutoActivate)
			return AudioComponentCandidate;
	}
	return nullptr;
}

void ULipSystemComponent::OnAudioPlaybackFinished(UAudioComponent* InAudioComponent)
{
	bAudioFinished = true;
}

const TArray<FString> ULipSystemComponent::VisemeNames = {
	FString(TEXT("sil")), 
	FString(TEXT("PP")),
	FString(TEXT("FF")),
	FString(TEXT("TH")),
	FString(TEXT("DD")),
	FString(TEXT("kk")),
	FString(TEXT("CH")),
	FString(TEXT("SS")),
	FString(TEXT("nn")),
	FString(TEXT("RR")),
	FString(TEXT("aa")),
	FString(TEXT("E")),
	FString(TEXT("ih")),
	FString(TEXT("oh")),
	FString(TEXT("ou")),
};

