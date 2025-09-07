#include "SpeechManager.h"
#include "SpeechConfig.h"

#include <string>

#include "Engine/Engine.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "Async/TaskGraphInterfaces.h"

USpeechManager::USpeechManager()
    : bIsSDKInitialized(false)
    , bIsRecognitionActive(false)
    , bIsSynthesisActive(false)
    , CurrentRecognitionSessionID("")
    , CurrentSynthesisSessionID("")
{
}

void USpeechManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Warning, TEXT("USpeechManager: Initializing speech system..."));
    
    // 使用默认配置初始化SDK
    if (!InitializeSpeech())
    {
        UE_LOG(LogTemp, Error, TEXT("USpeechManager: Failed to initialize speech SDK with default settings"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("USpeechManager: Speech SDK initialized successfully"));
    }
}

void USpeechManager::Deinitialize()
{
    // 停止所有语音操作
    if (bIsRecognitionActive)
    {
        StopSpeechRecognition();
    }

    // 清理SDK
    if (bIsSDKInitialized)
    {
        MSPLogout();
        bIsSDKInitialized = false;
    }

    Super::Deinitialize();
}

bool USpeechManager::InitializeSpeech(const FString& AppID, const FString& APIKey)
{
    if (bIsSDKInitialized)
    {
        UE_LOG(LogTemp, Log, TEXT("USpeechManager: SDK already initialized"));
        return true;
    }

    // 使用提供的参数或默认参数
    SDKAppID = AppID.IsEmpty() ? GetDefaultAppID() : AppID;
    SDKAPIKey = APIKey.IsEmpty() ? GetDefaultAPIKey() : APIKey;

    UE_LOG(LogTemp, Warning, TEXT("USpeechManager: Attempting to initialize with AppID: %s"), *SDKAppID);

    if (SDKAppID.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("USpeechManager: AppID is required for speech SDK initialization"));
        OnSpeechError.Broadcast(TEXT("AppID is required for speech SDK initialization"));
        return false;
    }

    // 构造登录参数
    FString LoginParams = FString::Printf(TEXT("appid = %s, work_dir = ."), *SDKAppID);

    // 转换为ANSI字符串
    std::string AppIDAnsi = TCHAR_TO_ANSI(*SDKAppID);
    std::string LoginParamsAnsi = TCHAR_TO_ANSI(*LoginParams);

    // 初始化MSC
    int ret = MSPLogin(nullptr, nullptr, LoginParamsAnsi.c_str());
    if (ret != MSP_SUCCESS)
    {
        FString ErrorMsg = FString::Printf(TEXT("MSPLogin failed with error code: %d"), ret);
        OnSpeechError.Broadcast(ErrorMsg);
        LogSpeechError(ret, TEXT("MSPLogin"));
        return false;
    }

    bIsSDKInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("Speech SDK initialized successfully"));
    return true;
}

bool USpeechManager::StartSpeechRecognition(const FString& Language)
{
    if (!bIsSDKInitialized)
    {
        OnSpeechError.Broadcast(TEXT("Speech SDK not initialized"));
        return false;
    }

    if (bIsRecognitionActive)
    {
        OnSpeechError.Broadcast(TEXT("Speech recognition already active"));
        return false;
    }

    FScopeLock Lock(&RecognitionCriticalSection);

    // 构造识别参数
    FString RecognitionParams = FString::Printf(
        TEXT("sub = iat, domain = iat, language = %s, accent = mandarin, sample_rate = 16000, result_type = plain, result_encoding = utf8"),
        *Language
    );

    std::string ParamsAnsi = TCHAR_TO_ANSI(*RecognitionParams);
    
    int ErrorCode = 0;
    const char* SessionID = QISRSessionBegin(nullptr, ParamsAnsi.c_str(), &ErrorCode);
    
    if (ErrorCode != MSP_SUCCESS || SessionID == nullptr)
    {
        FString ErrorMsg = FString::Printf(TEXT("QISRSessionBegin failed with error code: %d"), ErrorCode);
        OnSpeechError.Broadcast(ErrorMsg);
        LogSpeechError(ErrorCode, TEXT("QISRSessionBegin"));
        return false;
    }

    CurrentRecognitionSessionID = ANSI_TO_TCHAR(SessionID);

    // 注册回调
    std::string SessionIDAnsi = TCHAR_TO_ANSI(*CurrentRecognitionSessionID);
    QISRRegisterNotify(SessionIDAnsi.c_str(), OnRecognitionResult, OnRecognitionStatus, OnRecognitionError, this);

    bIsRecognitionActive = true;
    UE_LOG(LogTemp, Log, TEXT("Speech recognition started with session ID: %s"), *CurrentRecognitionSessionID);
    return true;
}

