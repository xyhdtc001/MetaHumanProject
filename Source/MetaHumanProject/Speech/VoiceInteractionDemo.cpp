#include "VoiceInteractionDemo.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"

// 添加Dify API相关方法实现

// 初始化Dify API
void AVoiceInteractionDemo::InitializeDifyAPI(const FString& BaseUrl, const FString& ApiKey)
{
    DifyBaseUrl = BaseUrl;
    DifyApiKey = ApiKey;
    bUseDifyForResponses = true;
    
    if (VoiceInteractionComponent)
    {
        VoiceInteractionComponent->InitializeDifyAPI(BaseUrl, ApiKey);
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionDemo: Dify API initialized with URL: %s"), *BaseUrl);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionDemo: Cannot initialize Dify API - VoiceInteractionComponent is null"));
    }
}

// 发送文本到Dify API
void AVoiceInteractionDemo::SendToDifyAPI(const FString& Query)
{
    if (VoiceInteractionComponent)
    {
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionDemo: Sending query to Dify API: %s"), *Query);
        VoiceInteractionComponent->SendToDifyAPI(Query);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionDemo: Cannot send to Dify API - VoiceInteractionComponent is null"));
    }
}

// Dify响应回调
void AVoiceInteractionDemo::OnDifyResponseReceived(const FString& Response)
{
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionDemo: Received response from Dify API: %s"), *Response);
    
    // 标记不再等待响应
    bIsWaitingForDifyResponse = false;
    
    // 如果有待处理的响应，则说出来
    if (!Response.IsEmpty())
    {
        // 如果是阻塞式模式，直接说出响应
        if (bWaitForDifyResponse)
        {
            SayText(Response);
        }
        // 否则，只在当前没有语音合成时说出响应
        else if (VoiceInteractionComponent && !VoiceInteractionComponent->IsSpeaking())
        {
            SayText(Response);
        }
    }
    else if (!PendingResponse.IsEmpty())
    {
        // 如果响应为空但有待处理的响应，则说出待处理的响应
        SayText(PendingResponse);
        PendingResponse.Empty();
    }
}

// Dify错误回调
void AVoiceInteractionDemo::OnDifyErrorReceived(const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("VoiceInteractionDemo: Dify API error: %s"), *ErrorMessage);
    
    // 标记不再等待响应
    bIsWaitingForDifyResponse = false;
    
    // 在屏幕上显示错误信息
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, 
            FString::Printf(TEXT("Dify API错误: %s"), *ErrorMessage));
    }
    
    // 如果有待处理的响应，则说出来
    if (!PendingResponse.IsEmpty())
    {
        SayText(PendingResponse);
        PendingResponse.Empty();
    }
    else
    {
        // 说出一个通用错误消息
        SayText(TEXT("抱歉，我暂时无法回答您的问题，请稍后再试。"));
    }
}

AVoiceInteractionDemo::AVoiceInteractionDemo()
{
    PrimaryActorTick.bCanEverTick = false;

    // 创建根组件
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
    RootComponent = RootSceneComponent;

    // 创建语音交互组件
    VoiceInteractionComponent = CreateDefaultSubobject<UVoiceInteractionComponent>(TEXT("VoiceInteractionComponent"));

    // 初始化状态
    bIsDemoActive = false;
    ConversationStep = 0;

    // 设置示例问题
    ExampleQuestions.Add(TEXT("你好"));
    ExampleQuestions.Add(TEXT("今天天气怎么样"));
    ExampleQuestions.Add(TEXT("现在几点了"));
    ExampleQuestions.Add(TEXT("你叫什么名字"));
    ExampleQuestions.Add(TEXT("你能做什么"));
    ExampleQuestions.Add(TEXT("再见"));
}

