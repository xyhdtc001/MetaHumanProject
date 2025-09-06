// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeAudioImporterTypes.h"
#include "MetaHumanPlayerController.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLipStart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLipEnd);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLipTick,float,DeltaTime,float,Percent);

UCLASS(Blueprintable,BlueprintType,meta=(BlueprintSpawnableComponent))
class ULipAnimationCpt : public UActorComponent
{
	GENERATED_BODY()

public:
	
	virtual void OnStartLipSys();
	
	virtual void OnTickLipAnimation(float DeltaTime,float Percent);
	
	virtual  void OnEndLipSys();
	
	//动作类型
	UPROPERTY(BlueprintReadWrite)
	FString AnimationType;

	//表情类型
	UPROPERTY(BlueprintReadWrite)
	FString ExpressionType;

	UPROPERTY(BlueprintAssignable)
	FOnLipStart OnLipStart;
	UPROPERTY(BlueprintAssignable)
	FOnLipEnd OnLipEnd;
	UPROPERTY(BlueprintAssignable)
	FOnLipTick OnLipTick;
};

UCLASS()
class AMetaHumanPlayerController : public APlayerController
{
	GENERATED_BODY()
public:

	AMetaHumanPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(Exec)
	void TestCommand(const FString& Param);


	void PlayHumanSpeech(const TArray<uint8>& SoundData,const FString& ExpressionType,const FString& AnimationType);

	virtual void Tick(float DeltaSeconds) override;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;


	UFUNCTION(BlueprintCallable)
	UTexture2D* LoadTexture2D(FString FilePath);
	
protected:
	UPROPERTY(EditAnywhere)
	class USeqConverterComponent* SeqConverterComponent;

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	class ULipSystemComponent* LipSystemComponent;

	UPROPERTY(EditAnywhere)
	UAudioComponent* AudioComponent;

	UFUNCTION()
	void OnSoundSeqFinish(class ULipSyncFrameSequence* Sequence);

	UFUNCTION()
	void OnSoundImported(class URuntimeAudioImporterLibrary* ImporterLibrary, class UImportedSoundWave* SoundWave,ERuntimeImportStatus Status);

	UPROPERTY(Transient)
	URuntimeAudioImporterLibrary* ImportedInstance;

	UPROPERTY(Transient)
	ULipSyncFrameSequence* ReadyInstanceForPlay;

	UPROPERTY(Transient)
	UImportedSoundWave* ImportedSoundWave;

	UPROPERTY(EditAnywhere)
	ULipSyncFrameSequence* DefaultSeq;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULipAnimationCpt> LipAnimationCpt;

	UPROPERTY(Transient,BlueprintReadOnly)
	TArray<UTexture2D*> LoadedTextures;
private:
	bool bLipPlay = false;

	void AdjustViewPortSize();
};