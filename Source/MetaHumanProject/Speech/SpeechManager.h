#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"

THIRD_PARTY_INCLUDES_START
#include "msp_cmn.h"
#include "msp_errors.h"
#include "qisr.h"
#include "qtts.h"
THIRD_PARTY_INCLUDES_END

#include "SpeechManager.generated.h"

#pragma pack(push, 1) 
// WAV文件头结构（参考iFlytek SDK示例）
struct FWavePCMHeader
{
    char riff[4] = {'R', 'I', 'F', 'F'};                   // = "RIFF"
    int32 size_8 = 0;                   // = FileSize - 8
    char wave[4] = {'W', 'A', 'V', 'E'};                   // = "WAVE"
    char fmt[4]={'f', 'm', 't', ' '};                    // = "fmt "
    int32 fmt_size = 16;                 // = 下一个结构体的大小 : 16

    int16 format_tag = 1;               // = PCM : 1
    int16 channels = 1;                 // = 通道数 : 1
    int32 samples_per_sec = 16000;          // = 采样率 : 8000 | 6000 | 11025 | 16000
    int32 avg_bytes_per_sec =  32000;        // = 每秒字节数 : samples_per_sec * bits_per_sample / 8
    int16 block_align = 2;              // = 每采样点字节数 : wBitsPerSample / 8
    int16 bits_per_sample = 16;          // = 量化精度: 8 | 16

    char data[4] = {'d', 'a', 't', 'a'};                   // = "data";
    int32 data_size = 0;                 // = 纯数据长度 : FileSize - 44 
};
static_assert(sizeof(FWavePCMHeader) == 44, "WAV header must be 44 bytes");
#pragma pack(pop)


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeechRecognized, const FString&, RecognizedText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeechSynthesized, const TArray<uint8>&, SynthesizedAudio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeechError, const FString&, ErrorMessage);

/**
 * 语音管理器 - 统一管理语音识别和语音合成
 */
UCLASS(BlueprintType, Blueprintable)
class METAHUMANCPP_API USpeechManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    USpeechManager();

    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 初始化语音SDK
    UFUNCTION(BlueprintCallable, Category = "Speech")
    bool InitializeSpeech(const FString& AppID = TEXT(""), const FString& APIKey = TEXT(""));

    // 语音识别相关
    UFUNCTION(BlueprintCallable, Category = "Speech|Recognition")
    bool StartSpeechRecognition(const FString& Language = TEXT("zh_cn"));

    UFUNCTION(BlueprintCallable, Category = "Speech|Recognition")
    bool StopSpeechRecognition();

    UFUNCTION(BlueprintCallable, Category = "Speech|Recognition")
    bool WriteSpeechData(const TArray<uint8>& AudioData);

    // 获取语音识别状态
    UFUNCTION(BlueprintPure, Category = "Speech|Recognition")
    bool IsRecognitionActive() const { return bIsRecognitionActive; }

    // 语音合成相关
    UFUNCTION(BlueprintCallable, Category = "Speech|Synthesis")
    bool SynthesizeText(const FString& Text, const FString& Voice = TEXT("xiaoyan"));

    // 事件委托
    UPROPERTY(BlueprintAssignable, Category = "Speech|Events")
    FOnSpeechRecognized OnSpeechRecognized;

    UPROPERTY(BlueprintAssignable, Category = "Speech|Events")
    FOnSpeechSynthesized OnSpeechSynthesized;

    UPROPERTY(BlueprintAssignable, Category = "Speech|Events")
    FOnSpeechError OnSpeechError;

protected:
    // SDK状态
    bool bIsSDKInitialized;
    bool bIsRecognitionActive;
    bool bIsSynthesisActive;

    // 当前会话ID
    FString CurrentRecognitionSessionID;
    FString CurrentSynthesisSessionID;

    // SDK配置
    FString SDKAppID;
    FString SDKAPIKey;

    // 回调函数
    static void OnRecognitionResult(const char* sessionID, const char* result, int resultLen, int resultStatus, void* userData);
    static void OnRecognitionStatus(const char* sessionID, int type, int status, int param1, const void* param2, void* userData);
    static void OnRecognitionError(const char* sessionID, int errorCode, const char* detail, void* userData);

    static void OnSynthesisResult(const char* sessionID, const char* audio, int audioLen, int synthStatus, int ced, const char* audioInfo, int audioInfoLen, void* userData);
    static void OnSynthesisStatus(const char* sessionID, int type, int status, int param1, const void* param2, void* userData);
    static void OnSynthesisError(const char* sessionID, int errorCode, const char* detail, void* userData);

    // 辅助函数
    FString GetDefaultAppID() const;
    FString GetDefaultAPIKey() const;
    void LogSpeechError(int ErrorCode, const FString& Context);

private:
    // 线程安全
    FCriticalSection RecognitionCriticalSection;
    FCriticalSection SynthesisCriticalSection;

    // 音频缓冲区
    TArray<uint8> SynthesizedAudioBuffer;
};