void AVoiceInteractionDemo::BeginPlay()
{
    Super::BeginPlay();

    if (VoiceInteractionComponent)
    {
        // 绑定语音事件
        VoiceInteractionComponent->OnRecognitionResult.AddDynamic(this, &AVoiceInteractionDemo::OnVoiceRecognized);
        VoiceInteractionComponent->OnSynthesisComplete.AddDynamic(this, &AVoiceInteractionDemo::OnVoiceSynthesized);
        VoiceInteractionComponent->OnVoiceError.AddDynamic(this, &AVoiceInteractionDemo::OnVoiceError);
        
        // 绑定Dify API事件
        VoiceInteractionComponent->OnDifyResponseReceived.AddDynamic(this, &AVoiceInteractionDemo::OnDifyResponseReceived);
        VoiceInteractionComponent->OnDifyErrorReceived.AddDynamic(this, &AVoiceInteractionDemo::OnDifyErrorReceived);

        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionDemo: Voice events bound successfully"));
        
        // 初始化Dify API（如果配置了API Key）
        if (bUseDifyForResponses && !DifyApiKey.IsEmpty())
        {
            VoiceInteractionComponent->InitializeDifyAPI(DifyBaseUrl, DifyApiKey);
            VoiceInteractionComponent->bUseDifyForResponses = bUseDifyForResponses;
            UE_LOG(LogTemp, Log, TEXT("VoiceInteractionDemo: Dify API initialized with URL: %s"), *DifyBaseUrl);
        }

        // 自动开始演示
        if (bAutoStartDemo)
        {
            // 延迟2秒启动，确保系统完全初始化
            FTimerHandle TimerHandle;
            GetWorldTimerManager().SetTimer(TimerHandle, this, &AVoiceInteractionDemo::StartVoiceDemo, 2.0f, false);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionDemo: VoiceInteractionComponent is null"));
    }
}

void AVoiceInteractionDemo::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AVoiceInteractionDemo::StartVoiceDemo()
{
    if (bIsDemoActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("VoiceInteractionDemo: Demo already active"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionDemo: Starting voice demo"));

    bIsDemoActive = true;
    ConversationStep = 0;

    // 说欢迎词
    if (!WelcomeMessage.IsEmpty())
    {
        SayText(WelcomeMessage);
    }

    // 延迟3秒后开始监听
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this]()
    {
        if (VoiceInteractionComponent)
        {
            VoiceInteractionComponent->StartListening();
        }
    }, 3.0f, false);
}

void AVoiceInteractionDemo::StopVoiceDemo()
{
    if (!bIsDemoActive)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionDemo: Stopping voice demo"));

    bIsDemoActive = false;

    if (VoiceInteractionComponent)
    {
        VoiceInteractionComponent->StopListening();
    }

    SayText(TEXT("语音演示结束，谢谢使用！"));
}

void AVoiceInteractionDemo::SayHello()
{
    SayText(TEXT("你好，我是虚幻引擎中的数字人！"));
}

void AVoiceInteractionDemo::SayText(const FString& Text)
{
    if (VoiceInteractionComponent)
    {
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionDemo: Speaking text: %s"), *Text);
        VoiceInteractionComponent->SpeakText(Text);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("VoiceInteractionDemo: Cannot speak - VoiceInteractionComponent is null"));
    }
}

void AVoiceInteractionDemo::StartConversation()
{
    if (!bIsDemoActive)
    {
        StartVoiceDemo();
        return;
    }

    ConversationStep = 1;
    SayText(TEXT("我们来聊天吧！你可以问我问题，比如：你好、现在几点了、你叫什么名字等。"));

    // 2秒后开始监听
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this]()
    {
        if (VoiceInteractionComponent)
        {
            VoiceInteractionComponent->StartListening();
        }
    }, 2.0f, false);
}

void AVoiceInteractionDemo::OnVoiceRecognized(const FString& RecognizedText)
{
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionDemo: Recognized: %s"), *RecognizedText);

    // 在屏幕上显示识别结果
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, 
            FString::Printf(TEXT("识别结果: %s"), *RecognizedText));
    }

    // 处理识别到的文本
    ProcessRecognizedText(RecognizedText);
}

void AVoiceInteractionDemo::OnVoiceSynthesized(USoundWave* GeneratedAudio)
{
    UE_LOG(LogTemp, Log, TEXT("VoiceInteractionDemo: Speech synthesis completed"));

    // 在屏幕上显示合成完成信息
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Blue, TEXT("语音合成完成"));
    }

    // 只在非连续识别模式下重新开始监听
    // 连续识别模式会自动处理监听重启
    if (bIsDemoActive && VoiceInteractionComponent && !VoiceInteractionComponent->bContinuousRecognitionMode)
    {
        FTimerHandle TimerHandle;
        GetWorldTimerManager().SetTimer(TimerHandle, [this]()
        {
            if (VoiceInteractionComponent && bIsDemoActive)
            {
                VoiceInteractionComponent->StartListening();
            }
        }, 1.0f, false);
    }
}

void AVoiceInteractionDemo::OnVoiceError(const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("VoiceInteractionDemo: Voice error: %s"), *ErrorMessage);

    // 在屏幕上显示错误信息
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, 
            FString::Printf(TEXT("语音错误: %s"), *ErrorMessage));
    }
}

