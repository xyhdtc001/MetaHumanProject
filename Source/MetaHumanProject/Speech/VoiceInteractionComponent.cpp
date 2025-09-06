#include "VoiceInteractionComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MetahumanCpp/MetaHumanPlayerController.h"

// UE音频录制支持
#include "AudioCaptureCore.h"
#include "AudioCapture.h"

UVoiceInteractionComponent::UVoiceInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    
    bIsListening = false;
    bIsSpeaking = false;
    bIsAudioCapturing = false;
    bCurrentVoiceActivity = false;
    bIsBufferingVoice = false;
    bContinuousRecognitionMode = true;
    
    VoiceStartTime = 0.0f;
    MaxBufferChunks = 3000; // 约3000个chunk，约3分钟的音频缓存上限
    
    // 初始化预缓冲区 - 缓冲大约500ms的音频数据
    // 以16kHz采样率，每个chunk约60ms，500ms需要约8个chunk
    PreBufferMaxChunks = 10; // 稍微多一点确保覆盖
    PreBufferCurrentIndex = 0;
    PreBuffer.Reserve(PreBufferMaxChunks);
    for (int32 i = 0; i < PreBufferMaxChunks; i++)
    {
        PreBuffer.Add(TArray<float>());
    }
    
    // 创建UE音频捕获组件
    AudioCapture = nullptr;
}

void UVoiceInteractionComponent::BeginPlay()
{
    Super::BeginPlay();

    // 获取Speech Manager实例
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        SpeechManager = GameInstance->GetSubsystem<USpeechManager>();
        if (SpeechManager)
        {
            // 绑定事件
            SpeechManager->OnSpeechRecognized.AddDynamic(this, &UVoiceInteractionComponent::OnSpeechRecognizedInternal);
            SpeechManager->OnSpeechSynthesized.AddDynamic(this, &UVoiceInteractionComponent::OnSpeechSynthesizedInternal);
            SpeechManager->OnSpeechError.AddDynamic(this, &UVoiceInteractionComponent::OnSpeechErrorInternal);

            UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Connected to SpeechManager"));

            // 如果设置了自动开始监听
            if (bAutoStartListening)
            {
                StartListening(DefaultLanguage);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Failed to get SpeechManager"));
        }
    }

    // 初始化VAD Manager
    UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: VAD initialization - bVADEnabled=%s"), bVADEnabled ? TEXT("true") : TEXT("false"));
    
    if (bVADEnabled)
    {
        VADManager = NewObject<UVoiceActivityManager>(this);
        if (VADManager)
        {
            UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: VADManager created successfully"));
            
            // 绑定VAD事件
            VADManager->OnVoiceActivityChanged.AddDynamic(this, &UVoiceInteractionComponent::OnVADActivityChangedInternal);
            
            // 设置VAD配置 - 为连续识别优化，减少频繁切换
            VADManager->bEnableSmoothing = bVADSmoothingEnabled;
            VADManager->VoiceStartThreshold = 5;  // 增加到5帧，减少误触发
            VADManager->VoiceEndThreshold = 30;   // 增加到30帧，避免过早结束
            
            // 初始化VAD
            UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Attempting to initialize VAD with mode %d, sample rate 16000"), static_cast<int32>(VADMode));
            
            if (VADManager->InitializeVAD(VADMode, 16000))
            {
                UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: VAD initialized successfully"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Failed to initialize VAD - Disabling VAD for this session"));
                // 如果VAD初始化失败，暂时禁用VAD但继续工作
                bVADEnabled = false;
                VADManager = nullptr;
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Failed to create VADManager - Disabling VAD for this session"));
            bVADEnabled = false;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: VAD is disabled in configuration"));
    }
    
    // 初始化Dify API Client
    if (bUseDifyForResponses)
    {
        DifyAPIClient = NewObject<UDifyAPIClient>(this);
        if (DifyAPIClient)
        {
            // 绑定事件
            DifyAPIClient->OnResponseReceived.AddDynamic(this, &UVoiceInteractionComponent::OnDifyResponseReceivedInternal);
            DifyAPIClient->OnErrorReceived.AddDynamic(this, &UVoiceInteractionComponent::OnDifyErrorReceivedInternal);
            
            // 初始化API客户端
            DifyAPIClient->Initialize(DifyBaseUrl, DifyApiKey);
            
            UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Dify API Client initialized with URL: %s"), *DifyBaseUrl);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Failed to create Dify API Client"));
        }
    }
    
    // 初始化音频捕获
    InitializeAudioCapture();
}

void UVoiceInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 停止所有活动
    if (bIsListening)
    {
        StopListening();
    }
    
    if (bIsAudioCapturing)
    {
        StopAudioCapture();
    }
    
    // 清理音频捕获
    CleanupAudioCapture();
    
    // 解绑事件
    if (SpeechManager)
    {
        SpeechManager->OnSpeechRecognized.RemoveDynamic(this, &UVoiceInteractionComponent::OnSpeechRecognizedInternal);
        SpeechManager->OnSpeechSynthesized.RemoveDynamic(this, &UVoiceInteractionComponent::OnSpeechSynthesizedInternal);
        SpeechManager->OnSpeechError.RemoveDynamic(this, &UVoiceInteractionComponent::OnSpeechErrorInternal);
    }
    
    if (VADManager)
    {
        VADManager->OnVoiceActivityChanged.RemoveDynamic(this, &UVoiceInteractionComponent::OnVADActivityChangedInternal);
        VADManager = nullptr;
    }
    
    // 清理Dify API Client
    if (DifyAPIClient)
    {
        DifyAPIClient->OnResponseReceived.RemoveDynamic(this, &UVoiceInteractionComponent::OnDifyResponseReceivedInternal);
        DifyAPIClient->OnErrorReceived.RemoveDynamic(this, &UVoiceInteractionComponent::OnDifyErrorReceivedInternal);
        DifyAPIClient = nullptr;
    }
    
    Super::EndPlay(EndPlayReason);
}

