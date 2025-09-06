#include "VoiceActivityManager.h"
#include "Engine/Engine.h"
#include "Async/TaskGraphInterfaces.h"

UVoiceActivityManager::UVoiceActivityManager()
    : bIsInitialized(false)
    , bCurrentVoiceState(false)
    , bLastReportedState(false)
    , ContinuousVoiceCount(0)
    , ContinuousSilenceCount(0)
    , CurrentSampleRate(16000)
    , CurrentVADMode(EVADMode::Aggressive)
{
}

void UVoiceActivityManager::BeginDestroy()
{
    bIsInitialized = false;
    Super::BeginDestroy();
}

bool UVoiceActivityManager::InitializeVAD(EVADMode Mode, int32 SampleRate)
{
    FScopeLock Lock(&VADCriticalSection);
    
    // 验证采样率
    if (SampleRate != 8000 && SampleRate != 16000 && SampleRate != 32000 && SampleRate != 48000)
    {
        UE_LOG(LogTemp, Error, TEXT("VoiceActivityManager: Unsupported sample rate: %d"), SampleRate);
        return false;
    }
    
    CurrentVADMode = Mode;
    CurrentSampleRate = SampleRate;
    
    // 重置状态
    bCurrentVoiceState = false;
    bLastReportedState = false;
    ContinuousVoiceCount = 0;
    ContinuousSilenceCount = 0;
    
    bIsInitialized = true;
    
    UE_LOG(LogTemp, Log, TEXT("VoiceActivityManager: VAD initialized with mode %d, sample rate %d"), 
           static_cast<int32>(Mode), SampleRate);
    
    return true;
}

bool UVoiceActivityManager::ProcessAudioForVAD(const TArray<uint8>& AudioData, int32 SampleRate, int32 NumChannels)
{
    if (!bIsInitialized || AudioData.Num() == 0)
    {
        return false;
    }
    
    // 转换PCM数据为float
    TArray<float> FloatAudioData = ConvertPCMToFloat(AudioData);
    return ProcessFloatAudioForVAD(FloatAudioData, SampleRate, NumChannels);
}

bool UVoiceActivityManager::ProcessFloatAudioForVAD(const TArray<float>& AudioData, int32 SampleRate, int32 NumChannels)
{
    if (!bIsInitialized || AudioData.Num() == 0)
    {
        return false;
    }
    
    FScopeLock Lock(&VADCriticalSection);
    
    // 执行VAD检测
    bool bVoiceDetected = DetectVoiceActivity(AudioData);
    
    // 更新语音活动状态
    UpdateVoiceActivity(bVoiceDetected);
    
    return bVoiceDetected;
}

bool UVoiceActivityManager::SetVADMode(EVADMode Mode)
{
    FScopeLock Lock(&VADCriticalSection);
    
    CurrentVADMode = Mode;
    
    UE_LOG(LogTemp, Log, TEXT("VoiceActivityManager: VAD mode changed to %d"), static_cast<int32>(Mode));
    
    return true;
}

bool UVoiceActivityManager::ResetVAD()
{
    FScopeLock Lock(&VADCriticalSection);
    
    bCurrentVoiceState = false;
    bLastReportedState = false;
    ContinuousVoiceCount = 0;
    ContinuousSilenceCount = 0;
    
    UE_LOG(LogTemp, Log, TEXT("VoiceActivityManager: VAD state reset"));
    
    return true;
}

void UVoiceActivityManager::UpdateVoiceActivity(bool bVoiceDetected)
{
    // 更新连续计数
    if (bVoiceDetected)
    {
        ContinuousVoiceCount++;
        ContinuousSilenceCount = 0;
    }
    else
    {
        ContinuousSilenceCount++;
        ContinuousVoiceCount = 0;
    }
    
    bool bNewVoiceState = bCurrentVoiceState;
    
    if (bEnableSmoothing)
    {
        // 语音开始检测：连续检测到语音帧数达到阈值
        if (!bCurrentVoiceState && ContinuousVoiceCount >= VoiceStartThreshold)
        {
            bNewVoiceState = true;
        }
        // 语音结束检测：连续检测到静音帧数达到阈值  
        else if (bCurrentVoiceState && ContinuousSilenceCount >= VoiceEndThreshold)
        {
            bNewVoiceState = false;
        }
    }
    else
    {
        // 无平滑处理，直接使用检测结果
        bNewVoiceState = bVoiceDetected;
    }
    
    // 状态变化时触发事件
    if (bNewVoiceState != bCurrentVoiceState)
    {
        bCurrentVoiceState = bNewVoiceState;
        
        if (bCurrentVoiceState != bLastReportedState)
        {
            bLastReportedState = bCurrentVoiceState;
            
            // 在游戏线程中触发事件
            AsyncTask(ENamedThreads::GameThread, [this, bCurrentVoiceState = bCurrentVoiceState]()
            {
                OnVoiceActivityChanged.Broadcast(bCurrentVoiceState);
            });
            
            UE_LOG(LogTemp, Log, TEXT("VoiceActivityManager: Voice activity changed to %s"), 
                   bCurrentVoiceState ? TEXT("Active") : TEXT("Inactive"));
        }
    }
}

TArray<float> UVoiceActivityManager::ConvertPCMToFloat(const TArray<uint8>& PCMData)
{
    TArray<float> FloatData;
    const int32 SampleCount = PCMData.Num() / sizeof(int16);
    FloatData.Reserve(SampleCount);
    
    const int16* SampleData = reinterpret_cast<const int16*>(PCMData.GetData());
    
    for (int32 i = 0; i < SampleCount; i++)
    {
        // 转换16位PCM到-1.0到1.0的浮点数
        float FloatSample = static_cast<float>(SampleData[i]) / 32768.0f;
        FloatData.Add(FloatSample);
    }
    
    return FloatData;
}

bool UVoiceActivityManager::DetectVoiceActivity(const TArray<float>& AudioData)
{
    // 简化的VAD实现 - 基于能量检测
    float Energy = CalculateAudioEnergy(AudioData);
    
    // 根据VAD模式设置不同的阈值 - 优化阈值以减少误检和漏检
    float Threshold = 0.008f; // 默认阈值，适合大多数环境
    
    switch (CurrentVADMode)
    {
        case EVADMode::Quality:
            Threshold = 0.015f; // 较高阈值，减少误检，适合安静环境
            break;
        case EVADMode::LowBitrate:
            Threshold = 0.012f; // 中等阈值，平衡检测精度
            break;
        case EVADMode::Aggressive:
            Threshold = 0.008f; // 较低阈值，更敏感，适合嘈杂环境
            break;
        case EVADMode::VeryAggressive:
            Threshold = 0.005f; // 很低阈值，非常敏感，可能有误检
            break;
    }
    
    bool bVoiceDetected = Energy > Threshold;
    
    // 添加调试日志来查看VAD工作情况
    if (bVoiceDetected)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("VoiceActivityManager: Voice detected - Energy: %f, Threshold: %f"), Energy, Threshold);
    }
    
    return bVoiceDetected;
}

float UVoiceActivityManager::CalculateAudioEnergy(const TArray<float>& AudioData)
{
    if (AudioData.Num() == 0)
    {
        return 0.0f;
    }
    
    float TotalEnergy = 0.0f;
    
    for (float Sample : AudioData)
    {
        TotalEnergy += Sample * Sample;
    }
    
    // 返回RMS能量
    return FMath::Sqrt(TotalEnergy / AudioData.Num());
}