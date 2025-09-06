#include "SpeechPerformanceMonitor.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"

USpeechPerformanceMonitor::USpeechPerformanceMonitor()
{
    Statistics.Reset();
    bIsMonitoring = false;
    LastAlertTime = 0.0;
}

void USpeechPerformanceMonitor::RecordRecognitionStart(const FString& SessionID)
{
    double CurrentTime = FPlatformTime::Seconds();
    FString ActualSessionID = SessionID.IsEmpty() ? FString::Printf(TEXT("Session_%f"), CurrentTime) : SessionID;
    
    RecognitionStartTimes.Add(ActualSessionID, CurrentTime);
    
    if (bIsMonitoring)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Performance Monitor: Recognition started - %s"), *ActualSessionID);
    }
}

void USpeechPerformanceMonitor::RecordRecognitionEnd(const FString& SessionID, bool bSuccess, const FString& Result)
{
    double CurrentTime = FPlatformTime::Seconds();
    double* StartTime = RecognitionStartTimes.Find(SessionID);
    
    Statistics.TotalRecognitions++;
    
    if (bSuccess)
    {
        Statistics.SuccessfulRecognitions++;
    }
    else
    {
        Statistics.FailedRecognitions++;
    }
    
    if (StartTime)
    {
        float RecognitionTime = static_cast<float>(CurrentTime - *StartTime);
        UpdateAverageRecognitionTime(RecognitionTime);
        RecognitionStartTimes.Remove(SessionID);
        
        if (RecognitionTime > LongRecognitionTimeThreshold)
        {
            SendAlert(FString::Printf(TEXT("Long recognition time detected: %.2f seconds"), RecognitionTime));
        }
    }
    
    if (bIsMonitoring)
    {
        UE_LOG(LogTemp, Log, TEXT("Performance Monitor: Recognition ended - %s, Success: %s, Result: %s"), 
               *SessionID, bSuccess ? TEXT("Yes") : TEXT("No"), *Result);
        
        CheckPerformanceAlerts();
    }
}

void USpeechPerformanceMonitor::RecordAudioOverflow()
{
    Statistics.AudioOverflowCount++;
    
    if (bIsMonitoring)
    {
        CheckPerformanceAlerts();
    }
}

void USpeechPerformanceMonitor::RecordNetworkError()
{
    Statistics.NetworkErrorCount++;
    
    if (bIsMonitoring)
    {
        SendAlert(FString::Printf(TEXT("Network error occurred. Total network errors: %d"), Statistics.NetworkErrorCount));
    }
}

void USpeechPerformanceMonitor::RecordLongSpeechSegment()
{
    Statistics.LongSpeechSegmentCount++;
}

void USpeechPerformanceMonitor::RecordAudioDuration(float Duration)
{
    Statistics.TotalAudioDuration += Duration;
}

void USpeechPerformanceMonitor::ResetStatistics()
{
    Statistics.Reset();
    RecognitionStartTimes.Empty();
    UE_LOG(LogTemp, Log, TEXT("Performance Monitor: Statistics reset"));
}

void USpeechPerformanceMonitor::StartMonitoring()
{
    if (bIsMonitoring)
    {
        return;
    }
    
    bIsMonitoring = true;
    
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(MonitoringTimer, [this]()
        {
            CheckPerformanceAlerts();
        }, 10.0f, true); // 每10秒检查一次性能
    }
    
    UE_LOG(LogTemp, Log, TEXT("Performance Monitor: Monitoring started"));
}

void USpeechPerformanceMonitor::StopMonitoring()
{
    if (!bIsMonitoring)
    {
        return;
    }
    
    bIsMonitoring = false;
    
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MonitoringTimer);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Performance Monitor: Monitoring stopped"));
}

