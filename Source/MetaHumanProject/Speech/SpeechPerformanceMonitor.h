#pragma once

#include "CoreMinimal.h"
#include "SpeechConfig.h"
#include "Engine/World.h"
#include "SpeechPerformanceMonitor.generated.h"

/**
 * 性能指标收集器
 */
UCLASS(BlueprintType)
class METAHUMANCPP_API USpeechPerformanceMonitor : public UObject
{
    GENERATED_BODY()

public:
    USpeechPerformanceMonitor();

    // 性能记录函数
    UFUNCTION(BlueprintCallable, Category = "Speech Performance")
    void RecordRecognitionStart(const FString& SessionID = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Speech Performance") 
    void RecordRecognitionEnd(const FString& SessionID, bool bSuccess, const FString& Result = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Speech Performance")
    void RecordAudioOverflow();

    UFUNCTION(BlueprintCallable, Category = "Speech Performance")
    void RecordNetworkError();

    UFUNCTION(BlueprintCallable, Category = "Speech Performance")
    void RecordLongSpeechSegment();

    UFUNCTION(BlueprintCallable, Category = "Speech Performance")
    void RecordAudioDuration(float Duration);

    // 统计信息获取
    UFUNCTION(BlueprintPure, Category = "Speech Performance")
    FSpeechStatistics GetCurrentStatistics() const { return Statistics; }

    UFUNCTION(BlueprintCallable, Category = "Speech Performance")
    void ResetStatistics();

    // 实时监控
    UFUNCTION(BlueprintCallable, Category = "Speech Performance")
    void StartMonitoring();

    UFUNCTION(BlueprintCallable, Category = "Speech Performance")
    void StopMonitoring();

    UFUNCTION(BlueprintPure, Category = "Speech Performance")
    bool IsMonitoring() const { return bIsMonitoring; }

    // 性能报告
    UFUNCTION(BlueprintCallable, Category = "Speech Performance")
    FString GeneratePerformanceReport() const;

    UFUNCTION(BlueprintCallable, Category = "Speech Performance")
    void SavePerformanceReport(const FString& FilePath) const;

    // 事件委托
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerformanceAlert, const FString&, AlertMessage);

    UPROPERTY(BlueprintAssignable, Category = "Speech Performance")
    FOnPerformanceAlert OnPerformanceAlert;

private:
    UPROPERTY()
    FSpeechStatistics Statistics;

    UPROPERTY()
    TMap<FString, double> RecognitionStartTimes;

    UPROPERTY()
    bool bIsMonitoring = false;

    FTimerHandle MonitoringTimer;
    double LastAlertTime = 0.0;
    const double AlertCooldown = 5.0; // 5秒内不重复发送相同类型的警报

    // 性能阈值
    const float HighFailureRateThreshold = 0.3f; // 30%失败率阈值
    const float HighOverflowRateThreshold = 0.1f; // 10%溢出率阈值
    const float LongRecognitionTimeThreshold = 10.0f; // 10秒识别时间阈值

    void UpdateAverageRecognitionTime(float NewTime);
    void CheckPerformanceAlerts();
    void SendAlert(const FString& AlertMessage);
};

/**
 * 资源使用监控器
 */
UCLASS(BlueprintType)
class METAHUMANCPP_API USpeechResourceMonitor : public UObject
{
    GENERATED_BODY()

public:
    USpeechResourceMonitor();

    // 资源监控
    UFUNCTION(BlueprintCallable, Category = "Speech Resource Monitor")
    void StartResourceMonitoring();

    UFUNCTION(BlueprintCallable, Category = "Speech Resource Monitor")
    void StopResourceMonitoring();

    // 内存使用情况
    UFUNCTION(BlueprintPure, Category = "Speech Resource Monitor")
    float GetMemoryUsageMB() const;

    UFUNCTION(BlueprintPure, Category = "Speech Resource Monitor")
    int32 GetActiveAudioBufferCount() const { return ActiveAudioBuffers; }

    UFUNCTION(BlueprintPure, Category = "Speech Resource Monitor")
    int32 GetPooledAudioBufferCount() const { return PooledAudioBuffers; }

    // 网络使用情况
    UFUNCTION(BlueprintPure, Category = "Speech Resource Monitor")
    float GetNetworkBytesPerSecond() const;

    UFUNCTION(BlueprintPure, Category = "Speech Resource Monitor")
    float GetTotalNetworkBytesSent() const { return TotalNetworkBytesSent; }

    // CPU使用情况（近似）
    UFUNCTION(BlueprintPure, Category = "Speech Resource Monitor")
    float GetCPUUsagePercent() const;

    // 资源记录
    void RecordAudioBufferAllocation() { ActiveAudioBuffers++; }
    void RecordAudioBufferDeallocation() { ActiveAudioBuffers = FMath::Max(0, ActiveAudioBuffers - 1); }
    void RecordPooledBufferChange(int32 Count) { PooledAudioBuffers = Count; }
    void RecordNetworkBytesSent(int32 Bytes);

    // 警报
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnResourceAlert, const FString&, ResourceType, float, Usage);

    UPROPERTY(BlueprintAssignable, Category = "Speech Resource Monitor")
    FOnResourceAlert OnResourceAlert;

private:
    FTimerHandle ResourceMonitorTimer;
    
    // 资源统计
    int32 ActiveAudioBuffers = 0;
    int32 PooledAudioBuffers = 0;
    float TotalNetworkBytesSent = 0.0f;
    
    // 网络速率计算
    TArray<float> NetworkBytesHistory;
    TArray<double> NetworkTimeHistory;
    const int32 MaxHistorySize = 60; // 保留60个数据点（约1分钟历史）
    
    // CPU使用率计算
    double LastCPUTime = 0.0;
    double LastSystemTime = 0.0;
    
    // 警报阈值
    const float MemoryAlertThresholdMB = 100.0f; // 100MB内存警报
    const float NetworkAlertThresholdBps = 1024 * 1024; // 1MB/s网络警报
    const float CPUAlertThresholdPercent = 50.0f; // 50% CPU警报
    
    void PerformResourceCheck();
    void UpdateNetworkHistory(float Bytes);
    float CalculateNetworkRate() const;
    void CheckResourceAlerts();
};