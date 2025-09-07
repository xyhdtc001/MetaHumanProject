#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpeechManager.h"
#include "VoiceActivityManager.h"
#include "DifyAPIClient.h"
#include "Sound/SoundWave.h"

// UE音频录制支持
namespace Audio { class FAudioCapture; }

#include "VoiceInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceRecognitionResult, const FString&, RecognizedText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceSynthesisComplete, USoundWave*, GeneratedAudio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceActivityChanged, bool, bVoiceDetected);

/**
 * 语音交互组件 - 为Actor提供语音交互能力
 * 可以添加到MetaHuman角色上实现语音对话功能
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class METAHUMANPROJECT_API UVoiceInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVoiceInteractionComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // 语音识别
    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|Recognition")
    bool StartListening(const FString& Language = TEXT("zh_cn"));

    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|Recognition")
    bool StopListening();

    UFUNCTION(BlueprintPure, Category = "Voice Interaction|Recognition")
    bool IsListening() const { return bIsListening; }

    // 语音合成
    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|Synthesis")
    bool SpeakText(const FString& Text, const FString& VoiceName = TEXT("aisjiuxu"));

    UFUNCTION(BlueprintPure, Category = "Voice Interaction|Synthesis")
    bool IsSpeaking() const { return bIsSpeaking; }

    // Dify API相关
    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|Dify")
    void InitializeDifyAPI(const FString& BaseUrl, const FString& ApiKey);

    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|Dify")
    void SendToDifyAPI(const FString& Query, const FString& ConversationId = "");

    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|Dify")
    void GenerateResponseWithDify(const FString& RecognizedText);

    UFUNCTION(BlueprintPure, Category = "Voice Interaction|Dify")
    FString GetCurrentConversationId() const;

    // 配置
    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|Settings")
    void SetSpeechSettings(const FString& AppID, const FString& Language = TEXT("zh_cn"), const FString& Voice = TEXT("xiaoyan"));

    // 音频录制控制（用于语音识别）
    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|Audio")
    bool StartAudioCapture();

    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|Audio")
    bool StopAudioCapture();

    // VAD控制
    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|VAD")
    bool InitializeVAD(EVADMode Mode = EVADMode::Aggressive, int32 SampleRate = 16000);

    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|VAD")
    bool SetVADMode(EVADMode Mode);

    UFUNCTION(BlueprintPure, Category = "Voice Interaction|VAD")
    bool IsVADInitialized() const;

    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|VAD")
    bool ResetVAD();

    UFUNCTION(BlueprintCallable, Category = "Voice Interaction|VAD")
    void SetVADEnabled(bool bEnabled) { bVADEnabled = bEnabled; }

    // 事件
    UPROPERTY(BlueprintAssignable, Category = "Voice Interaction|Events")
    FOnVoiceRecognitionResult OnRecognitionResult;

    UPROPERTY(BlueprintAssignable, Category = "Voice Interaction|Events")
    FOnVoiceSynthesisComplete OnSynthesisComplete;

    UPROPERTY(BlueprintAssignable, Category = "Voice Interaction|Events")
    FOnVoiceError OnVoiceError;

    UPROPERTY(BlueprintAssignable, Category = "Voice Interaction|Events")
    FOnVoiceActivityChanged OnVoiceActivityChanged;
    
    // 使用DifyAPIClient.h中定义的委托类型
    UPROPERTY(BlueprintAssignable, Category = "Voice Interaction|Events")
    FOnDifyResponseReceived OnDifyResponseReceived;
    
    // 添加DifyAPIClient中定义的错误委托
    UPROPERTY(BlueprintAssignable, Category = "Voice Interaction|Events")
    FOnDifyErrorReceived OnDifyErrorReceived;

    // 配置参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|Settings")
    FString DefaultLanguage = TEXT("zh_cn");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|Settings")
    FString DefaultVoice = TEXT("xiaoyan");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|Settings")
    bool bAutoStartListening = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|Settings")
    bool bContinuousRecognitionMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|Settings")
    float VoiceDetectionThreshold = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|VAD Settings")
    bool bVADEnabled = true;  

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|VAD Settings")
    EVADMode VADMode = EVADMode::Aggressive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|VAD Settings")
    bool bVADSmoothingEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|VAD Settings")
    int32 VADVoiceStartThreshold = 5;  // 增加阈值，减少误触发

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|VAD Settings")
    int32 VADVoiceEndThreshold = 15;   // 增加阈值，避免过早结束
    
    // Dify API设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|Dify Settings")
    bool bUseDifyForResponses = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|Dify Settings")
    FString DifyBaseUrl = TEXT("http://localhost/v1");
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Interaction|Dify Settings")
    FString DifyApiKey = TEXT("app-xEr4BATLe3Q6sez16Zosqpey");

protected:
    // Speech Manager引用
    UPROPERTY()
    TObjectPtr<USpeechManager> SpeechManager;

    // VAD Manager引用
    UPROPERTY()
    TObjectPtr<UVoiceActivityManager> VADManager;
    
    // Dify API Client
    UPROPERTY()
    TObjectPtr<UDifyAPIClient> DifyAPIClient;

    // 状态跟踪
    bool bIsListening;
    bool bIsSpeaking;
    bool bIsAudioCapturing;
    bool bCurrentVoiceActivity;  // 当前语音活动状态

    // 语音缓冲区 - 用于在VAD检测期间缓冲完整的语音片段
    bool bIsBufferingVoice;
    TArray<TArray<float>> VoiceBuffer;

    // 预缓冲区 - 持续缓冲最近的音频数据，防止语音开始部分丢失
    TArray<TArray<float>> PreBuffer;
    int32 PreBufferMaxChunks; // 预缓冲区最大chunk数量
    int32 PreBufferCurrentIndex; // 预缓冲区循环索引

    FTimerHandle VoiceEndTimer;
    
    // 长语音处理
    FTimerHandle LongSpeechTimer;
    float VoiceStartTime;
    int32 MaxBufferChunks;
    static constexpr float MaxSpeechDuration = 50.0f; // 50秒最大语音长度

    // 音频捕获 - 使用UE原生音频系统
    Audio::FAudioCapture* AudioCapture;
    
    TArray<uint8> AudioBuffer;
    
    // 音频录制参数
    int32 AudioSampleRate = 16000;
    int32 AudioChannels = 1;
    int32 AudioBufferSize = 1024;
    
    // 保存最后一次成功的音频配置
    int32 LastSuccessfulSampleRate = 16000;
    uint32 LastSuccessfulBufferSize = 960;
    int32 LastSuccessfulDeviceIndex = INDEX_NONE;
    int32 LastSuccessfulNumChannels = 1;
    
    // 模拟音频捕获定时器
    FTimerHandle AudioCaptureSimulationTimer;
    
    // 重采样相关
    bool bNeedResampling = false;
    float ResampleRatio = 1.0f;
    TArray<float> ResampleBuffer;

    // 回调函数
    UFUNCTION()
    void OnSpeechRecognizedInternal(const FString& RecognizedText);

    UFUNCTION()
    void OnSpeechSynthesizedInternal(const TArray<uint8>& SynthesizedAudio);

    UFUNCTION()
    void OnSpeechErrorInternal(const FString& ErrorMessage);
    
    // Dify API回调函数
    UFUNCTION()
    void OnDifyResponseReceivedInternal(const FString& Response);
    
    UFUNCTION()
    void OnDifyErrorReceivedInternal(const FString& ErrorMessage);

    // VAD回调函数
    UFUNCTION()
    void OnVADActivityChangedInternal(bool bVoiceDetected);

    // 音频处理
    void ProcessAudioData(const TArray<float>& AudioData);
    void SendAudioToSpeechRecognition(const TArray<float>& AudioData);
    void ProcessLongSpeechSegment(); // 处理长语音片段
    USoundWave* CreateSoundWaveFromAudioData(const TArray<uint8>& AudioData);

private:
    // 音频捕获相关 - UE原生音频系统
    void InitializeAudioCapture();
    void CleanupAudioCapture();
    
    // UE音频捕获相关
    bool StartUEAudioCapture();
    bool StopUEAudioCapture();
    
    // 备选的简单音频捕获（如果UE AudioCapture失败）
    bool StartSimpleAudioCapture();
    bool StopSimpleAudioCapture();
    
    // UE原生音频数据回调函数
    void OnNativeAudioData(const float* AudioData, int32 NumFrames, int32 NumChannels, int32 SampleRate);
    
    // 设置重采样参数
    void SetupResampling(int32 SampleRate, int32 NumChannels);
};