bool USpeechManager::StopSpeechRecognition()
{
    if (!bIsRecognitionActive)
    {
        return true;
    }

    FScopeLock Lock(&RecognitionCriticalSection);

    if (!CurrentRecognitionSessionID.IsEmpty())
    {
        std::string SessionIDAnsi = TCHAR_TO_ANSI(*CurrentRecognitionSessionID);
        int ret = QISRSessionEnd(SessionIDAnsi.c_str(), "Normal");
        if (ret != MSP_SUCCESS)
        {
            LogSpeechError(ret, TEXT("QISRSessionEnd"));
        }
        CurrentRecognitionSessionID.Empty();
    }

    bIsRecognitionActive = false;
    UE_LOG(LogTemp, Log, TEXT("Speech recognition stopped"));
    return true;
}

bool USpeechManager::WriteSpeechData(const TArray<uint8>& AudioData)
{
    if (!bIsRecognitionActive || CurrentRecognitionSessionID.IsEmpty())
    {
        static double LastLogTime = 0.0;
        double CurrentTime = FPlatformTime::Seconds();
        if (CurrentTime - LastLogTime > 2.0)
        {
            UE_LOG(LogTemp, Warning, TEXT("SpeechManager: WriteSpeechData called but recognition not active. bIsRecognitionActive=%s, SessionID=%s"), 
                   bIsRecognitionActive ? TEXT("true") : TEXT("false"), *CurrentRecognitionSessionID);
            LastLogTime = CurrentTime;
        }
        return false;
    }

    if (AudioData.Num() == 0)
    {
        return false;
    }

    FScopeLock Lock(&RecognitionCriticalSection);

    // 添加定期调试日志
    static double LastDataLogTime = 0.0;
    double CurrentTime = FPlatformTime::Seconds();
    if (CurrentTime - LastDataLogTime > 2.0)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpeechManager: Writing audio data - %d bytes to session %s"), 
               AudioData.Num(), *CurrentRecognitionSessionID);
        LastDataLogTime = CurrentTime;
    }

    std::string SessionIDAnsi = TCHAR_TO_ANSI(*CurrentRecognitionSessionID);
    int EpStatus = MSP_EP_LOOKING_FOR_SPEECH;
    int RecStatus = MSP_REC_STATUS_SUCCESS;

    int ret = QISRAudioWrite(
        SessionIDAnsi.c_str(),
        AudioData.GetData(),
        AudioData.Num(),
        MSP_AUDIO_SAMPLE_CONTINUE,
        &EpStatus,
        &RecStatus
    );

    if (ret != MSP_SUCCESS)
    {
        LogSpeechError(ret, TEXT("QISRAudioWrite"));
        
        // 对于10008错误（服务器响应错误），停止当前会话
        if (ret == 10008)
        {
            UE_LOG(LogTemp, Warning, TEXT("Stopping recognition session due to server error 10008"));
            StopSpeechRecognition();
        }
        
        return false;
    }

    // 记录端点检测状态
    if (CurrentTime - LastDataLogTime > 2.0)
    {
        UE_LOG(LogTemp, Log, TEXT("SpeechManager: QISRAudioWrite success, EpStatus=%d, RecStatus=%d"), EpStatus, RecStatus);
    }

    return true;
}