void AVoiceInteractionDemo::ProcessRecognizedText(const FString& Text)
{
    if (!bIsDemoActive)
    {
        return;
    }

    // 检查是否为连续识别模式 - 如果是，则不停止监听
    if (VoiceInteractionComponent && !VoiceInteractionComponent->bContinuousRecognitionMode)
    {
        // 非连续模式：停止当前监听
        VoiceInteractionComponent->StopListening();
    }
    // 连续模式：不停止监听，让连续模式自动处理

    // // 生成回复
    // FString Response = GenerateResponse(Text);
    //
    // // 说出回复
    // if (!Response.IsEmpty())
    // {
    //     SayText(Response);
    // }
}

FString AVoiceInteractionDemo::GenerateResponse(const FString& Input)
{
    // 如果启用了Dify API，使用它来生成响应
    if (bUseDifyForResponses && VoiceInteractionComponent && !Input.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("VoiceInteractionDemo: Using Dify API for response generation"));
        
        if (bWaitForDifyResponse)
        {
            // 阻塞式调用模式
            // 保存一个临时响应，以防Dify API调用失败
            PendingResponse = TEXT("正在处理您的请求...");
            
            // 发送到Dify API
            VoiceInteractionComponent->SendToDifyAPI(Input);
            
            // 返回空字符串，实际响应将通过OnDifyResponseReceived回调处理
            return TEXT("");
        }
        else
        {
            // 非阻塞式调用模式
            // 发送到Dify API
            VoiceInteractionComponent->SendToDifyAPI(Input);
            
            // 返回一个临时响应
            return TEXT("我正在思考您的问题，请稍等...");
        }
    }
    
    // 如果未启用Dify API或发生错误，使用默认的关键词匹配逻辑
    FString LowerInput = Input.ToLower();

    // 简单的关键词匹配回复
    if (LowerInput.Contains(TEXT("你好")) || LowerInput.Contains(TEXT("hello")))
    {
        return TEXT("你好！很高兴见到你，我是数字人助手。");
    }
    else if (LowerInput.Contains(TEXT("名字")) || LowerInput.Contains(TEXT("叫什么")))
    {
        return TEXT("我是基于虚幻引擎和科大讯飞语音技术开发的数字人助手。");
    }
    else if (LowerInput.Contains(TEXT("时间")) || LowerInput.Contains(TEXT("几点")))
    {
        FDateTime Now = FDateTime::Now();
        return FString::Printf(TEXT("现在时间是%d年%d月%d日，%d点%d分。"), 
            Now.GetYear(), Now.GetMonth(), Now.GetDay(), Now.GetHour(), Now.GetMinute());
    }
    else if (LowerInput.Contains(TEXT("天气")))
    {
        return TEXT("抱歉，我还没有接入天气数据，无法告诉您天气信息。");
    }
    else if (LowerInput.Contains(TEXT("你能做什么")) || LowerInput.Contains(TEXT("功能")))
    {
        return TEXT("我可以进行语音识别和语音合成，与您进行简单的对话。我还在不断学习中！");
    }
    else if (LowerInput.Contains(TEXT("再见")) || LowerInput.Contains(TEXT("拜拜")) || LowerInput.Contains(TEXT("goodbye")))
    {
        // 结束对话
        FTimerHandle TimerHandle;
        GetWorldTimerManager().SetTimer(TimerHandle, this, &AVoiceInteractionDemo::StopVoiceDemo, 2.0f, false);
        return TEXT("再见！期待下次与您交流！");
    }
    else if (LowerInput.Contains(TEXT("测试")))
    {
        return TEXT("语音系统运行正常！识别和合成功能都在正常工作。");
    }
    else
    {
        // 默认回复
        TArray<FString> DefaultResponses;
        DefaultResponses.Add(TEXT("我听到了您说的话，但是还不太理解您的意思。"));
        DefaultResponses.Add(TEXT("这是一个有趣的问题，让我想想如何回答您。"));
        DefaultResponses.Add(TEXT("您可以试试问我：你好、现在几点了、你叫什么名字等问题。"));
        DefaultResponses.Add(TEXT("我正在学习理解更多的语言内容，谢谢您的耐心。"));
        
        int32 RandomIndex = FMath::RandRange(0, DefaultResponses.Num() - 1);
        return DefaultResponses[RandomIndex];
    }
}