bool UVoiceInteractionComponent::StartListening(const FString& Language)
{
    if (!SpeechManager)
    {
        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: SpeechManager not available"));
        OnVoiceError.Broadcast(TEXT("Speech system not initialized"));
        return false;
    }

    if (bIsListening)
    {
        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Already listening"));
        return true;
    }

    // 设置监听状态和语言
    bIsListening = true;
    DefaultLanguage = Language;
    
    // 开始音频捕获
    if (!bIsAudioCapturing)
    {
        StartAudioCapture();
    }
    
    // 在连续识别模式下的处理
    UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Continuous recognition mode processing - bVADEnabled=%s, VADManager valid=%s, VAD initialized=%s"), 
           bVADEnabled ? TEXT("true") : TEXT("false"),
           VADManager ? TEXT("true") : TEXT("false"), 
           (VADManager && VADManager->IsVADInitialized()) ? TEXT("true") : TEXT("false"));
           
    if (bContinuousRecognitionMode)
    {
        if (bVADEnabled && VADManager && VADManager->IsVADInitialized())
        {
            UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Started listening in continuous mode (VAD-controlled) with language: %s"), *Language);
            // VAD模式：等待VAD检测到语音再启动识别会话
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: VAD disabled/failed - starting recognition session immediately for continuous mode with language: %s"), *Language);
            // 无VAD模式：立即启动识别会话
            if (SpeechManager->StartSpeechRecognition(Language))
            {
                UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Recognition session started successfully (no VAD mode)"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Failed to start recognition session (no VAD mode)"));
                bIsListening = false;
                StopAudioCapture();
                return false;
            }
        }
    }
    else
    {
        // 非连续模式：立即启动识别会话
        if (SpeechManager->StartSpeechRecognition(Language))
        {
            UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Started listening in standard mode with language: %s"), *Language);
        }
        else
        {
            bIsListening = false;
            StopAudioCapture();
            return false;
        }
    }
    
    return true;
}

bool UVoiceInteractionComponent::StopListening()
{
    if (!bIsListening)
    {
        return true;
    }

    if (SpeechManager)
    {
        SpeechManager->StopSpeechRecognition();
    }

    bIsListening = false;
    
    // 停止音频捕获
    if (bIsAudioCapturing)
    {
        StopAudioCapture();
    }

    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Stopped listening"));
    return true;
}

bool UVoiceInteractionComponent::SpeakText(const FString& Text, const FString& VoiceName)
{
    if (!SpeechManager)
    {
        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: SpeechManager not available"));
        OnVoiceError.Broadcast(TEXT("Speech system not initialized"));
        return false;
    }

    if (Text.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Text is empty"));
        OnVoiceError.Broadcast(TEXT("Text is empty"));
        return false;
    }

    bIsSpeaking = true;

    if (SpeechManager->SynthesizeText(Text, VoiceName))
    {
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Started synthesis for text: %s"), *Text);
        return true;
    }

    bIsSpeaking = false;
    return false;
}

void UVoiceInteractionComponent::SetSpeechSettings(const FString& AppID, const FString& Language, const FString& Voice)
{
    DefaultLanguage = Language;
    DefaultVoice = Voice;

    if (SpeechManager && !AppID.IsEmpty())
    {
        SpeechManager->InitializeSpeech(AppID);
    }

    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Updated settings - Language: %s, Voice: %s"), *Language, *Voice);
}

bool UVoiceInteractionComponent::StartAudioCapture()
{
    if (bIsAudioCapturing)
    {
        return true;
    }

    // 首先尝试UE原生音频捕获，如果失败则尝试备选方案
    return StartUEAudioCapture();
}

bool UVoiceInteractionComponent::StopAudioCapture()
{
    if (!bIsAudioCapturing)
    {
        return true;
    }

    // 尝试停止UE音频捕获，如果失败则尝试停止备选方案
    bool result = StopUEAudioCapture();
    if (!result)
    {
        result = StopSimpleAudioCapture();
    }
    
    bIsAudioCapturing = false;
    return result;
}

void UVoiceInteractionComponent::OnSpeechRecognizedInternal(const FString& RecognizedText)
{
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Recognition result: %s"), *RecognizedText);
    
    // 广播识别结果
    OnRecognitionResult.Broadcast(RecognizedText);
    
    // 如果启用了Dify API，使用它生成响应，但不影响原有流程
    if (bUseDifyForResponses && DifyAPIClient && !RecognizedText.IsEmpty())
    {
        // 在单独的任务中处理Dify API调用，不影响主流程
        AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, RecognizedText]()
        {
            GenerateResponseWithDify(RecognizedText);
        });
    }
    
    // 识别成功后立即处理会话状态
    if (bContinuousRecognitionMode && !RecognizedText.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Recognition completed - cleaning up session state"));
        
        // 立即停止缓冲，防止重复识别
        if (bIsBufferingVoice)
        {
            UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Stopping voice buffering after recognition"));
            bIsBufferingVoice = false;
            VoiceBuffer.Empty();
        }
        
        // 清除任何待处理的定时器
        if (VoiceEndTimer.IsValid())
        {
            GetWorld()->GetTimerManager().ClearTimer(VoiceEndTimer);
        }
        if (LongSpeechTimer.IsValid())
        {
            GetWorld()->GetTimerManager().ClearTimer(LongSpeechTimer);
        }
        
        // 检查是否为无VAD的连续识别模式
        bool bIsNoVADContinuousMode = (!bVADEnabled || !VADManager || !VADManager->IsVADInitialized());
        
        // 延迟结束当前会话并重启新会话（仅在无VAD连续模式下）
        FTimerHandle SessionCleanupTimer;
        GetWorld()->GetTimerManager().SetTimer(SessionCleanupTimer, [this, bIsNoVADContinuousMode]()
        {
            if (SpeechManager && SpeechManager->IsRecognitionActive())
            {
                UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Ending recognition session after successful recognition"));
                SpeechManager->StopSpeechRecognition();
            }
            
            // 在无VAD的连续识别模式下，立即重新启动新的识别会话
            if (bIsNoVADContinuousMode && bIsListening && SpeechManager)
            {
                UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Restarting recognition session for continuous no-VAD mode"));
                
                // 短暂延迟后重新开始识别会话
                FTimerHandle RestartTimer;
                GetWorld()->GetTimerManager().SetTimer(RestartTimer, [this]()
                {
                    if (bIsListening && SpeechManager && !SpeechManager->IsRecognitionActive())
                    {
                        if (SpeechManager->StartSpeechRecognition(DefaultLanguage))
                        {
                            UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Successfully restarted recognition session for continuous listening"));
                        }
                        else
                        {
                            UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Failed to restart recognition session"));
                        }
                    }
                }, 0.3f, false);
            }
        }, 0.2f, false); // 短暂延迟，确保识别完成
    }
}