bool USpeechManager::SynthesizeText(const FString& Text, const FString& Voice)
{
    if (!bIsSDKInitialized)
    {
        OnSpeechError.Broadcast(TEXT("Speech SDK not initialized"));
        return false;
    }

    if (Text.IsEmpty())
    {
        OnSpeechError.Broadcast(TEXT("Text is empty"));
        return false;
    }

    FScopeLock Lock(&SynthesisCriticalSection);

    // 构造合成参数（参考SDK示例的参数设置）
    FString SynthesisParams = FString::Printf(
        TEXT("voice_name = %s, text_encoding = utf8, sample_rate = 16000, speed = 50, volume = 50, pitch = 50, rdn = 2"),
        *Voice
    );

    std::string ParamsAnsi = TCHAR_TO_UTF8(*SynthesisParams);
    
    int ErrorCode = 0;
    const char* SessionID = QTTSSessionBegin(ParamsAnsi.c_str(), &ErrorCode);
    
    if (ErrorCode != MSP_SUCCESS || SessionID == nullptr)
    {
        FString ErrorMsg = FString::Printf(TEXT("QTTSSessionBegin failed with error code: %d"), ErrorCode);
        OnSpeechError.Broadcast(ErrorMsg);
        LogSpeechError(ErrorCode, TEXT("QTTSSessionBegin"));
        return false;
    }

    CurrentSynthesisSessionID = ANSI_TO_TCHAR(SessionID);

    // 提交文本
    std::string TextUTF8 = TCHAR_TO_UTF8(*Text);
    int ret = QTTSTextPut(SessionID, TextUTF8.c_str(), TextUTF8.length(), nullptr);
    if (ret != MSP_SUCCESS)
    {
        FString ErrorMsg = FString::Printf(TEXT("QTTSTextPut failed with error code: %d"), ret);
        OnSpeechError.Broadcast(ErrorMsg);
        LogSpeechError(ret, TEXT("QTTSTextPut"));
        
        QTTSSessionEnd(SessionID, "TextPutError");
        CurrentSynthesisSessionID.Empty();
        return false;
    }

    bIsSynthesisActive = true;
    SynthesizedAudioBuffer.Reset();

    // 复制SessionID字符串以便在lambda中使用
    std::string SessionIDCopy = SessionID;

    // 开始获取音频数据（参考SDK示例的循环逻辑）
    AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, SessionIDCopy, Text]()
    {
        TArray<uint8> CompleteAudioData;
        FWavePCMHeader WavHeader;
        // 先添加WAV文件头（暂时数据大小为0）
        CompleteAudioData.Append((const uint8*)(&WavHeader), sizeof(WavHeader));
        
        unsigned int AudioLen = 0;
        int SynthStatus = MSP_TTS_FLAG_STILL_HAVE_DATA;
        int ErrorCode = 0;
        
        UE_LOG(LogTemp, Log, TEXT("SpeechManager: Starting TTS synthesis for text: %s"), *Text);

        // 参考SDK示例的音频获取循环
        while (SynthStatus == MSP_TTS_FLAG_STILL_HAVE_DATA && bIsSynthesisActive)
        {
            const void* AudioData = QTTSAudioGet(SessionIDCopy.c_str(), &AudioLen, &SynthStatus, &ErrorCode);
            
            if (ErrorCode != MSP_SUCCESS)
            {
                AsyncTask(ENamedThreads::GameThread, [this, ErrorCode]()
                {
                    LogSpeechError(ErrorCode, TEXT("QTTSAudioGet"));
                });
                break;
            }

            if (AudioData && AudioLen > 0)
            {
                // 添加PCM音频数据到完整缓冲区
                const uint8* AudioBytes = static_cast<const uint8*>(AudioData);
                CompleteAudioData.Append(AudioBytes, AudioLen);
                WavHeader.data_size += AudioLen;
                
                UE_LOG(LogTemp, VeryVerbose, TEXT("SpeechManager: Got audio chunk: %d bytes, status: %d"), AudioLen, SynthStatus);
            }
            
            if (SynthStatus == MSP_TTS_FLAG_DATA_END)
            {
                UE_LOG(LogTemp, Log, TEXT("SpeechManager: TTS synthesis completed, total audio data: %d bytes"), WavHeader.data_size);
                break;
            }
            
            // 参考SDK示例的延迟，防止频繁占用CPU
            FPlatformProcess::Sleep(0.05f); // 50ms
        }

        // 完成WAV文件头设置（参考SDK示例）
        WavHeader.size_8 = WavHeader.data_size + (sizeof(WavHeader) - 8);
        
        // 更新WAV文件头中的大小信息
        FMemory::Memcpy(CompleteAudioData.GetData() + 4, &WavHeader.size_8, sizeof(WavHeader.size_8));
        FMemory::Memcpy(CompleteAudioData.GetData() + 40, &WavHeader.data_size, sizeof(WavHeader.data_size));

        // 合成完成，回到主线程触发事件
        AsyncTask(ENamedThreads::GameThread, [this, SessionIDCopy, CompleteAudioData, WavHeader]()
        {
            if (CompleteAudioData.Num() > sizeof(FWavePCMHeader))
            {
                UE_LOG(LogTemp, Log, TEXT("SpeechManager: TTS synthesis successful - Total size: %d bytes (PCM data: %d bytes)"), 
                       CompleteAudioData.Num(), WavHeader.data_size);
                OnSpeechSynthesized.Broadcast(CompleteAudioData);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("SpeechManager: TTS synthesis produced no audio data"));
                OnSpeechError.Broadcast(TEXT("No audio data generated"));
            }

            QTTSSessionEnd(SessionIDCopy.c_str(), "Normal");
            CurrentSynthesisSessionID.Empty();
            bIsSynthesisActive = false;
        });
    });

    UE_LOG(LogTemp, Log, TEXT("SpeechManager: Text synthesis started: %s"), *Text);
    return true;
}

