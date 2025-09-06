// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommandSystemDefine.h"
#include "Modules/ModuleManager.h"
#include "CommandSystem.generated.h"

class FCommandSystemModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};


DECLARE_DELEGATE_OneParam(FOnDataReceivedDelegate, const FString& CommandDesc);

class FNetworkServer : public FRunnable
{
public:
	FNetworkServer(int32 Port);
	virtual ~FNetworkServer();

	// FRunnable interface
	virtual uint32 Run() override;
	virtual void Stop() override;

	FOnDataReceivedDelegate OnDataReceived;

private:
	bool ParseDatagram(const TArray<uint8>& DataBuffer);

	FRunnableThread* Thread;
	FSocket* ListenSocket;
	bool bRunning;
	int32 ServerPort;
	TSharedPtr<FInternetAddr> SenderAddress;
};

//todo 使用GameAbilitySystem?

UCLASS()
class COMMANDSYSTEM_API UCommandBase : public UObject
{
	GENERATED_BODY()
public:
	UCommandBase();
	virtual void ProcessCommand(const FCommandDescribe& CommandDesc) {};

	//todo 
	virtual bool HandleJsonParam(TSharedPtr<FJsonObject> JsonObject) { return false; };
};

class FCommandBaseFactory
{
	friend class UCommandSystem;
public:
	static UCommandBase* CreateCommandProcessObject(UObject* Outer,FName CommandType);

	static void RegisterCommandProcessObject(FName ClassName,TSubclassOf<UCommandBase> CommandProcessClass);
private:
	static TMap<FName,TSubclassOf<UCommandBase>> CommandFactoryMap;
	static TMap<FName,TSubclassOf<UCommandBase>> NativeCommandFactoryMap;
};

UCLASS(Config=Game)
class COMMANDSYSTEM_API UCommandSystem : public UGameInstanceSubsystem,public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual bool IsTickable() const override {return true;};
	virtual TStatId GetStatId() const override {return TStatId();}
	virtual void Tick(float DeltaTime) override;

	void PushCommandByString(const FString& JsonParam);
protected:
	UPROPERTY(EditAnywhere,Config)
	TMap<FName,TSubclassOf<UCommandBase>> CommandFactoryMap;
private:

	//线程安全.
	void OnReceiveCommand_ThreadSafe(const FString& CommandDesc);
	
	//FStallingTaskQueue<FCommandDescribe,PLATFORM_CACHE_LINE_SIZE,1> PendingCommands;
	TQueue<FCommandDescribe, EQueueMode::Spsc> PendingCommands;
	
	FNetworkServer* NetworkServer = nullptr;
};