void UVoiceInteractionComponent::OnSpeechSynthesizedInternal(const TArray<uint8>& SynthesizedAudio)
{
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Synthesis complete, audio size: %d bytes"), SynthesizedAudio.Num());
    
    bIsSpeaking = false;

    // 将音频数据转换为SoundWave
    USoundWave* GeneratedSound = CreateSoundWaveFromAudioData(SynthesizedAudio);
    if (GeneratedSound)
    {
        // 广播合成完成事件
        OnSynthesisComplete.Broadcast(GeneratedSound);
    }

    // 尝试集成MetaHuman播放（检查是否有MetaHumanPlayerController）
    if (AActor* Owner = GetOwner())
    {
        // 检查是否为玩家控制的Pawn
        if (APawn* OwnerPawn = Cast<APawn>(Owner))
        {
            if (AMetaHumanPlayerController* MetaHumanController = Cast<AMetaHumanPlayerController>(OwnerPawn->GetController()))
            {
                UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Using MetaHuman controller for speech playback"));
                
                // 使用MetaHuman控制器播放语音，包含唇形同步
                MetaHumanController->PlayHumanSpeech(SynthesizedAudio, TEXT("Default"), TEXT("Speaking"));
                return;
            }
        }
        
        // 检查Owner本身是否为MetaHumanPlayerController
        if (AMetaHumanPlayerController* MetaHumanController = Cast<AMetaHumanPlayerController>(Owner))
        {
            UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Owner is MetaHuman controller, using for speech playback"));
            MetaHumanController->PlayHumanSpeech(SynthesizedAudio, TEXT("Default"), TEXT("Speaking"));
            return;
        }
        
        // 尝试通过GameMode获取MetaHuman控制器
        if (UWorld* World = GetWorld())
        {
            if (APlayerController* PC = World->GetFirstPlayerController())
            {
                if (AMetaHumanPlayerController* MetaHumanController = Cast<AMetaHumanPlayerController>(PC))
                {
                    UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Using first MetaHuman player controller for speech playback"));
                    MetaHumanController->PlayHumanSpeech(SynthesizedAudio, TEXT("Default"), TEXT("Speaking"));
                    return;
                }
            }
        }
    }

    // 如果没有找到MetaHuman控制器，使用常规的音频播放
    UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: MetaHuman controller not found, using regular audio playback"));
    
    if (GeneratedSound && GetOwner())
    {
        if (USceneComponent* RootComponent = GetOwner()->GetRootComponent())
        {
            // 创建音频组件并播放
            if (UWorld* World = GetWorld())
            {
                // 播放声音
                UAudioComponent* AudioComponent = nullptr;
                if (GetOwner()->GetRootComponent())
                {
                    AudioComponent = UGameplayStatics::SpawnSoundAtLocation(
                        World,
                        GeneratedSound,
                        GetOwner()->GetActorLocation()
                    );
                    
                    if (AudioComponent)
                    {
                        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Playing synthesized speech via SpawnSoundAtLocation"));
                    }
                }
            }
        }
    }
}

void UVoiceInteractionComponent::OnSpeechErrorInternal(const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Speech error: %s"), *ErrorMessage);
    
    bIsSpeaking = false;
    
    // 广播错误事件
    OnVoiceError.Broadcast(ErrorMessage);
}