// 静态回调函数实现
void USpeechManager::OnRecognitionResult(const char* sessionID, const char* result, int resultLen, int resultStatus, void* userData)
{
    USpeechManager* Manager = static_cast<USpeechManager*>(userData);
    if (Manager && result && resultLen > 0)
    {
        // 科大讯飞SDK返回UTF-8编码的文本，需要正确转换
        FString ResultText = UTF8_TO_TCHAR(result);
        
        UE_LOG(LogTemp, Log, TEXT("Recognition raw result: %s (len=%d)"), *ResultText, resultLen);
        
        AsyncTask(ENamedThreads::GameThread, [Manager, ResultText]()
        {
            Manager->OnSpeechRecognized.Broadcast(ResultText);
        });
    }
}

void USpeechManager::OnRecognitionStatus(const char* sessionID, int type, int status, int param1, const void* param2, void* userData)
{
    USpeechManager* Manager = static_cast<USpeechManager*>(userData);
    if (Manager)
    {
        UE_LOG(LogTemp, Verbose, TEXT("Recognition status: type=%d, status=%d"), type, status);
    }
}

void USpeechManager::OnRecognitionError(const char* sessionID, int errorCode, const char* detail, void* userData)
{
    USpeechManager* Manager = static_cast<USpeechManager*>(userData);
    if (Manager)
    {
        FString ErrorDetail = detail ? ANSI_TO_TCHAR(detail) : TEXT("Unknown error");
        
        AsyncTask(ENamedThreads::GameThread, [Manager, errorCode, ErrorDetail]()
        {
            FString ErrorMsg = FString::Printf(TEXT("Recognition error %d: %s"), errorCode, *ErrorDetail);
            Manager->OnSpeechError.Broadcast(ErrorMsg);
        });
    }
}

void USpeechManager::OnSynthesisResult(const char* sessionID, const char* audio, int audioLen, int synthStatus, int ced, const char* audioInfo, int audioInfoLen, void* userData)
{
    // 音频数据在主循环中处理
}

void USpeechManager::OnSynthesisStatus(const char* sessionID, int type, int status, int param1, const void* param2, void* userData)
{
    USpeechManager* Manager = static_cast<USpeechManager*>(userData);
    if (Manager)
    {
        UE_LOG(LogTemp, Verbose, TEXT("Synthesis status: type=%d, status=%d"), type, status);
    }
}

