#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SpeechManager.h"
#include "SpeechBlueprintLibrary.generated.h"

/**
 * 语音功能蓝图函数库
 * 提供静态函数方便在蓝图中调用语音相关功能
 */
UCLASS()
class METAHUMANPROJECT_API USpeechBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // 获取语音管理器
    UFUNCTION(BlueprintPure, Category = "Speech")
    static USpeechManager* GetSpeechManager(const UObject* WorldContext);

    // 快速语音合成
    UFUNCTION(BlueprintCallable, Category = "Speech|Quick Functions")
    static bool QuickSpeakText(const UObject* WorldContext, const FString& Text, const FString& Voice = TEXT("xiaoyan"));

    // 快速开始语音识别
    UFUNCTION(BlueprintCallable, Category = "Speech|Quick Functions")
    static bool QuickStartListening(const UObject* WorldContext, const FString& Language = TEXT("zh_cn"));

    // 快速停止语音识别
    UFUNCTION(BlueprintCallable, Category = "Speech|Quick Functions")
    static bool QuickStopListening(const UObject* WorldContext);

    // 检查语音系统状态
    UFUNCTION(BlueprintPure, Category = "Speech|Status")
    static bool IsSpeechSystemInitialized(const UObject* WorldContext);

    // 语音设置辅助函数
    UFUNCTION(BlueprintCallable, Category = "Speech|Settings")
    static bool InitializeSpeechSystem(const UObject* WorldContext, const FString& AppID, const FString& APIKey = TEXT(""));

    // 获取可用的语音列表（中文）
    UFUNCTION(BlueprintPure, Category = "Speech|Utilities")
    static TArray<FString> GetAvailableChineseVoices();

    // 获取可用的语言列表
    UFUNCTION(BlueprintPure, Category = "Speech|Utilities")
    static TArray<FString> GetAvailableLanguages();

    // 语音配置验证
    UFUNCTION(BlueprintPure, Category = "Speech|Utilities")
    static bool IsValidVoiceName(const FString& VoiceName);

    UFUNCTION(BlueprintPure, Category = "Speech|Utilities")
    static bool IsValidLanguageCode(const FString& LanguageCode);

    // 音频格式转换辅助
    UFUNCTION(BlueprintCallable, Category = "Speech|Audio")
    static USoundWave* ConvertAudioDataToSoundWave(const TArray<uint8>& AudioData, int32 SampleRate = 16000, int32 NumChannels = 1);
};