void UVoiceInteractionComponent::OnVADActivityChangedInternal(bool bVoiceDetected)
{
    bCurrentVoiceActivity = bVoiceDetected;
    
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Voice activity changed: %s"), bVoiceDetected ? TEXT("Active") : TEXT("Inactive"));
    
    // 广播VAD事件
    OnVoiceActivityChanged.Broadcast(bVoiceDetected);
    
    // 基于VAD的智能语音识别会话管理 - 使用缓冲策略
    if (bContinuousRecognitionMode && SpeechManager)
    {
        if (bVoiceDetected)
        {
            // 检测到语音活动：开始缓冲语音数据
            if (!bIsBufferingVoice)
            {
                UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Voice detected - Starting voice buffering"));
                bIsBufferingVoice = true;
                VoiceBuffer.Empty();
                VoiceStartTime = FPlatformTime::Seconds();
                
                // 清除任何待处理的结束定时器
                if (VoiceEndTimer.IsValid())
                {
                    GetWorld()->GetTimerManager().ClearTimer(VoiceEndTimer);
                }
                
                // 启动新的语音识别会话
                if (!SpeechManager->IsRecognitionActive())
                {
                    SpeechManager->StartSpeechRecognition(DefaultLanguage);
                }
                
                // **关键修复：发送预缓冲的数据，防止第一个字丢失**
                UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Sending pre-buffered audio data to prevent speech loss"));
                
                // 按正确顺序发送预缓冲区中的数据
                for (int32 i = 0; i < PreBufferMaxChunks; i++)
                {
                    int32 Index = (PreBufferCurrentIndex + i) % PreBufferMaxChunks;
                    if (PreBuffer[Index].Num() > 0)
                    {
                        SendAudioToSpeechRecognition(PreBuffer[Index]);
                        // 也添加到语音缓冲区用于长语音处理
                        VoiceBuffer.Add(PreBuffer[Index]);
                    }
                }
                
                UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Sent %d pre-buffered chunks to prevent speech start loss"), 
                       PreBufferMaxChunks);
                
                // 设置长语音保护定时器 - 50秒后自动处理
                GetWorld()->GetTimerManager().SetTimer(LongSpeechTimer, [this]()
                {
                    if (bIsBufferingVoice && bCurrentVoiceActivity)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Long speech detected (>50s) - Force processing to avoid timeout"));
                        ProcessLongSpeechSegment();
                    }
                }, MaxSpeechDuration, false);
            }
        }
        else
        {
            // 语音活动结束：设置延迟处理
            if (bIsBufferingVoice)
            {
                UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Voice ended - Scheduling buffer processing"));
                
                // 延迟1.5秒处理，给识别结果回调一些时间先处理
                GetWorld()->GetTimerManager().SetTimer(VoiceEndTimer, [this]()
                {
                    // 只有在还在缓冲状态且没有语音活动时才处理
                    if (!bCurrentVoiceActivity && bIsBufferingVoice)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Processing voice end after no recognition result"));
                        
                        // 如果还有缓冲数据且识别会话还活跃，发送剩余数据
                        if (VoiceBuffer.Num() > 0 && SpeechManager && SpeechManager->IsRecognitionActive())
                        {
                            UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Sending remaining buffered data (%d chunks)"), VoiceBuffer.Num());
                            for (const TArray<float>& AudioChunk : VoiceBuffer)
                            {
                                SendAudioToSpeechRecognition(AudioChunk);
                            }
                        }
                        
                        // 清理状态
                        VoiceBuffer.Empty();
                        bIsBufferingVoice = false;
                        
                        // 延迟结束会话
                        FTimerHandle SessionEndTimer;
                        GetWorld()->GetTimerManager().SetTimer(SessionEndTimer, [this]()
                        {
                            if (SpeechManager && SpeechManager->IsRecognitionActive())
                            {
                                UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Ending session after voice inactivity timeout"));
                                SpeechManager->StopSpeechRecognition();
                            }
                        }, 0.5f, false);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Voice end timer cancelled - recognition already handled or voice resumed"));
                    }
                }, 1.5f, false);
            }
        }
    }
}

void UVoiceInteractionComponent::ProcessAudioData(const TArray<float>& AudioData)
{
    if (AudioData.Num() == 0)
    {
        return;
    }

    // 连续识别模式：基于VAD的智能语音识别 - 使用预缓冲和缓冲策略
    if (bIsListening && SpeechManager && bContinuousRecognitionMode)
    {
        if (bVADEnabled && VADManager && VADManager->IsVADInitialized())
        {
            // 持续进行预缓冲 - 无论是否在语音状态
            // 使用循环缓冲区存储最近的音频数据
            PreBuffer[PreBufferCurrentIndex] = AudioData;
            PreBufferCurrentIndex = (PreBufferCurrentIndex + 1) % PreBufferMaxChunks;
            
            // 处理VAD检测
            VADManager->ProcessFloatAudioForVAD(AudioData, 16000, 1);
            
            // 在缓冲模式下收集语音数据
            if (bIsBufferingVoice)
            {
                // 检查缓冲区大小限制
                if (VoiceBuffer.Num() >= MaxBufferChunks)
                {
                    UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Voice buffer full (%d chunks) - Force processing segment"), MaxBufferChunks);
                    ProcessLongSpeechSegment();
                }
                
                // 缓冲语音数据
                VoiceBuffer.Add(AudioData);
                UE_LOG(LogTemp, VeryVerbose, TEXT("VoiceInteractionComponent: Buffering audio data, chunk size: %d, total chunks: %d"), AudioData.Num(), VoiceBuffer.Num());
                
                // 实时发送数据以便获得即时反馈
                if (SpeechManager->IsRecognitionActive())
                {
                    SendAudioToSpeechRecognition(AudioData);
                }
            }
            else
            {
                // 不在缓冲模式，但仍进行预缓冲
                UE_LOG(LogTemp, VeryVerbose, TEXT("VoiceInteractionComponent: Pre-buffering audio data (chunk %d/%d)"), 
                       PreBufferCurrentIndex, PreBufferMaxChunks);
            }
        }
        else
        {
            // VAD未启用或初始化失败，使用简化的连续模式
            UE_LOG(LogTemp, VeryVerbose, TEXT("VoiceInteractionComponent: VAD disabled - using simplified continuous mode"));
            
            // 直接发送音频数据（识别会话已在StartListening中启动）
            if (SpeechManager->IsRecognitionActive())
            {
                SendAudioToSpeechRecognition(AudioData);
            }
            else
            {
                // 识别会话可能在识别成功后暂时停止并重启，这是正常的
                // 只在长时间没有活跃会话时才记录警告
                static double LastErrorLogTime = 0.0;
                double CurrentTime = FPlatformTime::Seconds();
                if (CurrentTime - LastErrorLogTime > 10.0) // 增加到10秒，减少不必要的日志
                {
                    UE_LOG(LogTemp, VeryVerbose, TEXT("VoiceInteractionComponent: Recognition session temporarily not active (may be restarting after successful recognition)"));
                    LastErrorLogTime = CurrentTime;
                }
            }
        }
    }
}