FString USpeechPerformanceMonitor::GeneratePerformanceReport() const
{
    FString Report;
    
    Report += FString::Printf(TEXT("=== Speech System Performance Report ===\n"));
    Report += FString::Printf(TEXT("Generated at: %s\n\n"), *FDateTime::Now().ToString());
    
    // 基本统计
    Report += FString::Printf(TEXT("Total Recognitions: %d\n"), Statistics.TotalRecognitions);
    Report += FString::Printf(TEXT("Successful Recognitions: %d\n"), Statistics.SuccessfulRecognitions);
    Report += FString::Printf(TEXT("Failed Recognitions: %d\n"), Statistics.FailedRecognitions);
    Report += FString::Printf(TEXT("Success Rate: %.2f%%\n"), Statistics.GetSuccessRate() * 100.0f);
    Report += FString::Printf(TEXT("Average Recognition Time: %.2f seconds\n"), Statistics.AverageRecognitionTime);
    
    // 错误统计
    Report += FString::Printf(TEXT("\n=== Error Statistics ===\n"));
    Report += FString::Printf(TEXT("Audio Overflow Count: %d\n"), Statistics.AudioOverflowCount);
    Report += FString::Printf(TEXT("Network Error Count: %d\n"), Statistics.NetworkErrorCount);
    Report += FString::Printf(TEXT("Long Speech Segments: %d\n"), Statistics.LongSpeechSegmentCount);
    
    // 音频统计
    Report += FString::Printf(TEXT("\n=== Audio Statistics ===\n"));
    Report += FString::Printf(TEXT("Total Audio Duration: %.2f seconds\n"), Statistics.TotalAudioDuration);
    
    if (Statistics.TotalRecognitions > 0)
    {
        float OverflowRate = (float)Statistics.AudioOverflowCount / Statistics.TotalRecognitions;
        float ErrorRate = (float)Statistics.NetworkErrorCount / Statistics.TotalRecognitions;
        
        Report += FString::Printf(TEXT("Audio Overflow Rate: %.2f%%\n"), OverflowRate * 100.0f);
        Report += FString::Printf(TEXT("Network Error Rate: %.2f%%\n"), ErrorRate * 100.0f);
    }
    
    return Report;
}

void USpeechPerformanceMonitor::SavePerformanceReport(const FString& FilePath) const
{
    FString Report = GeneratePerformanceReport();
    
    if (FFileHelper::SaveStringToFile(Report, *FilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("Performance report saved to: %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save performance report to: %s"), *FilePath);
    }
}

void USpeechPerformanceMonitor::UpdateAverageRecognitionTime(float NewTime)
{
    if (Statistics.SuccessfulRecognitions <= 1)
    {
        Statistics.AverageRecognitionTime = NewTime;
    }
    else
    {
        // 计算移动平均值
        float Weight = 1.0f / Statistics.SuccessfulRecognitions;
        Statistics.AverageRecognitionTime = Statistics.AverageRecognitionTime * (1.0f - Weight) + NewTime * Weight;
    }
}

void USpeechPerformanceMonitor::CheckPerformanceAlerts()
{
    double CurrentTime = FPlatformTime::Seconds();
    
    // 限制警报频率
    if (CurrentTime - LastAlertTime < AlertCooldown)
    {
        return;
    }
    
    // 检查失败率
    if (Statistics.TotalRecognitions >= 5) // 至少5次识别后才检查
    {
        float FailureRate = 1.0f - Statistics.GetSuccessRate();
        if (FailureRate > HighFailureRateThreshold)
        {
            SendAlert(FString::Printf(TEXT("High failure rate detected: %.2f%%"), FailureRate * 100.0f));
        }
    }
    
    // 检查溢出率
    if (Statistics.TotalRecognitions >= 3)
    {
        float OverflowRate = (float)Statistics.AudioOverflowCount / Statistics.TotalRecognitions;
        if (OverflowRate > HighOverflowRateThreshold)
        {
            SendAlert(FString::Printf(TEXT("High audio overflow rate: %.2f%%"), OverflowRate * 100.0f));
        }
    }
}

void USpeechPerformanceMonitor::SendAlert(const FString& AlertMessage)
{
    LastAlertTime = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Warning, TEXT("Performance Alert: %s"), *AlertMessage);
    OnPerformanceAlert.Broadcast(AlertMessage);
}

