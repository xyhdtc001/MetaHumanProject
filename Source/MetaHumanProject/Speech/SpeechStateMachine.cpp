#include "SpeechStateMachine.h"
#include "Engine/World.h"
#include "TimerManager.h"

USpeechStateMachine::USpeechStateMachine()
{
    CurrentState = ESpeechSystemState::Uninitialized;
    LastErrorType = ESpeechErrorType::None;
    StateHistory.Reserve(20);
    AddToHistory(CurrentState);
}

bool USpeechStateMachine::TransitionTo(ESpeechSystemState NewState, const FString& Reason)
{
    if (!IsValidTransition(CurrentState, NewState))
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid state transition from %s to %s"), 
               *GetCurrentStateString(), *UEnum::GetValueAsString(NewState));
        return false;
    }

    ESpeechSystemState OldState = CurrentState;
    CurrentState = NewState;
    AddToHistory(NewState);

    // 如果转换到Ready状态，清除之前的错误
    if (NewState == ESpeechSystemState::Ready)
    {
        ClearError();
    }

    UE_LOG(LogTemp, Log, TEXT("State transition: %s -> %s (%s)"), 
           *UEnum::GetValueAsString(OldState), *UEnum::GetValueAsString(NewState), *Reason);

    OnStateChanged.Broadcast(OldState, NewState, Reason);
    return true;
}

FString USpeechStateMachine::GetCurrentStateString() const
{
    return UEnum::GetValueAsString(CurrentState);
}

bool USpeechStateMachine::CanTransitionTo(ESpeechSystemState NewState) const
{
    return IsValidTransition(CurrentState, NewState);
}

void USpeechStateMachine::SetError(ESpeechErrorType ErrorType, const FString& ErrorMessage)
{
    LastErrorType = ErrorType;
    LastErrorMessage = ErrorMessage;

    UE_LOG(LogTemp, Error, TEXT("Speech System Error: %s - %s"), 
           *UEnum::GetValueAsString(ErrorType), *ErrorMessage);

    if (CurrentState != ESpeechSystemState::Error)
    {
        TransitionTo(ESpeechSystemState::Error, FString::Printf(TEXT("Error: %s"), *ErrorMessage));
    }

    OnErrorOccurred.Broadcast(ErrorType, ErrorMessage);
}

void USpeechStateMachine::ClearError()
{
    LastErrorType = ESpeechErrorType::None;
    LastErrorMessage.Empty();
}

bool USpeechStateMachine::IsValidTransition(ESpeechSystemState FromState, ESpeechSystemState ToState) const
{
    // 定义合法的状态转换
    switch (FromState)
    {
        case ESpeechSystemState::Uninitialized:
            return ToState == ESpeechSystemState::Initializing || ToState == ESpeechSystemState::Error;

        case ESpeechSystemState::Initializing:
            return ToState == ESpeechSystemState::Ready || ToState == ESpeechSystemState::Error;

        case ESpeechSystemState::Ready:
            return ToState == ESpeechSystemState::Listening || 
                   ToState == ESpeechSystemState::Error ||
                   ToState == ESpeechSystemState::Reconnecting;

        case ESpeechSystemState::Listening:
            return ToState == ESpeechSystemState::Processing || 
                   ToState == ESpeechSystemState::Ready ||
                   ToState == ESpeechSystemState::Error ||
                   ToState == ESpeechSystemState::Reconnecting;

        case ESpeechSystemState::Processing:
            return ToState == ESpeechSystemState::Ready ||
                   ToState == ESpeechSystemState::Listening ||
                   ToState == ESpeechSystemState::Error ||
                   ToState == ESpeechSystemState::Reconnecting;

        case ESpeechSystemState::Reconnecting:
            return ToState == ESpeechSystemState::Ready ||
                   ToState == ESpeechSystemState::Error ||
                   ToState == ESpeechSystemState::Initializing;

        case ESpeechSystemState::Error:
            // 错误状态可以转换到任何状态（用于恢复）
            return true;

        default:
            return false;
    }
}