void UVoiceInteractionComponent::SendAudioToSpeechRecognition(const TArray<float>& AudioData)
{
    if (!bIsListening || !SpeechManager)
    {
        return;
    }

    // 将float音频数据转换为int16格式
    TArray<uint8> ConvertedData;
    ConvertedData.Reserve(AudioData.Num() * sizeof(int16));

    for (float Sample : AudioData)
    {
        // 限制范围并转换为int16
        float ClampedSample = FMath::Clamp(Sample, -1.0f, 1.0f);
        int16 IntSample = static_cast<int16>(ClampedSample * 32767.0f);
        
        ConvertedData.Append(reinterpret_cast<uint8*>(&IntSample), sizeof(int16));
    }

    // 添加定期调试日志（每秒一次）以避免日志过多
    static double LastLogTime = 0.0;
    double CurrentTime = FPlatformTime::Seconds();
    if (CurrentTime - LastLogTime > 1.0) // 每秒记录一次
    {
        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Continuous recognition active - converting %d float samples to %d bytes, bIsListening=%s"), 
               AudioData.Num(), ConvertedData.Num(), bIsListening ? TEXT("true") : TEXT("false"));
        LastLogTime = CurrentTime;
    }

    // 发送到语音识别
    if (ConvertedData.Num() > 0)
    {
        SpeechManager->WriteSpeechData(ConvertedData);
    }
}

USoundWave* UVoiceInteractionComponent::CreateSoundWaveFromAudioData(const TArray<uint8>& AudioData)
{
    if (AudioData.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: CreateSoundWaveFromAudioData - Empty audio data"));
        return nullptr;
    }

    // 检查是否为WAV格式（有完整的WAV头）
    bool bHasWavHeader = false;
    int32 PCMDataOffset = 0;
    int32 PCMDataSize = AudioData.Num();
    
    if (AudioData.Num() >= 44) // 最小WAV头大小
    {
        // 检查RIFF头
        if (AudioData[0] == 'R' && AudioData[1] == 'I' && AudioData[2] == 'F' && AudioData[3] == 'F' &&
            AudioData[8] == 'W' && AudioData[9] == 'A' && AudioData[10] == 'V' && AudioData[11] == 'E')
        {
            bHasWavHeader = true;
            PCMDataOffset = 44; // 标准WAV头大小
            PCMDataSize = AudioData.Num() - PCMDataOffset;
            
            UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Detected WAV format - Header size: %d, PCM data size: %d"), 
                   PCMDataOffset, PCMDataSize);
        }
    }

    if (PCMDataSize <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: CreateSoundWaveFromAudioData - No PCM data found"));
        return nullptr;
    }

    // 创建新的SoundWave对象
    USoundWave* SoundWave = NewObject<USoundWave>();
    if (!SoundWave)
    {
        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Failed to create SoundWave object"));
        return nullptr;
    }

    // 设置音频格式参数（iFlyTek TTS输出格式：16kHz, 16bit, 单声道）
    SoundWave->NumChannels = 1; // 单声道
    const int32 SampleRate = 16000; // 16kHz采样率
    SoundWave->SetSampleRate(SampleRate);
    
    // 计算时长（PCM数据字节数 / (采样率 * 声道数 * 每样本字节数)）
    const int32 BytesPerSample = 2; // 16位 = 2字节
    SoundWave->Duration = static_cast<float>(PCMDataSize) / (SampleRate * SoundWave->NumChannels * BytesPerSample);
    
    // 设置音频格式
    SoundWave->SoundGroup = SOUNDGROUP_Default;
    SoundWave->bLooping = false;
    
    // 设置RawPCMData（只使用PCM数据部分，跳过WAV头）
    SoundWave->RawPCMDataSize = PCMDataSize;
    if (SoundWave->RawPCMData != nullptr)
    {
        FMemory::Free(SoundWave->RawPCMData);
    }
    
    SoundWave->RawPCMData = (uint8*)FMemory::Malloc(SoundWave->RawPCMDataSize);
    FMemory::Memcpy(SoundWave->RawPCMData, AudioData.GetData() + PCMDataOffset, SoundWave->RawPCMDataSize);
    
    // 设置总样本数
    SoundWave->TotalSamples = PCMDataSize / BytesPerSample;

    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Created SoundWave - Duration: %.2fs, Samples: %lld, Size: %d bytes"), 
           SoundWave->Duration, (int64)SoundWave->TotalSamples, SoundWave->RawPCMDataSize);

    return SoundWave;
}

void UVoiceInteractionComponent::ProcessLongSpeechSegment()
{
    if (!bIsBufferingVoice || !SpeechManager)
    {
        return;
    }
    
    float CurrentTime = FPlatformTime::Seconds();
    float SpeechDuration = CurrentTime - VoiceStartTime;
    
    UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Processing long speech segment - Duration: %.1f seconds, Buffer chunks: %d"), 
           SpeechDuration, VoiceBuffer.Num());
    
    // 结束当前识别会话，获取到目前为止的识别结果
    if (SpeechManager->IsRecognitionActive())
    {
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Ending current session to get partial results"));
        SpeechManager->StopSpeechRecognition();
    }
    
    // 清除长语音定时器
    if (LongSpeechTimer.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(LongSpeechTimer);
    }
    
    // 如果语音还在继续，重新开始一个新的会话
    if (bCurrentVoiceActivity)
    {
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Voice still active - Starting new segment"));
        
        // 短暂延迟后启动新会话，给上一个会话的识别结果一些时间
        FTimerHandle NewSessionTimer;
        GetWorld()->GetTimerManager().SetTimer(NewSessionTimer, [this]()
        {
            if (bCurrentVoiceActivity && SpeechManager)
            {
                VoiceBuffer.Empty(); // 清空缓冲区，开始新段
                VoiceStartTime = FPlatformTime::Seconds(); // 重置开始时间
                
                SpeechManager->StartSpeechRecognition(DefaultLanguage);
                
                // 重新设置长语音保护定时器
                GetWorld()->GetTimerManager().SetTimer(LongSpeechTimer, [this]()
                {
                    if (bIsBufferingVoice && bCurrentVoiceActivity)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Another long speech segment detected - Force processing again"));
                        ProcessLongSpeechSegment();
                    }
                }, MaxSpeechDuration, false);
            }
        }, 0.3f, false);
    }
    else
    {
        // 语音已结束，清理状态
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Voice ended during long speech processing - Cleaning up"));
        VoiceBuffer.Empty();
        bIsBufferingVoice = false;
    }
}