// ============================================================================
// USpeechResourceMonitor 实现
// ============================================================================

USpeechResourceMonitor::USpeechResourceMonitor()
{
    ActiveAudioBuffers = 0;
    PooledAudioBuffers = 0;
    TotalNetworkBytesSent = 0.0f;
    LastCPUTime = 0.0;
    LastSystemTime = 0.0;
}

void USpeechResourceMonitor::StartResourceMonitoring()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(ResourceMonitorTimer, [this]()
        {
            PerformResourceCheck();
        }, 5.0f, true); // 每5秒检查一次资源使用
    }
    
    UE_LOG(LogTemp, Log, TEXT("Resource Monitor: Started"));
}

void USpeechResourceMonitor::StopResourceMonitoring()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ResourceMonitorTimer);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Resource Monitor: Stopped"));
}

float USpeechResourceMonitor::GetMemoryUsageMB() const
{
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    return MemStats.UsedPhysical / (1024.0f * 1024.0f);
}

float USpeechResourceMonitor::GetNetworkBytesPerSecond() const
{
    return CalculateNetworkRate();
}

float USpeechResourceMonitor::GetCPUUsagePercent() const
{
    // 简化的CPU使用率计算
    // 实际项目中可能需要更精确的实现
    return 0.0f; // 占位符
}

void USpeechResourceMonitor::RecordNetworkBytesSent(int32 Bytes)
{
    TotalNetworkBytesSent += Bytes;
    UpdateNetworkHistory(Bytes);
}

void USpeechResourceMonitor::PerformResourceCheck()
{
    CheckResourceAlerts();
    
    // 记录详细统计信息
    UE_LOG(LogTemp, VeryVerbose, TEXT("Resource Monitor: Memory=%.2fMB, ActiveBuffers=%d, PooledBuffers=%d, NetworkRate=%.2fKB/s"),
           GetMemoryUsageMB(), ActiveAudioBuffers, PooledAudioBuffers, GetNetworkBytesPerSecond() / 1024.0f);
}

void USpeechResourceMonitor::UpdateNetworkHistory(float Bytes)
{
    double CurrentTime = FPlatformTime::Seconds();
    
    NetworkBytesHistory.Add(Bytes);
    NetworkTimeHistory.Add(CurrentTime);
    
    // 限制历史记录大小
    if (NetworkBytesHistory.Num() > MaxHistorySize)
    {
        NetworkBytesHistory.RemoveAt(0);
        NetworkTimeHistory.RemoveAt(0);
    }
}

float USpeechResourceMonitor::CalculateNetworkRate() const
{
    if (NetworkBytesHistory.Num() < 2)
    {
        return 0.0f;
    }
    
    // 计算最近时间段的平均速率
    double TimeDelta = NetworkTimeHistory.Last() - NetworkTimeHistory[0];
    if (TimeDelta <= 0.0)
    {
        return 0.0f;
    }
    
    float TotalBytes = 0.0f;
    for (float Bytes : NetworkBytesHistory)
    {
        TotalBytes += Bytes;
    }
    
    return TotalBytes / TimeDelta;
}

void USpeechResourceMonitor::CheckResourceAlerts()
{
    // 检查内存使用
    float MemoryUsage = GetMemoryUsageMB();
    if (MemoryUsage > MemoryAlertThresholdMB)
    {
        OnResourceAlert.Broadcast(TEXT("Memory"), MemoryUsage);
    }
    
    // 检查网络使用
    float NetworkRate = GetNetworkBytesPerSecond();
    if (NetworkRate > NetworkAlertThresholdBps)
    {
        OnResourceAlert.Broadcast(TEXT("Network"), NetworkRate);
    }
    
    // 检查缓冲区数量
    if (ActiveAudioBuffers > 100) // 假设阈值
    {
        OnResourceAlert.Broadcast(TEXT("AudioBuffers"), ActiveAudioBuffers);
    }
}