void USpeechStateMachine::AddToHistory(ESpeechSystemState State)
{
    StateHistory.Add(State);
    
    // 限制历史记录大小
    if (StateHistory.Num() > 20)
    {
        StateHistory.RemoveAt(0);
    }
}

// ============================================================================
// USpeechErrorRecovery 实现
// ============================================================================

USpeechErrorRecovery::USpeechErrorRecovery()
{
    bIsReconnecting = false;
    ReconnectAttempts = 0;
}

bool USpeechErrorRecovery::AttemptRecovery(ESpeechErrorType ErrorType, const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Log, TEXT("Attempting recovery for error: %s"), *UEnum::GetValueAsString(ErrorType));

    switch (ErrorType)
    {
        case ESpeechErrorType::NetworkError:
            return HandleNetworkError();

        case ESpeechErrorType::AudioCaptureError:
            return HandleAudioError();

        case ESpeechErrorType::SDKError:
            return HandleSDKError();

        case ESpeechErrorType::RecognitionTimeout:
            // 超时错误通常可以通过重启会话解决
            StartReconnection();
            return true;

        case ESpeechErrorType::InitializationFailed:
            // 初始化失败需要重新初始化
            UE_LOG(LogTemp, Warning, TEXT("Initialization failed, attempting re-initialization"));
            return false; // 让外部系统处理重新初始化

        default:
            return false;
    }
}

void USpeechErrorRecovery::StartReconnection()
{
    if (bIsReconnecting)
    {
        UE_LOG(LogTemp, Warning, TEXT("Reconnection already in progress"));
        return;
    }

    bIsReconnecting = true;
    ReconnectAttempts = 0;

    UE_LOG(LogTemp, Log, TEXT("Starting reconnection process"));
    PerformReconnectAttempt();
}

void USpeechErrorRecovery::StopReconnection()
{
    if (!bIsReconnecting)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReconnectTimer);
    }

    ResetReconnectState();
    UE_LOG(LogTemp, Log, TEXT("Reconnection process stopped"));
}

bool USpeechErrorRecovery::HandleNetworkError()
{
    UE_LOG(LogTemp, Log, TEXT("Handling network error"));
    StartReconnection();
    return true;
}

bool USpeechErrorRecovery::HandleAudioError()
{
    UE_LOG(LogTemp, Log, TEXT("Handling audio error"));
    // 音频错误通常需要重新初始化音频系统
    return false; // 让外部系统处理
}

bool USpeechErrorRecovery::HandleSDKError()
{
    UE_LOG(LogTemp, Log, TEXT("Handling SDK error"));
    StartReconnection();
    return true;
}

void USpeechErrorRecovery::PerformReconnectAttempt()
{
    if (!bIsReconnecting)
    {
        return;
    }

    ReconnectAttempts++;
    OnRecoveryAttempt.Broadcast(ReconnectAttempts);

    UE_LOG(LogTemp, Log, TEXT("Reconnection attempt %d/%d"), ReconnectAttempts, SpeechConfig.MaxReconnectAttempts);

    if (ReconnectAttempts >= SpeechConfig.MaxReconnectAttempts)
    {
        // 达到最大重试次数
        FString FailureReason = FString::Printf(TEXT("Max reconnection attempts reached (%d)"), SpeechConfig.MaxReconnectAttempts);
        UE_LOG(LogTemp, Error, TEXT("Reconnection failed: %s"), *FailureReason);
        
        OnRecoveryFailed.Broadcast(FailureReason);
        ResetReconnectState();
        return;
    }

    // 这里应该调用外部的重连逻辑
    // 实际的重连成功/失败应该由外部系统通过调用相应的函数来通知

    // 设置下次重试的定时器
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(ReconnectTimer, [this]()
        {
            PerformReconnectAttempt();
        }, SpeechConfig.ReconnectDelay, false);
    }
}

void USpeechErrorRecovery::ResetReconnectState()
{
    bIsReconnecting = false;
    ReconnectAttempts = 0;
}