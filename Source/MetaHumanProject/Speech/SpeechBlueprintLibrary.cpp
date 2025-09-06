#include "SpeechBlueprintLibrary.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Sound/SoundWave.h"
#include "AudioDevice.h"

USpeechManager* USpeechBlueprintLibrary::GetSpeechManager(const UObject* WorldContext)
{
    if (!WorldContext)
    {
        return nullptr;
    }

    const UWorld* World = WorldContext->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }

    return GameInstance->GetSubsystem<USpeechManager>();
}

bool USpeechBlueprintLibrary::QuickSpeakText(const UObject* WorldContext, const FString& Text, const FString& Voice)
{
    USpeechManager* SpeechManager = GetSpeechManager(WorldContext);
    if (!SpeechManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpeechBlueprintLibrary: SpeechManager not available"));
        return false;
    }

    return SpeechManager->SynthesizeText(Text, Voice);
}

bool USpeechBlueprintLibrary::QuickStartListening(const UObject* WorldContext, const FString& Language)
{
    USpeechManager* SpeechManager = GetSpeechManager(WorldContext);
    if (!SpeechManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpeechBlueprintLibrary: SpeechManager not available"));
        return false;
    }

    return SpeechManager->StartSpeechRecognition(Language);
}

bool USpeechBlueprintLibrary::QuickStopListening(const UObject* WorldContext)
{
    USpeechManager* SpeechManager = GetSpeechManager(WorldContext);
    if (!SpeechManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpeechBlueprintLibrary: SpeechManager not available"));
        return false;
    }

    return SpeechManager->StopSpeechRecognition();
}

bool USpeechBlueprintLibrary::IsSpeechSystemInitialized(const UObject* WorldContext)
{
    USpeechManager* SpeechManager = GetSpeechManager(WorldContext);
    return SpeechManager != nullptr;
}

bool USpeechBlueprintLibrary::InitializeSpeechSystem(const UObject* WorldContext, const FString& AppID, const FString& APIKey)
{
    USpeechManager* SpeechManager = GetSpeechManager(WorldContext);
    if (!SpeechManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpeechBlueprintLibrary: SpeechManager not available"));
        return false;
    }

    return SpeechManager->InitializeSpeech(AppID, APIKey);
}

TArray<FString> USpeechBlueprintLibrary::GetAvailableChineseVoices()
{
    TArray<FString> ChineseVoices;
    
    // 科大讯飞常用中文发音人
    ChineseVoices.Add(TEXT("xiaoyan"));      // 小燕，女声
    ChineseVoices.Add(TEXT("xiaoyu"));       // 小宇，男声
    ChineseVoices.Add(TEXT("xiaoxin"));      // 小鑫，男声
    ChineseVoices.Add(TEXT("xiaoli"));       // 小李，女声
    ChineseVoices.Add(TEXT("xiaofeng"));     // 小峰，男声
    ChineseVoices.Add(TEXT("xiaoqian"));     // 小倩，女声
    ChineseVoices.Add(TEXT("xiaochun"));     // 小春，女声
    ChineseVoices.Add(TEXT("xiaolong"));     // 小龙，男声
    ChineseVoices.Add(TEXT("xiaomei"));      // 小美，女声
    ChineseVoices.Add(TEXT("xiaoxue"));      // 小雪，女声
    ChineseVoices.Add(TEXT("xiaoyun"));      // 小云，女声

    return ChineseVoices;
}

TArray<FString> USpeechBlueprintLibrary::GetAvailableLanguages()
{
    TArray<FString> Languages;
    
    // 科大讯飞支持的语言代码
    Languages.Add(TEXT("zh_cn"));            // 中文（普通话）
    Languages.Add(TEXT("zh_tw"));            // 中文（台湾）
    Languages.Add(TEXT("en_us"));            // 英语（美国）
    Languages.Add(TEXT("en_gb"));            // 英语（英国）
    Languages.Add(TEXT("ja_jp"));            // 日语
    Languages.Add(TEXT("ko_kr"));            // 韩语
    Languages.Add(TEXT("ru_ru"));            // 俄语
    Languages.Add(TEXT("fr_fr"));            // 法语
    Languages.Add(TEXT("es_es"));            // 西班牙语
    Languages.Add(TEXT("de_de"));            // 德语

    return Languages;
}

bool USpeechBlueprintLibrary::IsValidVoiceName(const FString& VoiceName)
{
    TArray<FString> ValidVoices = GetAvailableChineseVoices();
    
    // 添加英文发音人
    ValidVoices.Add(TEXT("henry"));          // 英文男声
    ValidVoices.Add(TEXT("mark"));           // 英文男声
    ValidVoices.Add(TEXT("emily"));          // 英文女声
    ValidVoices.Add(TEXT("jason"));          // 英文男声
    
    return ValidVoices.Contains(VoiceName.ToLower());
}

bool USpeechBlueprintLibrary::IsValidLanguageCode(const FString& LanguageCode)
{
    TArray<FString> ValidLanguages = GetAvailableLanguages();
    return ValidLanguages.Contains(LanguageCode.ToLower());
}

USoundWave* USpeechBlueprintLibrary::ConvertAudioDataToSoundWave(const TArray<uint8>& AudioData, int32 SampleRate, int32 NumChannels)
{
    if (AudioData.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpeechBlueprintLibrary: Audio data is empty"));
        return nullptr;
    }

    // 创建新的SoundWave对象
    USoundWave* SoundWave = NewObject<USoundWave>();
    if (!SoundWave)
    {
        UE_LOG(LogTemp, Error, TEXT("SpeechBlueprintLibrary: Failed to create SoundWave object"));
        return nullptr;
    }

    // 设置音频参数
    SoundWave->NumChannels = NumChannels;
    SoundWave->SetSampleRate(SampleRate);
    SoundWave->Duration = static_cast<float>(AudioData.Num()) / (SampleRate * NumChannels * sizeof(int16));

    // 设置音频格式（假设是16位PCM）
    SoundWave->SoundGroup = SOUNDGROUP_Default;
    SoundWave->bLooping = false;

    // 设置RawPCMData
    SoundWave->RawPCMDataSize = AudioData.Num();
    if (SoundWave->RawPCMData != nullptr)
    {
        FMemory::Free(SoundWave->RawPCMData);
    }
    SoundWave->RawPCMData = (uint8*)FMemory::Malloc(SoundWave->RawPCMDataSize);
    FMemory::Memcpy(SoundWave->RawPCMData, AudioData.GetData(), SoundWave->RawPCMDataSize);
    SoundWave->TotalSamples = AudioData.Num() / sizeof(int16);

    // 标记为需要重新构建平台数据
    SoundWave->InvalidateCompressedData();

    UE_LOG(LogTemp, Log, TEXT("SpeechBlueprintLibrary: Created SoundWave with %d bytes, Duration: %.2f seconds"), 
           AudioData.Num(), SoundWave->Duration);

    return SoundWave;
}