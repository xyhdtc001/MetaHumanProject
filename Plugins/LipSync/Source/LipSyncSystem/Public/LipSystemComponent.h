// Copyright 2022 Stendhal Syndrome Studio. All Rights Reserved.
#pragma once

#include "LipSyncFrameSequence.h"

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Containers/Queue.h"
#include "Runtime/Engine/Classes/Components/ActorComponent.h"
#include "LipSystemComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSyncVisemesDataReadyDelegate);


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LIPSYNCSYSTEM_API ULipSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULipSystemComponent();
	
	UFUNCTION(BlueprintPure, Category = "LipSync", Meta = (Tooltip = "Returns last predicted viseme scores"))
	const TArray<float> &GetVisemes() const;

	UFUNCTION(BlueprintPure, Category = "LipSync", Meta = (Tooltip = "Returns list of viseme names"))
	const TArray<FString> &GetVisemeNames() const;

	UFUNCTION(BlueprintPure, Category = "LipSync", Meta = (Tooltip = "Returns predicted laughter probability"))
	float GetLaughterScore() const;

	UFUNCTION(
		BlueprintCallable,
		Category = "LipSync",
		Meta = (Tooltip = "Set skeletal mesh morph targets to the predicted viseme scores", AutoCreateRefTerm = "MorphTargetNames"))
	void AssignVisemesToMorphTargets(USkeletalMeshComponent *Mesh, const TArray<FString> &InMorphTargetNames);
	
	UPROPERTY(BlueprintAssignable, Category = "LipSync", Meta = (Tooltip = "Event triggered when new prediction is ready"))
    FSyncVisemesDataReadyDelegate OnVisemesReady;

	//-----------------------------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "LipSync", Meta = (Tooltip = "LipSync Sequence to be played"))
	ULipSyncFrameSequence *Sequence;

	UPROPERTY(BlueprintReadonly, Category = "LipSync")
	UAudioComponent *AudioComponent;

	UFUNCTION(BlueprintCallable, Category = "LipSync",
			  Meta = (Tooltip = "Start playback of the canned sequence synchronized with AudioComponent"))
	void Start(UAudioComponent *InAudioComponent, ULipSyncFrameSequence *InSequence);

	UFUNCTION(BlueprintCallable, Category = "LipSync")
	void Stop();

	UFUNCTION(BlueprintCallable, Category = "LipSync", Meta = (Tooltip = "Sets playback sequence property"))
	void SetPlaybackSequence(ULipSyncFrameSequence *InSequence);

	void OnAudioPlaybackPercent(
		const UAudioComponent *,
		const USoundWave *SoundWave,
		float Percent);
	
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "LipSync")
	bool IsPlaying()const {return !bAudioFinished&&Sequence;};

	float GetPercent()const{return CurrentPercent;};

protected:
	UAudioComponent *FindAutoplayAudioComponent() const;

	void OnAudioPlaybackFinished(UAudioComponent* InAudioComponent);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void InitNeutralPose();
	void AppendShutYourMouthSeq(int32 LastIndex);

	float LaughterScore;
	TArray<float> Visemes;
	
	TQueue<FLipSyncFrame> AdditionalFrames;
	static const TArray<FString> VisemeNames;
	
	FDelegateHandle PlaybackPercentHandle;
    FDelegateHandle PlaybackFinishedHandle;
	
	bool bAdditionalFramesAdded;
	bool bAudioFinished;
	
	unsigned IntPos;
	
	float CurrentPercent  = 0;
};
