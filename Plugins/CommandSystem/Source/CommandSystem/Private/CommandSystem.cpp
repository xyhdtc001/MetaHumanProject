// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommandSystem.h"
#include "Common/UdpSocketBuilder.h"


TMap<FName,TSubclassOf<UCommandBase>> FCommandBaseFactory::CommandFactoryMap;
TMap<FName,TSubclassOf<UCommandBase>> FCommandBaseFactory::NativeCommandFactoryMap;


#define LOCTEXT_NAMESPACE "FCommandSystemModule"

void FCommandSystemModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FCommandSystemModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

FNetworkServer::FNetworkServer(int32 Port)
    : bRunning(true),ServerPort(Port)
{
    ListenSocket = FUdpSocketBuilder(TEXT("JSONUDPServer"))
        .AsReusable()
        .WithBroadcast()
        .BoundToPort(ServerPort)
        .Build();

    SenderAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    Thread = FRunnableThread::Create(this, TEXT("NetworkServerThread"));
}

FNetworkServer::~FNetworkServer()
{
    Stop();
    if (Thread) Thread->Kill(true);
}

uint32 FNetworkServer::Run()
{
    const int32 BufferSize = 1024;
    TArray<uint8> ReceiveBuffer;
    ReceiveBuffer.SetNumUninitialized(BufferSize);

    while (bRunning)
    {
        if (ListenSocket && ListenSocket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(100)))
        {
            int32 BytesRead = 0;
            ListenSocket->RecvFrom(ReceiveBuffer.GetData(), BufferSize, BytesRead, *SenderAddress);

            if (BytesRead > 0)
            {
                // 截取有效
                TArray<uint8> ValidData(ReceiveBuffer.GetData(), BytesRead);
                ParseDatagram(ValidData);
            }
        }
    }
    return 0;
}

bool FNetworkServer::ParseDatagram(const TArray<uint8>& DataBuffer)
{
    // 转换为字符串
    FString JsonString = FString((reinterpret_cast<const char*>(DataBuffer.GetData())));
    if (JsonString.Len() > 0)
    {
        OnDataReceived.ExecuteIfBound(JsonString);
    }
    return true;
}

void FNetworkServer::Stop()
{
    bRunning = false;
    if (ListenSocket)
    {
        ListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
        ListenSocket= nullptr;
    }
}

UCommandBase::UCommandBase()
{
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        if (GetClass()->IsNative())
        {
            FCommandBaseFactory::RegisterCommandProcessObject(GetClass()->GetFName(),GetClass());
        }
    }
}

UCommandBase* FCommandBaseFactory::CreateCommandProcessObject(UObject* Outer,FName CommandType)
{
    if (CommandFactoryMap.Contains(CommandType))
    {
        return NewObject<UCommandBase>(Outer,CommandFactoryMap[CommandType]);
    }
    return nullptr;
}

void FCommandBaseFactory::RegisterCommandProcessObject(FName ClassName, TSubclassOf<UCommandBase> CommandProcessClass)
{
    if (!CommandProcessClass)
    {
        return;
    }
    if (!CommandProcessClass->IsNative())
    {
        return;
    }
    NativeCommandFactoryMap.Emplace(ClassName,CommandProcessClass);
}

void UCommandSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    UGameInstanceSubsystem::Initialize(Collection);
    //初始化网络模块.
    NetworkServer = new FNetworkServer(7751);
    NetworkServer->OnDataReceived.BindUObject(this, &UCommandSystem::OnReceiveCommand_ThreadSafe);
    FCommandBaseFactory::CommandFactoryMap.Append(CommandFactoryMap);
    FCommandBaseFactory::CommandFactoryMap.Append(FCommandBaseFactory::NativeCommandFactoryMap);
}

void UCommandSystem::Deinitialize()
{
    UGameInstanceSubsystem::Deinitialize();
    FCommandBaseFactory::CommandFactoryMap.Reset();
    NetworkServer->Stop();
    delete NetworkServer;
}

void UCommandSystem::Tick(float DeltaTime)
{
    static  int MaxHandleCommandCount = 100;
    int CurrentHandleCommandCount = 0;
    while (CurrentHandleCommandCount < MaxHandleCommandCount)
    {
        FCommandDescribe CommandDescribe;
        if (PendingCommands.IsEmpty() || !PendingCommands.Dequeue(CommandDescribe))
        {
            break;
        }
        //Handle Command
        UCommandBase* CommandBase = FCommandBaseFactory::CreateCommandProcessObject(this,CommandDescribe.CommandTypeName);
        if (CommandBase)
        {
            CommandBase->ProcessCommand(CommandDescribe);
        }
        ++CurrentHandleCommandCount;
    }
}

void UCommandSystem::PushCommandByString(const FString& JsonParam)
{
    OnReceiveCommand_ThreadSafe(JsonParam);
}

void UCommandSystem::OnReceiveCommand_ThreadSafe(const FString& JsonString)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (FJsonSerializer::Deserialize(Reader, JsonObject))
    {
        
        FCommandDescribe CommandDescribe;
        FString CommandTypeName;
        //todo 反射封装
        if (JsonObject->TryGetStringField(TEXT("voice_file"), CommandDescribe.VoiceSourceFileFullPath) &&
            JsonObject->TryGetStringField(TEXT("expression_type"), CommandDescribe.ExpressionType) &&
            JsonObject->TryGetStringField(TEXT("animation_type"), CommandDescribe.AnimationType)&&
            JsonObject->TryGetStringField(TEXT("cmd_type"),CommandTypeName))
        {
            CommandDescribe.CommandTypeName = FName(CommandTypeName);
            PendingCommands.Enqueue(CommandDescribe);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid UDP datagram received"));
    }
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCommandSystemModule, CommandSystem)