void UVoiceInteractionComponent::InitializeAudioCapture()
{
    // 创建UE原生音频捕获
    if (AudioCapture == nullptr)
    {
        AudioCapture = new Audio::FAudioCapture();
        if (AudioCapture)
        {
            UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Native UE audio capture created successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Failed to create native UE audio capture"));
        }
    }
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Audio capture initialized"));
}

void UVoiceInteractionComponent::CleanupAudioCapture()
{
    // 清理UE原生音频捕获
    if (AudioCapture)
    {
        // 确保录音已停止
        if (bIsAudioCapturing)
        {
            StopAudioCapture();
        }
        
        // 删除音频捕获实例
        delete AudioCapture;
        AudioCapture = nullptr;
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Native UE audio capture cleaned up"));
    }
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Audio capture cleaned up"));
}

// VAD函数实现
bool UVoiceInteractionComponent::InitializeVAD(EVADMode Mode, int32 SampleRate)
{
    if (!VADManager)
    {
        VADManager = NewObject<UVoiceActivityManager>(this);
        if (VADManager)
        {
            VADManager->OnVoiceActivityChanged.AddDynamic(this, &UVoiceInteractionComponent::OnVADActivityChangedInternal);
        }
    }

    if (VADManager)
    {
        VADMode = Mode;
        return VADManager->InitializeVAD(Mode, SampleRate);
    }

    return false;
}

bool UVoiceInteractionComponent::SetVADMode(EVADMode Mode)
{
    VADMode = Mode;
    if (VADManager)
    {
        return VADManager->SetVADMode(Mode);
    }
    return false;
}

bool UVoiceInteractionComponent::IsVADInitialized() const
{
    return VADManager ? VADManager->IsVADInitialized() : false;
}

bool UVoiceInteractionComponent::ResetVAD()
{
    if (VADManager)
    {
        return VADManager->ResetVAD();
    }
    return false;
}

