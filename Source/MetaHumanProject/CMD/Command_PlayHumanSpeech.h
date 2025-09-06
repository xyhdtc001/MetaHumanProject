#pragma once
#include "CoreMinimal.h"
#include "CommandSystem.h"
#include "Command_PlayHumanSpeech.generated.h"

UCLASS()
class UCommad_PlayHumanSpeech : public UCommandBase
{
	GENERATED_BODY()
public:

	virtual void ProcessCommand(const FCommandDescribe& CommandDesc) override;
};