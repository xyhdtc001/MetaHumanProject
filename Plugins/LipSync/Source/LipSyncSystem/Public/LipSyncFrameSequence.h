// Copyright 2022 Stendhal Syndrome Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LipSyncFrameSequence.generated.h"

USTRUCT()
struct LIPSYNCSYSTEM_API FLipSyncFrame
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<float> VisemeScores;

	UPROPERTY()
	float LaughterScore;

	FLipSyncFrame(const TArray<float> &visemes = {}, float laughterScore = 0.0f)
		: VisemeScores(visemes), LaughterScore(laughterScore)
	{
	}
};

UCLASS(BlueprintType)
class LIPSYNCSYSTEM_API ULipSyncFrameSequence : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FLipSyncFrame> FrameSequence;
	unsigned Num() const { return FrameSequence.Num(); }
	void Add(const TArray<float> &Visemes, float LaughterScore) { FrameSequence.Emplace(Visemes, LaughterScore); }
	const FLipSyncFrame &operator[](unsigned idx) const { return FrameSequence[idx]; }
};
