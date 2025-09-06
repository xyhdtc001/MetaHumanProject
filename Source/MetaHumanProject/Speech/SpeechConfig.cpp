#include "SpeechConfig.h"

USpeechSystemSettings::USpeechSystemSettings(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // 设置默认配置
    SpeechConfig = FSpeechSystemConfig();
    
    // 从现有配置文件读取AppID（向后兼容）
    FString ExistingAppID;
    if (GConfig->GetString(TEXT("/Script/MetahumanCpp.SpeechManager"), TEXT("DefaultAppID"), ExistingAppID, GGameIni))
    {
        if (!ExistingAppID.IsEmpty() && ExistingAppID != TEXT("your_app_id_here"))
        {
            SpeechConfig.AppID = ExistingAppID;
            UE_LOG(LogTemp, Log, TEXT("Loaded existing AppID from config: %s"), *ExistingAppID);
        }
    }
}