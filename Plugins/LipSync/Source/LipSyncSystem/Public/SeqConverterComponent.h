// Copyright 2023 Stendhal Syndrome Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeqConverterRunnable.h"
#include "Templates/UniquePtr.h"
#include "SeqConverterComponent.generated.h"

class ULipSyncFrameSequence;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNewSequence, ULipSyncFrameSequence*, Sequence);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LIPSYNCSYSTEM_API USeqConverterComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USeqConverterComponent();

	UFUNCTION(BlueprintCallable, meta = ( DisplayName = "Add audio data for convert", Category = "SequenceConverter" ))
	void PutAudioData(const TArray<uint8>& AudioData);

	UPROPERTY(BlueprintAssignable, meta = ( DisplayName = "FOnNewSequence", Category = "SequenceConverter" ))
	FOnNewSequence OnNewSequence;
protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;
private:	
	TUniquePtr<FSequenceConverterRunnable> SeqConverterWorker_;
	bool bInitialized{false};
};
