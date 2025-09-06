#pragma once

struct COMMANDSYSTEM_API FCommandDescribe
{
public:
	FCommandDescribe()
	{
		
	}

	~FCommandDescribe()
	{
		
	}
	FName CommandTypeName;
	FString VoiceSourceFileFullPath;
	FString ExpressionType;
	FString AnimationType;
};