// UE音频捕获具体实现
bool UVoiceInteractionComponent::StartUEAudioCapture()
{
    // 首先尝试UE原生AudioCapture
    if (AudioCapture)
    {
        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Attempting audio capture with UE WASAPI"));
        
        // 如果之前有音频捕获会话，确保它已经被清理
        StopUEAudioCapture();
        
        // 添加短暂延迟，让系统有时间释放资源
        FPlatformProcess::Sleep(0.3f);
        
        // 获取可用设备列表
        TArray<Audio::FCaptureDeviceInfo> AvailableDevices;
        int32 NumDevices = AudioCapture->GetCaptureDevicesAvailable(AvailableDevices);
        
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Found %d audio capture devices"), NumDevices);
        
        // 记录所有可用设备的信息
        for (int32 DeviceIndex = 0; DeviceIndex < AvailableDevices.Num(); ++DeviceIndex)
        {
            const Audio::FCaptureDeviceInfo& DeviceInfo = AvailableDevices[DeviceIndex];
            UE_LOG(LogTemp, Log, TEXT("Device %d: %s - SampleRate: %d, Channels: %d"), 
                   DeviceIndex, *DeviceInfo.DeviceName, DeviceInfo.PreferredSampleRate, DeviceInfo.InputChannels);
        }
        
        // 尝试不同的设备
        TArray<int32> DeviceIndices;
        DeviceIndices.Add(INDEX_NONE); // 默认设备
        
        // 添加所有可用设备的索引
        for (int32 i = 0; i < AvailableDevices.Num(); ++i)
        {
            DeviceIndices.AddUnique(i);
        }
        
        // 首先尝试使用上次成功的配置（如果有）
        if (LastSuccessfulSampleRate > 0 && LastSuccessfulBufferSize > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Trying last successful configuration first - SampleRate: %d, BufferSize: %d, DeviceIndex: %d, Channels: %d"), 
                   LastSuccessfulSampleRate, LastSuccessfulBufferSize, LastSuccessfulDeviceIndex, LastSuccessfulNumChannels);
            
            // 设置音频格式参数
            Audio::FAudioCaptureDeviceParams DeviceParams;
            DeviceParams.DeviceIndex = LastSuccessfulDeviceIndex;
            DeviceParams.SampleRate = LastSuccessfulSampleRate;
            DeviceParams.NumInputChannels = LastSuccessfulNumChannels;
            
            // 创建回调函数
            Audio::FOnAudioCaptureFunction OnCapture = [this](const void* AudioData, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverFlow)
            {
                OnNativeAudioData(static_cast<const float*>(AudioData), NumFrames, NumChannels, SampleRate);
            };
            
            // 尝试使用上次成功的配置
            bool bOpenSuccess = false;
            try
            {
                bOpenSuccess = AudioCapture->OpenAudioCaptureStream(DeviceParams, MoveTemp(OnCapture), LastSuccessfulBufferSize);
            }
            catch (...)
            {
                UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Exception during OpenAudioCaptureStream with last successful config"));
            }
            
            if (bOpenSuccess)
            {
                bool bStartSuccess = false;
                try
                {
                    bStartSuccess = AudioCapture->StartStream();
                }
                catch (...)
                {
                    UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Exception during StartStream with last successful config"));
                }
                
                if (bStartSuccess)
                {
                    bIsAudioCapturing = true;
                    UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Audio capture started successfully with last successful configuration"));
                    
                    // 设置重采样参数
                    SetupResampling(LastSuccessfulSampleRate, LastSuccessfulNumChannels);
                    
                    return true;
                }
                else
                {
                    try
                    {
                        AudioCapture->CloseStream();
                    }
                    catch (...)
                    {
                        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Exception during CloseStream"));
                    }
                    UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Failed to start stream with last successful configuration, trying alternatives"));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Failed to open stream with last successful configuration, trying alternatives"));
            }
            
            // 在尝试其他配置前添加短暂延迟，让系统有时间释放资源
            FPlatformProcess::Sleep(0.2f);
        }
        
        // 尝试每个设备
        for (int32 DeviceIndex : DeviceIndices)
        {
            // 获取设备信息
            Audio::FCaptureDeviceInfo DeviceInfo;
            bool bGotDeviceInfo = false;
            
            try
            {
                bGotDeviceInfo = AudioCapture->GetCaptureDeviceInfo(DeviceInfo, DeviceIndex);
            }
            catch (...)
            {
                UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Exception during GetCaptureDeviceInfo for device %d"), DeviceIndex);
            }
            
            if (bGotDeviceInfo)
            {
                UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Device %d info - Name: %s, SampleRate: %d, Channels: %d"), 
                       DeviceIndex, *DeviceInfo.DeviceName, DeviceInfo.PreferredSampleRate, DeviceInfo.InputChannels);
                
                // 尝试两种通道配置：设备首选通道数和立体声(2)
                TArray<int32> ChannelConfigs;
                ChannelConfigs.Add(DeviceInfo.InputChannels); // 设备首选通道数
                if (DeviceInfo.InputChannels != 2)
                {
                    ChannelConfigs.Add(2); // 立体声
                }
                if (DeviceInfo.InputChannels != 1)
                {
                    ChannelConfigs.Add(1); // 单声道
                }
                
                // 尝试不同的采样率
                TArray<int32> SampleRates;
                SampleRates.Add(DeviceInfo.PreferredSampleRate); // 设备首选采样率
                
                // 添加其他常见采样率（如果与首选不同）
                if (DeviceInfo.PreferredSampleRate != 48000) SampleRates.Add(48000);
                if (DeviceInfo.PreferredSampleRate != 44100) SampleRates.Add(44100);
                if (DeviceInfo.PreferredSampleRate != 16000) SampleRates.Add(16000);
                if (DeviceInfo.PreferredSampleRate != 8000) SampleRates.Add(8000);
                
                // 尝试每种通道配置和采样率组合
                for (int32 NumChannels : ChannelConfigs)
                {
                    for (int32 SampleRate : SampleRates)
                    {
                        // 计算合适的缓冲区大小（约60ms的音频）
                        uint32 BufferSize = (SampleRate * 60) / 1000;
                        
                        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Trying device %d with SampleRate: %d, Channels: %d, BufferSize: %d"), 
                               DeviceIndex, SampleRate, NumChannels, BufferSize);
                        
                        // 设置音频格式参数
                        Audio::FAudioCaptureDeviceParams DeviceParams;
                        DeviceParams.DeviceIndex = DeviceIndex;
                        DeviceParams.SampleRate = SampleRate;
                        DeviceParams.NumInputChannels = NumChannels;
                        
                        // 创建回调函数
                        Audio::FOnAudioCaptureFunction OnCapture = [this](const void* AudioData, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverFlow)
                        {
                            OnNativeAudioData(static_cast<const float*>(AudioData), NumFrames, NumChannels, SampleRate);
                        };
                        
                        // 尝试打开音频设备
                        bool bOpenSuccess = false;
                        try
                        {
                            bOpenSuccess = AudioCapture->OpenAudioCaptureStream(DeviceParams, MoveTemp(OnCapture), BufferSize);
                        }
                        catch (...)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Exception during OpenAudioCaptureStream"));
                        }
                        
                        if (bOpenSuccess)
                        {
                            // 尝试启动流
                            bool bStartSuccess = false;
                            try
                            {
                                bStartSuccess = AudioCapture->StartStream();
                            }
                            catch (...)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Exception during StartStream"));
                            }
                            
                            if (bStartSuccess)
                            {
                                bIsAudioCapturing = true;
                                UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Audio capture started successfully with device %d, SampleRate: %d, Channels: %d"), 
                                       DeviceIndex, SampleRate, NumChannels);
                                
                                // 记录成功的配置
                                LastSuccessfulSampleRate = SampleRate;
                                LastSuccessfulBufferSize = BufferSize;
                                LastSuccessfulDeviceIndex = DeviceIndex;
                                LastSuccessfulNumChannels = NumChannels;
                                
                                // 设置重采样参数
                                SetupResampling(SampleRate, NumChannels);
                                
                                return true;
                            }
                            else
                            {
                                // 关闭流并继续尝试
                                try
                                {
                                    AudioCapture->CloseStream();
                                }
                                catch (...)
                                {
                                    UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Exception during CloseStream"));
                                }
                            }
                        }
                        
                        // 在尝试下一个配置前添加短暂延迟，让系统有时间释放资源
                        FPlatformProcess::Sleep(0.1f);
                    }
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Failed to get device %d info"), DeviceIndex);
            }
        }
        
        // 如果所有尝试都失败，使用备选方案
        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: All UE AudioCapture configurations failed"));
        return StartSimpleAudioCapture();
    }
    
    return false;
}

bool UVoiceInteractionComponent::StopUEAudioCapture()
{
    if (!AudioCapture)
    {
        return true;
    }

    // 停止录音
    AudioCapture->StopStream();
    AudioCapture->CloseStream();

    bIsAudioCapturing = false;
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Stopped native UE audio capture"));
    return true;
}

// 备选的简单音频捕获实现
bool UVoiceInteractionComponent::StartSimpleAudioCapture()
{
    // 这是一个备选方案 - 暂时返回false并记录信息
    // 实际项目中可能需要：
    // 1. 使用UE的其他音频组件
    // 2. 使用第三方音频库
    // 3. 或者让用户知道需要外部音频输入
    
    UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Simple audio capture not implemented yet."));
    UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Consider:"));
    UE_LOG(LogTemp, Error, TEXT("  1. Checking Windows microphone permissions"));
    UE_LOG(LogTemp, Error, TEXT("  2. Ensuring no other application is using the microphone exclusively"));
    UE_LOG(LogTemp, Error, TEXT("  3. Using external audio input tools"));
    
    // 可以在这里实现一个简单的定时器来生成测试音频数据
    // 用于测试语音识别管道的其余部分
    
    return false;
}

