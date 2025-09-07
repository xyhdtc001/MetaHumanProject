#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "VoiceActivityManager.generated.h"

// VAD模式枚举 - 替代RuntimeAudioImporter的ERuntimeVADMode
UENUM(BlueprintType)
enum class EVADMode : uint8
{
    Quality UMETA(DisplayName = "Quality"),     // 高质量模式
    LowBitrate UMETA(DisplayName = "LowBitrate"), // 低比特率模式  
    Aggressive UMETA(DisplayName = "Aggressive"), // 激进模式
    VeryAggressive UMETA(DisplayName = "VeryAggressive") // 非常激进模式
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceActivityDetected, bool, bVoiceDetected);

/**
 * 语音活动检测管理器
 * 集成RuntimeAudioImporter的VAD功能，检测音频中的语音活动
 */
UCLASS(BlueprintType, Blueprintable)
class METAHUMANPROJECT_API UVoiceActivityManager : public UObject
{
    GENERATED_BODY()

public:
    UVoiceActivityManager();

    // UObject接口
    virtual void BeginDestroy() override;

    /**
     * 初始化VAD检测器
     * @param Mode VAD检测模式（Quality、LowBitrate、Aggressive、VeryAggressive）
     * @param SampleRate 音频采样率（8000、16000、32000、48000）
     * @return 是否初始化成功
     */
    UFUNCTION(BlueprintCallable, Category = "Voice Activity Detection")
    bool InitializeVAD(EVADMode Mode = EVADMode::Aggressive, int32 SampleRate = 16000);

    /**
     * 处理音频数据进行VAD检测
     * @param AudioData 音频数据（16位PCM格式）
     * @param SampleRate 采样率
     * @param NumChannels 声道数
     * @return VAD检测结果：true=检测到语音，false=无语音
     */
    UFUNCTION(BlueprintCallable, Category = "Voice Activity Detection")
    bool ProcessAudioForVAD(const TArray<uint8>& AudioData, int32 SampleRate = 16000, int32 NumChannels = 1);

    /**
     * 处理浮点音频数据进行VAD检测
     * @param AudioData 浮点音频数据
     * @param SampleRate 采样率
     * @param NumChannels 声道数
     * @return VAD检测结果
     */
    UFUNCTION(BlueprintCallable, Category = "Voice Activity Detection")
    bool ProcessFloatAudioForVAD(const TArray<float>& AudioData, int32 SampleRate = 16000, int32 NumChannels = 1);

    /**
     * 设置VAD检测模式
     * @param Mode 新的VAD模式
     * @return 是否设置成功
     */
    UFUNCTION(BlueprintCallable, Category = "Voice Activity Detection")
    bool SetVADMode(EVADMode Mode);

    /**
     * 重置VAD检测器状态
     * @return 是否重置成功
     */
    UFUNCTION(BlueprintCallable, Category = "Voice Activity Detection")
    bool ResetVAD();

    /**
     * 获取当前VAD是否已初始化
     * @return VAD初始化状态
     */
    UFUNCTION(BlueprintPure, Category = "Voice Activity Detection")
    bool IsVADInitialized() const { return bIsInitialized; }

    /**
     * 获取连续语音检测计数
     * @return 连续检测到语音的帧数
     */
    UFUNCTION(BlueprintPure, Category = "Voice Activity Detection")
    int32 GetContinuousVoiceCount() const { return ContinuousVoiceCount; }

    /**
     * 获取连续静音检测计数
     * @return 连续检测到静音的帧数
     */
    UFUNCTION(BlueprintPure, Category = "Voice Activity Detection")
    int32 GetContinuousSilenceCount() const { return ContinuousSilenceCount; }

    // 事件委托
    UPROPERTY(BlueprintAssignable, Category = "Voice Activity Detection|Events")
    FOnVoiceActivityDetected OnVoiceActivityChanged;

    // 配置参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Activity Detection|Settings")
    int32 VoiceStartThreshold = 3;  // 连续多少帧检测到语音才认为开始说话

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Activity Detection|Settings")
    int32 VoiceEndThreshold = 10;   // 连续多少帧检测到静音才认为停止说话

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Activity Detection|Settings")
    bool bEnableSmoothing = true;   // 启用平滑过滤，减少误检

protected:
    // VAD状态跟踪
    bool bIsInitialized;
    bool bCurrentVoiceState;        // 当前语音状态
    bool bLastReportedState;        // 上次报告的状态
    int32 ContinuousVoiceCount;     // 连续语音帧数
    int32 ContinuousSilenceCount;   // 连续静音帧数
    int32 CurrentSampleRate;
    EVADMode CurrentVADMode;        // 当前VAD模式

    // 内部方法
    void UpdateVoiceActivity(bool bVoiceDetected);
    TArray<float> ConvertPCMToFloat(const TArray<uint8>& PCMData);
    
    // 简化的VAD实现
    bool DetectVoiceActivity(const TArray<float>& AudioData);
    float CalculateAudioEnergy(const TArray<float>& AudioData);

private:
    // 线程安全
    FCriticalSection VADCriticalSection;
};