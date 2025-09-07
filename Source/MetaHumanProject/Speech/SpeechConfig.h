#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VoiceActivityManager.h"
#include "SpeechConfig.generated.h"

/**
 * 语音系统全局配置
 */
USTRUCT(BlueprintType)
struct METAHUMANPROJECT_API FSpeechSystemConfig
{
    GENERATED_BODY()

    // iFlytek SDK 配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDK", meta = (DisplayName = "应用ID"))
    FString AppID = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDK", meta = (DisplayName = "API密钥"))
    FString APIKey = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDK", meta = (DisplayName = "默认语言"))
    FString DefaultLanguage = TEXT("zh_cn");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDK", meta = (DisplayName = "默认声音"))
    FString DefaultVoice = TEXT("xiaoyan");

    // 语音识别配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recognition", meta = (DisplayName = "最大语音时长(秒)", ClampMin = "10", ClampMax = "120"))
    float MaxSpeechDuration = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recognition", meta = (DisplayName = "最大缓冲块数", ClampMin = "100", ClampMax = "10000"))
    int32 MaxBufferChunks = 3000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recognition", meta = (DisplayName = "重连尝试次数", ClampMin = "1", ClampMax = "10"))
    int32 MaxReconnectAttempts = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recognition", meta = (DisplayName = "重连延迟(秒)", ClampMin = "1", ClampMax = "30"))
    float ReconnectDelay = 5.0f;

    // VAD 配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VAD", meta = (DisplayName = "VAD模式"))
    EVADMode VADMode = EVADMode::Aggressive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VAD", meta = (DisplayName = "语音开始阈值(帧)", ClampMin = "1", ClampMax = "20"))
    int32 VoiceStartThreshold = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VAD", meta = (DisplayName = "语音结束阈值(帧)", ClampMin = "5", ClampMax = "100"))
    int32 VoiceEndThreshold = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VAD", meta = (DisplayName = "启用平滑处理"))
    bool bEnableSmoothing = true;

    // 音频配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (DisplayName = "采样率"))
    int32 SampleRate = 16000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (DisplayName = "声道数", ClampMin = "1", ClampMax = "2"))
    int32 Channels = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (DisplayName = "缓冲区大小", ClampMin = "240", ClampMax = "4800"))
    int32 BufferSize = 960;

    // 调试配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "启用详细日志"))
    bool bEnableVerboseLogging = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "启用性能统计"))
    bool bEnablePerformanceStats = true;

    FSpeechSystemConfig()
    {
        // 从环境变量或配置文件读取默认值
        FString EnvironmentAppID = FPlatformMisc::GetEnvironmentVariable(TEXT("IFLYTEK_APPID"));
        if (!EnvironmentAppID.IsEmpty())
        {
            AppID = EnvironmentAppID;
        }
    }
};

/**
 * 语音性能统计信息
 */
USTRUCT(BlueprintType)
struct METAHUMANPROJECT_API FSpeechStatistics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Statistics", meta = (DisplayName = "总识别次数"))
    int32 TotalRecognitions = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Statistics", meta = (DisplayName = "成功识别次数"))
    int32 SuccessfulRecognitions = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Statistics", meta = (DisplayName = "失败识别次数"))
    int32 FailedRecognitions = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Statistics", meta = (DisplayName = "平均识别时间(秒)"))
    float AverageRecognitionTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Statistics", meta = (DisplayName = "音频溢出次数"))
    int32 AudioOverflowCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Statistics", meta = (DisplayName = "网络错误次数"))
    int32 NetworkErrorCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Statistics", meta = (DisplayName = "长语音分段次数"))
    int32 LongSpeechSegmentCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Statistics", meta = (DisplayName = "总音频处理时长(秒)"))
    float TotalAudioDuration = 0.0f;

    void Reset()
    {
        TotalRecognitions = 0;
        SuccessfulRecognitions = 0;
        FailedRecognitions = 0;
        AverageRecognitionTime = 0.0f;
        AudioOverflowCount = 0;
        NetworkErrorCount = 0;
        LongSpeechSegmentCount = 0;
        TotalAudioDuration = 0.0f;
    }

    float GetSuccessRate() const
    {
        return TotalRecognitions > 0 ? (float)SuccessfulRecognitions / TotalRecognitions : 0.0f;
    }
};

/**
 * 语音系统开发者设置
 * 在项目设置中可配置
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Speech System Settings"))
class METAHUMANPROJECT_API USpeechSystemSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    USpeechSystemSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    // UDeveloperSettings interface
    virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }
    virtual FText GetSectionText() const override { return NSLOCTEXT("SpeechSystemSettings", "SectionText", "Speech System"); }
    virtual FText GetSectionDescription() const override { return NSLOCTEXT("SpeechSystemSettings", "SectionDescription", "Configuration for the Speech Recognition System"); }

    // 获取全局配置实例
    UFUNCTION(BlueprintPure, Category = "Speech System")
    static USpeechSystemSettings* Get() { return GetMutableDefault<USpeechSystemSettings>(); }

    // 配置数据
    UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Speech System", meta = (DisplayName = "语音系统配置"))
    FSpeechSystemConfig SpeechConfig;

    // 运行时统计（不保存到配置文件）
    UPROPERTY(BlueprintReadOnly, Category = "Statistics", meta = (DisplayName = "运行时统计"))
    FSpeechStatistics RuntimeStatistics;

    // 配置管理函数
    UFUNCTION(BlueprintCallable, Category = "Speech System", meta = (DisplayName = "重置统计数据"))
    void ResetStatistics() { RuntimeStatistics.Reset(); }

    UFUNCTION(BlueprintPure, Category = "Speech System", meta = (DisplayName = "获取配置"))
    const FSpeechSystemConfig& GetConfig() const { return SpeechConfig; }

    UFUNCTION(BlueprintCallable, Category = "Speech System", meta = (DisplayName = "更新配置"))
    void UpdateConfig(const FSpeechSystemConfig& NewConfig) 
    { 
        SpeechConfig = NewConfig; 
        SaveConfig();
    }
};