#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoiceInteractionComponent.h"
#include "VoiceInteractionDemo.generated.h"

/**
 * 语音交互演示Actor
 * 展示如何使用语音交互功能
 */
UCLASS(BlueprintType, Blueprintable)
class METAHUMANCPP_API AVoiceInteractionDemo : public AActor
{
    GENERATED_BODY()
    
public:    
    AVoiceInteractionDemo();

protected:
    virtual void BeginPlay() override;

public:    
    virtual void Tick(float DeltaTime) override;

    // 组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UVoiceInteractionComponent> VoiceInteractionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> RootSceneComponent;

    // 演示功能
    UFUNCTION(BlueprintCallable, Category = "Voice Demo")
    void StartVoiceDemo();

    UFUNCTION(BlueprintCallable, Category = "Voice Demo")
    void StopVoiceDemo();

    UFUNCTION(BlueprintCallable, Category = "Voice Demo")
    void SayHello();

    UFUNCTION(BlueprintCallable, Category = "Voice Demo")
    void SayText(const FString& Text);

    // 测试对话
    UFUNCTION(BlueprintCallable, Category = "Voice Demo")
    void StartConversation();
    
    // Dify API相关
    UFUNCTION(BlueprintCallable, Category = "Voice Demo|Dify")
    void InitializeDifyAPI(const FString& BaseUrl, const FString& ApiKey);
    
    UFUNCTION(BlueprintCallable, Category = "Voice Demo|Dify")
    void SendToDifyAPI(const FString& Query);

protected:
    // 事件处理
    UFUNCTION()
    void OnVoiceRecognized(const FString& RecognizedText);

    UFUNCTION()
    void OnVoiceSynthesized(USoundWave* GeneratedAudio);

    UFUNCTION()
    void OnVoiceError(const FString& ErrorMessage);
    
    // Dify API回调
    UFUNCTION()
    void OnDifyResponseReceived(const FString& Response);
    
    UFUNCTION()
    void OnDifyErrorReceived(const FString& ErrorMessage);

    // 简单的对话逻辑
    void ProcessRecognizedText(const FString& Text);
    FString GenerateResponse(const FString& Input);

    // 配置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Demo|Settings")
    bool bAutoStartDemo = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Demo|Settings")
    FString WelcomeMessage = TEXT("你好，我是数字人助手，请问有什么可以帮助您的吗？");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Demo|Settings")
    TArray<FString> ExampleQuestions;
    
    // Dify API设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Demo|Dify")
    bool bUseDifyForResponses = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Demo|Dify")
    FString DifyBaseUrl = TEXT("http://localhost/v1");
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Demo|Dify")
    FString DifyApiKey = TEXT("app-exEByu6vZWflAIX3zKxkeew8");
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Demo|Dify")
    bool bWaitForDifyResponse = true;
    
    // Dify API状态
    FString PendingResponse;
    bool bIsWaitingForDifyResponse = false;

private:
    // 内部状态
    bool bIsDemoActive;
    int32 ConversationStep;
};