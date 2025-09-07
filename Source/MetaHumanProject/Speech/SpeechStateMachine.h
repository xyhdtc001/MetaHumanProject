#pragma once

#include "CoreMinimal.h"
#include "SpeechConfig.h"
#include "SpeechStateMachine.generated.h"

/**
 * 语音系统状态枚举
 */
UENUM(BlueprintType)
enum class ESpeechSystemState : uint8
{
    Uninitialized   UMETA(DisplayName = "未初始化"),
    Initializing    UMETA(DisplayName = "初始化中"),
    Ready          UMETA(DisplayName = "就绪"),
    Listening      UMETA(DisplayName = "监听中"),
    Processing     UMETA(DisplayName = "处理中"),
    Reconnecting   UMETA(DisplayName = "重连中"),
    Error          UMETA(DisplayName = "错误")
};

/**
 * 语音错误类型枚举
 */
UENUM(BlueprintType)
enum class ESpeechErrorType : uint8
{
    None               UMETA(DisplayName = "无错误"),
    InitializationFailed UMETA(DisplayName = "初始化失败"),
    NetworkError       UMETA(DisplayName = "网络错误"),
    AudioCaptureError  UMETA(DisplayName = "音频捕获错误"),
    RecognitionTimeout UMETA(DisplayName = "识别超时"),
    SDKError          UMETA(DisplayName = "SDK错误"),
    ConfigurationError UMETA(DisplayName = "配置错误")
};

/**
 * 语音系统状态机
 */
UCLASS(BlueprintType)
class METAHUMANPROJECT_API USpeechStateMachine : public UObject
{
    GENERATED_BODY()

public:
    USpeechStateMachine();

    // 状态转换函数
    UFUNCTION(BlueprintCallable, Category = "Speech State Machine")
    bool TransitionTo(ESpeechSystemState NewState, const FString& Reason = TEXT(""));

    // 状态查询
    UFUNCTION(BlueprintPure, Category = "Speech State Machine")
    ESpeechSystemState GetCurrentState() const { return CurrentState; }

    UFUNCTION(BlueprintPure, Category = "Speech State Machine")
    FString GetCurrentStateString() const;

    UFUNCTION(BlueprintPure, Category = "Speech State Machine")
    bool IsInState(ESpeechSystemState State) const { return CurrentState == State; }

    UFUNCTION(BlueprintPure, Category = "Speech State Machine")
    bool CanTransitionTo(ESpeechSystemState NewState) const;

    // 错误处理
    UFUNCTION(BlueprintCallable, Category = "Speech State Machine")
    void SetError(ESpeechErrorType ErrorType, const FString& ErrorMessage);

    UFUNCTION(BlueprintPure, Category = "Speech State Machine")
    ESpeechErrorType GetLastErrorType() const { return LastErrorType; }

    UFUNCTION(BlueprintPure, Category = "Speech State Machine")
    FString GetLastErrorMessage() const { return LastErrorMessage; }

    UFUNCTION(BlueprintCallable, Category = "Speech State Machine")
    void ClearError();

    // 状态历史
    UFUNCTION(BlueprintPure, Category = "Speech State Machine")
    TArray<ESpeechSystemState> GetStateHistory() const { return StateHistory; }

    // 事件委托
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStateChanged, ESpeechSystemState, OldState, ESpeechSystemState, NewState, const FString&, Reason);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnErrorOccurred, ESpeechErrorType, ErrorType, const FString&, ErrorMessage);

    UPROPERTY(BlueprintAssignable, Category = "Speech State Machine")
    FOnStateChanged OnStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Speech State Machine")
    FOnErrorOccurred OnErrorOccurred;

private:
    UPROPERTY()
    ESpeechSystemState CurrentState;

    UPROPERTY()
    ESpeechErrorType LastErrorType;

    UPROPERTY()
    FString LastErrorMessage;

    // 状态历史（最多保存20个状态）
    UPROPERTY()
    TArray<ESpeechSystemState> StateHistory;

    // 状态转换规则
    bool IsValidTransition(ESpeechSystemState FromState, ESpeechSystemState ToState) const;
    void AddToHistory(ESpeechSystemState State);
};

/**
 * 错误恢复管理器
 */
UCLASS(BlueprintType)
class METAHUMANPROJECT_API USpeechErrorRecovery : public UObject
{
    GENERATED_BODY()

public:
    USpeechErrorRecovery();

    // 设置配置
    void SetConfig(const FSpeechSystemConfig& Config) { SpeechConfig = Config; }

    // 错误恢复策略
    UFUNCTION(BlueprintCallable, Category = "Speech Error Recovery")
    bool AttemptRecovery(ESpeechErrorType ErrorType, const FString& ErrorMessage);

    // 重连管理
    UFUNCTION(BlueprintCallable, Category = "Speech Error Recovery")
    void StartReconnection();

    UFUNCTION(BlueprintCallable, Category = "Speech Error Recovery")
    void StopReconnection();

    UFUNCTION(BlueprintPure, Category = "Speech Error Recovery")
    bool IsReconnecting() const { return bIsReconnecting; }

    UFUNCTION(BlueprintPure, Category = "Speech Error Recovery")
    int32 GetReconnectAttempts() const { return ReconnectAttempts; }

    // 事件委托
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRecoveryAttempt, int32, AttemptNumber);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRecoverySuccess);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRecoveryFailed, const FString&, Reason);

    UPROPERTY(BlueprintAssignable, Category = "Speech Error Recovery")
    FOnRecoveryAttempt OnRecoveryAttempt;

    UPROPERTY(BlueprintAssignable, Category = "Speech Error Recovery")
    FOnRecoverySuccess OnRecoverySuccess;

    UPROPERTY(BlueprintAssignable, Category = "Speech Error Recovery")
    FOnRecoveryFailed OnRecoveryFailed;

private:
    FSpeechSystemConfig SpeechConfig;
    
    bool bIsReconnecting = false;
    int32 ReconnectAttempts = 0;
    FTimerHandle ReconnectTimer;

    // 网络错误恢复
    bool HandleNetworkError();
    
    // 音频错误恢复
    bool HandleAudioError();
    
    // SDK错误恢复
    bool HandleSDKError();
    
    // 执行重连尝试
    void PerformReconnectAttempt();
    
    // 重置重连状态
    void ResetReconnectState();
};