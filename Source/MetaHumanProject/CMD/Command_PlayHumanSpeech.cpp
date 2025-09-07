#include "Command_PlayHumanSpeech.h"

#include "Kismet/GameplayStatics.h"
#include "MetaHumanProject/MetaHumanPlayerController.h"

void UCommad_PlayHumanSpeech::ProcessCommand(const FCommandDescribe& CommandDesc)
{
	Super::ProcessCommand(CommandDesc);

	//查看音频是否存在.
	TArray<uint8> WavBuffer;
	bool bLoadedFile = FFileHelper::LoadFileToArray(WavBuffer,*CommandDesc.VoiceSourceFileFullPath);
	if (!bLoadedFile)
	{
		return;
	}
	AMetaHumanPlayerController* MetaHumanPlayerController = Cast<AMetaHumanPlayerController>(UGameplayStatics::GetPlayerController(this,0));
	if (!MetaHumanPlayerController)
	{
		return;
	}
	
	MetaHumanPlayerController->PlayHumanSpeech(WavBuffer,CommandDesc.ExpressionType,CommandDesc.AnimationType);
	
}
