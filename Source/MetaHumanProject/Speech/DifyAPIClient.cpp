#include "DifyAPIClient.h"
#include "HttpModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"

UDifyAPIClient::UDifyAPIClient()
    : BaseUrl(TEXT("http://localhost/v1"))
    , ApiKey(TEXT("app-exEByu6vZWflAIX3zKxkeew8"))
    , CurrentConversationId("")
{
}

void UDifyAPIClient::Initialize(const FString& InBaseUrl, const FString& InApiKey)
{
    BaseUrl = InBaseUrl;
    ApiKey = InApiKey;
    
    UE_LOG(LogTemp, Log, TEXT("DifyAPIClient: Initialized with BaseUrl: %s"), *BaseUrl);
}

void UDifyAPIClient::SendChatMessage(const FString& Query, const FString& ConversationId)
{
    if (Query.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("DifyAPIClient: Query is empty"));
        OnErrorReceived.Broadcast("Query cannot be empty");
        return;
    }

    if (ApiKey.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("DifyAPIClient: API Key not set. Call Initialize first."));
        OnErrorReceived.Broadcast("API Key not set. Call Initialize first.");
        return;
    }

    // 创建HTTP请求
    FHttpModule* Http = &FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    
    // 设置请求URL
    FString Url = FString::Printf(TEXT("%s/chat-messages"), *BaseUrl);
    Request->SetURL(Url);
    
    // 设置请求方法
    Request->SetVerb("POST");
    
    // 设置请求头
    Request->SetHeader("Content-Type", "application/json");
    Request->SetHeader("Authorization", FString::Printf(TEXT("Bearer %s"), *ApiKey));
    
    // 构建请求体
    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("query"), Query);
    JsonObject->SetObjectField(TEXT("inputs"), MakeShared<FJsonObject>());
    JsonObject->SetStringField(TEXT("response_mode"), TEXT("blocking")); // 使用阻塞模式
    JsonObject->SetStringField(TEXT("user"), TEXT("metahuman_user"));
    
    // 如果提供了会话ID，则添加到请求中
    if (!ConversationId.IsEmpty())
    {
        JsonObject->SetStringField(TEXT("conversation_id"), ConversationId);
    }
    
    // 序列化JSON
    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    
    // 设置请求体
    Request->SetContentAsString(RequestBody);
    
    // 设置回调
    Request->OnProcessRequestComplete().BindUObject(this, &UDifyAPIClient::HandleResponse);
    
    // 发送请求
    UE_LOG(LogTemp, Log, TEXT("DifyAPIClient: Sending request to %s with body: %s"), *Url, *RequestBody);
    Request->ProcessRequest();
}

void UDifyAPIClient::HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("DifyAPIClient: Request failed"));
        OnErrorReceived.Broadcast("Request failed");
        return;
    }
    
    int32 ResponseCode = Response->GetResponseCode();
    FString ResponseBody = Response->GetContentAsString();
    
    UE_LOG(LogTemp, Log, TEXT("DifyAPIClient: Response received with code %d and body: %s"), ResponseCode, *ResponseBody);
    
    if (ResponseCode < 200 || ResponseCode >= 300)
    {
        FString ErrorMessage = FString::Printf(TEXT("HTTP Error: %d - %s"), ResponseCode, *ResponseBody);
        UE_LOG(LogTemp, Error, TEXT("DifyAPIClient: %s"), *ErrorMessage);
        OnErrorReceived.Broadcast(ErrorMessage);
        return;
    }
    
    // 解析JSON响应
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    
    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        // 获取会话ID
        if (JsonObject->HasField(TEXT("conversation_id")))
        {
            CurrentConversationId = JsonObject->GetStringField(TEXT("conversation_id"));
            UE_LOG(LogTemp, Log, TEXT("DifyAPIClient: Conversation ID: %s"), *CurrentConversationId);
        }
        
        // 获取回答
        if (JsonObject->HasField(TEXT("answer")))
        {
            FString Answer = JsonObject->GetStringField(TEXT("answer"));
            UE_LOG(LogTemp, Log, TEXT("DifyAPIClient: Answer: %s"), *Answer);
            OnResponseReceived.Broadcast(Answer);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("DifyAPIClient: No answer field in response"));
            OnErrorReceived.Broadcast("No answer field in response");
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("DifyAPIClient: Failed to parse JSON response"));
        OnErrorReceived.Broadcast("Failed to parse JSON response");
    }
} 