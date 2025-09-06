#pragma once

#include "CoreMinimal.h"
#include "Async/AsyncWork.h"

/**
 * 语音系统异步任务基类
 */
class METAHUMANCPP_API FSpeechTask : public FNonAbandonableTask
{
public:
    FSpeechTask() = default;
    virtual ~FSpeechTask() = default;

    // FNonAbandonableTask interface
    void DoWork() 
    {
        ExecuteTask();
    }

    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FSpeechTask, STATGROUP_ThreadPoolAsyncTasks);
    }

    // 任务状态
    bool IsCompleted() const { return bCompleted; }
    bool HasError() const { return bHasError; }
    FString GetErrorMessage() const { return ErrorMessage; }

protected:
    virtual void ExecuteTask() = 0;

    void SetCompleted() { bCompleted = true; }
    void SetError(const FString& Error) 
    { 
        bHasError = true; 
        ErrorMessage = Error;
        bCompleted = true;
    }

private:
    bool bCompleted = false;
    bool bHasError = false;
    FString ErrorMessage;
};

/**
 * 音频处理异步任务
 */
class METAHUMANCPP_API FAudioProcessingTask : public FSpeechTask
{
public:
    FAudioProcessingTask(const TArray<float>& InAudioData, TFunction<void(const TArray<uint8>&)> InCallback)
        : AudioData(InAudioData), Callback(InCallback) {}

protected:
    virtual void ExecuteTask() override
    {
        try
        {
            // 将float音频数据转换为int16格式
            TArray<uint8> ConvertedData;
            ConvertedData.Reserve(AudioData.Num() * sizeof(int16));

            for (float Sample : AudioData)
            {
                float ClampedSample = FMath::Clamp(Sample, -1.0f, 1.0f);
                int16 IntSample = static_cast<int16>(ClampedSample * 32767.0f);
                ConvertedData.Append(reinterpret_cast<uint8*>(&IntSample), sizeof(int16));
            }

            // 在游戏线程中执行回调
            AsyncTask(ENamedThreads::GameThread, [this, ConvertedData = MoveTemp(ConvertedData)]()
            {
                if (Callback)
                {
                    Callback(ConvertedData);
                }
            });

            SetCompleted();
        }
        catch (...)
        {
            SetError(TEXT("Audio processing failed"));
        }
    }

private:
    TArray<float> AudioData;
    TFunction<void(const TArray<uint8>&)> Callback;
};

/**
 * 语音识别异步任务
 */
class METAHUMANCPP_API FSpeechRecognitionTask : public FSpeechTask
{
public:
    FSpeechRecognitionTask(const TArray<uint8>& InAudioData, const FString& InSessionID, TFunction<void(bool)> InCallback)
        : AudioData(InAudioData), SessionID(InSessionID), Callback(InCallback) {}

protected:
    virtual void ExecuteTask() override
    {
        // 这里可以添加额外的音频预处理
        // 比如降噪、回声消除等
        
        AsyncTask(ENamedThreads::GameThread, [this]()
        {
            if (Callback)
            {
                Callback(true); // 简化处理，实际应该调用SDK
            }
        });

        SetCompleted();
    }

private:
    TArray<uint8> AudioData;
    FString SessionID;
    TFunction<void(bool)> Callback;
};

/**
 * 异步任务管理器
 */
class METAHUMANCPP_API FSpeechAsyncManager
{
public:
    static FSpeechAsyncManager& Get()
    {
        static FSpeechAsyncManager Instance;
        return Instance;
    }

    // 提交音频处理任务
    void SubmitAudioProcessingTask(const TArray<float>& AudioData, TFunction<void(const TArray<uint8>&)> Callback)
    {
        auto Task = MakeShared<FAudioProcessingTask>(AudioData, Callback);
        auto AsyncTask = MakeShared<FAsyncTask<FAudioProcessingTask>>(*Task);
        
        ActiveTasks.Add(AsyncTask);
        AsyncTask->StartBackgroundTask();
        
        // 清理完成的任务
        CleanupCompletedTasks();
    }

    // 提交语音识别任务
    void SubmitRecognitionTask(const TArray<uint8>& AudioData, const FString& SessionID, TFunction<void(bool)> Callback)
    {
        auto Task = MakeShared<FSpeechRecognitionTask>(AudioData, SessionID, Callback);
        auto AsyncTask = MakeShared<FAsyncTask<FSpeechRecognitionTask>>(*Task);
        
        RecognitionTasks.Add(AsyncTask);
        AsyncTask->StartBackgroundTask();
        
        // 清理完成的任务
        CleanupRecognitionTasks();
    }

    // 获取活跃任务数量
    int32 GetActiveTaskCount() const { return ActiveTasks.Num() + RecognitionTasks.Num(); }

    // 清理所有任务
    void Shutdown()
    {
        // 等待所有任务完成
        for (auto& Task : ActiveTasks)
        {
            if (Task && Task->GetTask().IsCompleted())
            {
                Task->EnsureCompletion();
            }
        }
        
        for (auto& Task : RecognitionTasks)
        {
            if (Task && Task->GetTask().IsCompleted())
            {
                Task->EnsureCompletion();
            }
        }
        
        ActiveTasks.Empty();
        RecognitionTasks.Empty();
    }

private:
    TArray<TSharedPtr<FAsyncTask<FAudioProcessingTask>>> ActiveTasks;
    TArray<TSharedPtr<FAsyncTask<FSpeechRecognitionTask>>> RecognitionTasks;

    void CleanupCompletedTasks()
    {
        ActiveTasks.RemoveAll([](const TSharedPtr<FAsyncTask<FAudioProcessingTask>>& Task)
        {
            return Task && Task->IsDone();
        });
    }

    void CleanupRecognitionTasks()
    {
        RecognitionTasks.RemoveAll([](const TSharedPtr<FAsyncTask<FSpeechRecognitionTask>>& Task)
        {
            return Task && Task->IsDone();
        });
    }
};