void USpeechManager::OnSynthesisError(const char* sessionID, int errorCode, const char* detail, void* userData)
{
    USpeechManager* Manager = static_cast<USpeechManager*>(userData);
    if (Manager)
    {
        FString ErrorDetail = detail ? ANSI_TO_TCHAR(detail) : TEXT("Unknown error");
        
        AsyncTask(ENamedThreads::GameThread, [Manager, errorCode, ErrorDetail]()
        {
            FString ErrorMsg = FString::Printf(TEXT("Synthesis error %d: %s"), errorCode, *ErrorDetail);
            Manager->OnSpeechError.Broadcast(ErrorMsg);
        });
    }
}

FString USpeechManager::GetDefaultAppID() const
{
    // 优先从新的配置系统读取
    if (USpeechSystemSettings* Settings = USpeechSystemSettings::Get())
    {
        if (!Settings->SpeechConfig.AppID.IsEmpty() && Settings->SpeechConfig.AppID != TEXT("your_app_id_here"))
        {
            return Settings->SpeechConfig.AppID;
        }
    }
    
    // 从配置文件读取AppID（向后兼容）
    FString AppID;
    if (GConfig->GetString(TEXT("/Script/MetahumanProject.SpeechManager"), TEXT("DefaultAppID"), AppID, GGameIni))
    {
        if (AppID != TEXT("your_app_id_here") && !AppID.IsEmpty())
        {
            return AppID;
        }
    }
    
    // 最后尝试从SDK目录的配置文件读取（如果存在）
    FString SDKConfigPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/MetahumanProject/ThirdParty/iFlytek/config.ini"));
    if (FPaths::FileExists(SDKConfigPath))
    {
        FString ConfigContent;
        if (FFileHelper::LoadFileToString(ConfigContent, *SDKConfigPath))
        {
            // 简单解析配置文件中的AppID
            TArray<FString> Lines;
            ConfigContent.ParseIntoArrayLines(Lines);
            for (const FString& Line : Lines)
            {
                if (Line.StartsWith(TEXT("appid=")))
                {
                    FString ConfigAppID = Line.RightChop(6).TrimStartAndEnd();
                    if (!ConfigAppID.IsEmpty())
                    {
                        return ConfigAppID;
                    }
                }
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("No valid AppID found. Please configure in DefaultSpeech.ini or set IFLYTEK_APPID environment variable"));
    return TEXT("");
}

FString USpeechManager::GetDefaultAPIKey() const
{
    // 从配置文件读取APIKey
    FString APIKey;
    if (GConfig->GetString(TEXT("/Script/MetahumanProject.SpeechManager"), TEXT("DefaultAPIKey"), APIKey, GGameIni))
    {
        if (APIKey != TEXT("your_api_key_here") && !APIKey.IsEmpty())
        {
            return APIKey;
        }
    }
    return TEXT("");
}

void USpeechManager::LogSpeechError(int ErrorCode, const FString& Context)
{
    FString ErrorMessage;
    
    switch (ErrorCode)
    {
        case MSP_ERROR_NO_LICENSE:
            ErrorMessage = TEXT("No license");
            break;
        case MSP_ERROR_INVALID_PARA:
            ErrorMessage = TEXT("Invalid parameter");
            break;
        case MSP_ERROR_NOT_INIT:
            ErrorMessage = TEXT("SDK not initialized");
            break;
        case MSP_ERROR_TIME_OUT:
            ErrorMessage = TEXT("Timeout");
            break;
        case MSP_ERROR_NET_GENERAL:
            ErrorMessage = TEXT("Network error");
            break;
        case 10008:
            ErrorMessage = TEXT("Bad response from server - Check AppID/network/quota");
            break;
        case 10013:
            ErrorMessage = TEXT("Insufficient privileges - Check AppID permissions");
            break;
        case 10019:
            ErrorMessage = TEXT("No quota - AppID has no remaining quota");
            break;
        case 10022:
            ErrorMessage = TEXT("Invalid audio format");
            break;
        default:
            ErrorMessage = FString::Printf(TEXT("Error code: %d"), ErrorCode);
            break;
    }
    
    UE_LOG(LogTemp, Error, TEXT("%s: %s"), *Context, *ErrorMessage);
}