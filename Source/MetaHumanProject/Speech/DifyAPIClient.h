#pragma once

#include "CoreMinimal.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "DifyAPIClient.generated.h"

// 确保委托定义在正确的命名空间中
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDifyResponseReceived, const FString&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDifyErrorReceived, const FString&, ErrorMessage);

/**
 * 客户端组件，用于与Dify API进行通信
 */
UCLASS(BlueprintType, Blueprintable)
class METAHUMANCPP_API UDifyAPIClient : public UObject
{
    GENERATED_BODY()

public:
    UDifyAPIClient();

    /**
     * 初始化Dify API客户端
     * @param BaseUrl API基础URL
     * @param ApiKey API密钥
     */
    UFUNCTION(BlueprintCallable, Category = "Dify API")
    void Initialize(const FString& BaseUrl, const FString& ApiKey);

    /**
     * 发送对话消息到Dify API（阻塞模式）
     * @param Query 用户输入/提问内容
     * @param ConversationId 会话ID，可选
     */
    UFUNCTION(BlueprintCallable, Category = "Dify API")
    void SendChatMessage(const FString& Query, const FString& ConversationId = "");

    /** 当收到Dify API响应时触发 */
    UPROPERTY(BlueprintAssignable, Category = "Dify API|Events")
    FOnDifyResponseReceived OnResponseReceived;

    /** 当发生错误时触发 */
    UPROPERTY(BlueprintAssignable, Category = "Dify API|Events")
    FOnDifyErrorReceived OnErrorReceived;

    /** 获取最近的会话ID */
    UFUNCTION(BlueprintPure, Category = "Dify API")
    FString GetCurrentConversationId() const { return CurrentConversationId; }

private:
    /** API基础URL */
    FString BaseUrl;
    
    /** API密钥 */
    FString ApiKey;
    
    /** 当前会话ID */
    FString CurrentConversationId;

    /** 处理HTTP响应 */
    void HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
}; 