bool UVoiceInteractionComponent::StopSimpleAudioCapture()
{
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Simple audio capture stopped"));
    return true;
}
void UVoiceInteractionComponent::SetupResampling(int32 SampleRate, int32 NumChannels)
{
    // 语音识别只支持16000Hz采样率和单声道
    bNeedResampling = (SampleRate != 16000 || NumChannels != 1);
    
    if (bNeedResampling)
    {
        // 计算重采样比例
        ResampleRatio = 16000.0f / static_cast<float>(SampleRate);
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Setup resampling - SampleRate: %d -> 16000, Channels: %d -> 1, ResampleRatio: %f"), 
               SampleRate, NumChannels, ResampleRatio);
    }
    else
    {
        ResampleRatio = 1.0f;
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: No resampling needed - SampleRate: %d, Channels: %d"), SampleRate, NumChannels);
    }
}

void UVoiceInteractionComponent::OnNativeAudioData(const float* AudioData, int32 NumFrames, int32 NumChannels, int32 SampleRate)
{
    if (!AudioData || NumFrames <= 0 || NumChannels <= 0)
    {
        return;
    }

    // 检查是否需要重采样
    if (bNeedResampling)
    {
        // 如果需要重采样，处理采样率和通道数转换
        
        // 计算目标帧数（重采样后）
        int32 TargetFrames = FMath::CeilToInt(NumFrames * ResampleRatio);
        
        // 准备重采样后的数据缓冲区
        TArray<float> ResampledData;
        ResampledData.SetNumZeroed(TargetFrames);
        
        // 对于通道数转换（多通道到单声道），我们取所有通道的平均值
        for (int32 i = 0; i < TargetFrames; i++)
        {
            // 计算原始数据中的索引（可能是小数）
            float SourceIdx = i / ResampleRatio;
            int32 SourceIdxFloor = FMath::FloorToInt(SourceIdx);
            int32 SourceIdxCeil = FMath::Min(FMath::CeilToInt(SourceIdx), NumFrames - 1);
            float Fraction = SourceIdx - SourceIdxFloor;
            
            // 对每个通道进行线性插值，然后取平均值
            float SampleValue = 0.0f;
            for (int32 Channel = 0; Channel < NumChannels; Channel++)
            {
                float Sample1 = AudioData[SourceIdxFloor * NumChannels + Channel];
                float Sample2 = AudioData[SourceIdxCeil * NumChannels + Channel];
                
                // 线性插值
                float InterpolatedSample = FMath::Lerp(Sample1, Sample2, Fraction);
                
                // 累加所有通道的值
                SampleValue += InterpolatedSample;
            }
            
            // 计算平均值
            ResampledData[i] = SampleValue / NumChannels;
        }
        
        // 在游戏线程中处理重采样后的音频数据
        TArray<float> FinalData = ResampledData;
        AsyncTask(ENamedThreads::GameThread, [this, FinalData]()
        {
            ProcessAudioData(FinalData);
        });
    }
    else
    {
        // 如果不需要重采样，直接转换为TArray<float>
        TArray<float> AudioDataArray;
        AudioDataArray.Reserve(NumFrames);
        
        for (int32 i = 0; i < NumFrames; i++)
        {
            AudioDataArray.Add(AudioData[i]);
        }
        
        // 在游戏线程中处理音频数据
        AsyncTask(ENamedThreads::GameThread, [this, AudioDataArray]()
        {
            ProcessAudioData(AudioDataArray);
        });
    }
}

// 添加Dify API相关方法
void UVoiceInteractionComponent::InitializeDifyAPI(const FString& BaseUrl, const FString& ApiKey)
{
    DifyBaseUrl = BaseUrl;
    DifyApiKey = ApiKey;
    bUseDifyForResponses = true;
    
    if (!DifyAPIClient)
    {
        DifyAPIClient = NewObject<UDifyAPIClient>(this);
        if (DifyAPIClient)
        {
            // 绑定事件
            DifyAPIClient->OnResponseReceived.AddDynamic(this, &UVoiceInteractionComponent::OnDifyResponseReceivedInternal);
            DifyAPIClient->OnErrorReceived.AddDynamic(this, &UVoiceInteractionComponent::OnDifyErrorReceivedInternal);
        }
    }
    
    if (DifyAPIClient)
    {
        DifyAPIClient->Initialize(BaseUrl, ApiKey);
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Dify API Client initialized with URL: %s"), *BaseUrl);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Failed to create Dify API Client"));
    }
}

void UVoiceInteractionComponent::SendToDifyAPI(const FString& Query, const FString& ConversationId)
{
    if (!DifyAPIClient)
    {
        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Dify API Client not initialized"));
        OnVoiceError.Broadcast(TEXT("Dify API Client not initialized"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Sending query to Dify API: %s"), *Query);
    DifyAPIClient->SendChatMessage(Query, ConversationId);
}

void UVoiceInteractionComponent::GenerateResponseWithDify(const FString& RecognizedText)
{
    if (!bUseDifyForResponses || !DifyAPIClient)
    {
        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionComponent: Dify API is not enabled or initialized"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Generating response with Dify for: %s"), *RecognizedText);
    SendToDifyAPI(RecognizedText, DifyAPIClient->GetCurrentConversationId());
}

FString UVoiceInteractionComponent::GetCurrentConversationId() const
{
    if (DifyAPIClient)
    {
        return DifyAPIClient->GetCurrentConversationId();
    }
    return TEXT("");
}

// 添加Dify API回调函数
void UVoiceInteractionComponent::OnDifyResponseReceivedInternal(const FString& Response)
{
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionComponent: Received response from Dify API: %s"), *Response);
    
    // 广播响应
    OnDifyResponseReceived.Broadcast(Response);
}

void UVoiceInteractionComponent::OnDifyErrorReceivedInternal(const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("VoiceInteractionComponent: Dify API error: %s"), *ErrorMessage);
    OnVoiceError.Broadcast(FString::Printf(TEXT("Dify API error: %s"), *ErrorMessage));
}