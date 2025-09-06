#include "MetaHumanPlayerController.h"

#include "CommandSystem.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "LipSystemComponent.h"
#include "RuntimeAudioImporterLibrary.h"
#include "SeqConverterComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Slate/SceneViewport.h"


namespace mu
{
	enum class EImageFormat : uint8;
}

void ULipAnimationCpt::OnStartLipSys()
{
	OnLipStart.Broadcast();
}

void ULipAnimationCpt::OnTickLipAnimation(float DeltaTime, float Percent)
{
	OnLipTick.Broadcast(DeltaTime,Percent);
}

void ULipAnimationCpt::OnEndLipSys()
{
	OnLipStart.Broadcast();
}

AMetaHumanPlayerController::AMetaHumanPlayerController(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SeqConverterComponent = CreateDefaultSubobject<USeqConverterComponent>(TEXT("SeqConverter"));
	LipSystemComponent = CreateDefaultSubobject<ULipSystemComponent>(TEXT("LipSystem"));
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	bShowMouseCursor = true;
}


void AMetaHumanPlayerController::BeginPlay()
{
	Super::BeginPlay();
#if WITH_EDITOR
#else
    	UKismetSystemLibrary::ExecuteConsoleCommand(
    	GetWorld(), 
    	TEXT("DisableAllScreenMessages"), 
    	nullptr
    );
#endif
	SeqConverterComponent->OnNewSequence.AddUniqueDynamic(this,&AMetaHumanPlayerController::OnSoundSeqFinish);

	TArray<FString> FoundFiles;
	FString SearchPath = FPaths::ProjectContentDir()+TEXT("Back");

	{
		TArray<FString> LevelFiles;
		IFileManager::Get().FindFilesRecursive(LevelFiles, *SearchPath, TEXT("*.png"), true, false);
		FoundFiles.Append(LevelFiles);
	}
	{
		TArray<FString> LevelFiles;
		IFileManager::Get().FindFilesRecursive(LevelFiles, *SearchPath, TEXT("*.jpg"), true, false);
		FoundFiles.Append(LevelFiles);
	}
	{
		TArray<FString> LevelFiles;
		IFileManager::Get().FindFilesRecursive(LevelFiles, *SearchPath, TEXT("*.jpeg"), true, false);
		FoundFiles.Append(LevelFiles);
	}
	{
		TArray<FString> LevelFiles;
		IFileManager::Get().FindFilesRecursive(LevelFiles, *SearchPath, TEXT("*.bmp"), true, false);
		FoundFiles.Append(LevelFiles);
	}

	for (auto& File : FoundFiles)
	{
		UTexture2D* Tex = LoadTexture2D(File);
		if (Tex)
		{
			LoadedTextures.Emplace(Tex);
		}
	}
}

void AMetaHumanPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	SeqConverterComponent->OnNewSequence.RemoveAll(this);
}

void AMetaHumanPlayerController::TestCommand(const FString& Param)
{
	if (!GetGameInstance())
	{
		return;
	}
	UCommandSystem* CommandSystem = GetGameInstance()->GetSubsystem<UCommandSystem>();
	if (!CommandSystem)
	{
		return;
	}
	CommandSystem->PushCommandByString(Param);
}

USoundWave* CreateSoundWaveFromMemory(const TArray<uint8>& AudioData)
{
	return nullptr;
}

void AMetaHumanPlayerController::PlayHumanSpeech(const TArray<uint8>& SoundData, const FString& ExpressionType,const FString& AnimationType)
{
	if (!LipSystemComponent)
	{
		UKismetSystemLibrary::PrintString(this,TEXT("不存在组件LipSystem"));
		return;
	}
	if (ImportedInstance)
	{
		UKismetSystemLibrary::PrintString(this,TEXT("LipSystem正在处理音频..."));
		return;
	}
	if (LipSystemComponent->IsPlaying())
	{
		UKismetSystemLibrary::PrintString(this,TEXT("LipSystem正在播放音频-将打断.."));
		LipSystemComponent->Stop();
		bLipPlay = false;
		if (LipAnimationCpt.IsValid())
		{
			LipAnimationCpt->OnEndLipSys();
		}
	}
	//转为Sounwave
	URuntimeAudioImporterLibrary* RuntimeAudioImporterInstance = URuntimeAudioImporterLibrary::CreateRuntimeAudioImporter();
	if (!RuntimeAudioImporterInstance)
	{
		UKismetSystemLibrary::PrintString(this,TEXT("创建RuntimeAudioImporter失败.."));
		return;
	}
	RuntimeAudioImporterInstance->OnResultNative.AddUObject(this,&AMetaHumanPlayerController::OnSoundImported);
	SeqConverterComponent->PutAudioData(SoundData);
	RuntimeAudioImporterInstance->ImportAudioFromBuffer(SoundData,ERuntimeAudioFormat::Wav);
	ImportedInstance = RuntimeAudioImporterInstance;
	if (LipAnimationCpt.IsValid())
	{
		LipAnimationCpt->AnimationType = AnimationType;
		LipAnimationCpt->ExpressionType = ExpressionType;
	}
}

void AMetaHumanPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AdjustViewPortSize();
	//动作处理.
	if (LipSystemComponent->IsPlaying())
	{
		bLipPlay = true;
		//随机播放一些说话动作.
		if (LipAnimationCpt.IsValid())
		{
			LipAnimationCpt->OnTickLipAnimation(DeltaSeconds,LipSystemComponent->GetPercent());
		}
	}
	else if (bLipPlay)
	{
		bLipPlay = false;
		if (LipAnimationCpt.IsValid())
		{
			LipAnimationCpt->OnEndLipSys();
		}
	}
	
	if (ReadyInstanceForPlay && ImportedInstance && ImportedSoundWave)
	{
		LipSystemComponent->SetPlaybackSequence(ReadyInstanceForPlay);
		//todo播放相应的动作
		AudioComponent->SetSound(ImportedSoundWave);
		//if (DefaultSeq)
		{
			LipSystemComponent->Start(AudioComponent,ReadyInstanceForPlay);
			if (LipAnimationCpt.IsValid())
			{
				LipAnimationCpt->OnStartLipSys();
			}
		}
		
		ImportedInstance = nullptr;
		ImportedSoundWave = nullptr;
		ReadyInstanceForPlay = nullptr;
	}
}

void AMetaHumanPlayerController::OnSoundSeqFinish(ULipSyncFrameSequence* Sequence)
{
	if (Sequence)
	{
		ReadyInstanceForPlay = Sequence;
	}
	else
	{
		//转换失败
		ImportedInstance = nullptr;
		ImportedSoundWave = nullptr;
		ReadyInstanceForPlay = nullptr;
	}
}

void AMetaHumanPlayerController::OnSoundImported(class URuntimeAudioImporterLibrary* ImporterLibrary, class UImportedSoundWave* SoundWave,ERuntimeImportStatus Status)
{
	ImportedInstance = ImporterLibrary;
	ImportedSoundWave = SoundWave;
	if (ImportedSoundWave == nullptr)
	{
		//转换失败
		ImportedInstance = nullptr;
		ImportedSoundWave = nullptr;
		ReadyInstanceForPlay = nullptr;
	}
}

void AMetaHumanPlayerController::AdjustViewPortSize()
{
#if !WITH_EDITOR
	if (GSystemResolution.WindowMode == EWindowMode::Type::Fullscreen || GSystemResolution.WindowMode == EWindowMode::Type::WindowedFullscreen)
	{
		return;
	}
	
	int SizeX,SizeY;
	GetViewportSize(SizeX,SizeY);

	int TitleHeight = 0;
	TSharedPtr<SWindow> TopLevelWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	
	if (GEngine && GEngine->GameViewport)
	{
		// 获取游戏窗口
		TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
		if (Window.IsValid() && Window->GetNativeWindow())
		{
			TitleHeight  = Window->GetNativeWindow()->GetWindowTitleBarSize() + Window->GetNativeWindow()->GetWindowBorderSize();
		}
	}
	
	
	// 获取屏幕尺寸
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);
	const int32 ScreenWidth = DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left;
	const int32 ScreenHeight = DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top - TitleHeight;

	if (!FMath::IsNearlyEqual(SizeX / (float)SizeY , 9.0/16,0.001) || SizeX >= ScreenWidth || SizeY >= ScreenHeight)
	{
		float NewSizeX = FMath::Min(SizeX,ScreenWidth);
		float NewSizeY = FMath::Min(SizeY,ScreenHeight);

		if (NewSizeX / NewSizeY > 9.0/16)
		{
			NewSizeX = NewSizeY*(9.0/16);
		}
		else
		{
			NewSizeY = NewSizeX*(16/9.0);
		}
		GEngine->GameViewport->GetGameViewport()->ResizeFrame(NewSizeX,NewSizeY,GSystemResolution.WindowMode);
	}
#endif
}

void AMetaHumanPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	LipAnimationCpt = InPawn->FindComponentByClass<ULipAnimationCpt>();
}

void AMetaHumanPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
	LipAnimationCpt = nullptr;
}

UTexture2D* AMetaHumanPlayerController::LoadTexture2D(FString FilePath)
    {
        if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath)) //判断文件是否存在
        {
            return nullptr; //如果不存在 就返回空
        }

        // 加载文件数据
        TArray<uint8> FileData;
        if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *FilePath);
            return nullptr;
        }

        // 获取 ImageWrapper 模块
        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
        EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
        if (ImageFormat == EImageFormat::Invalid)
        {
            UE_LOG(LogTemp, Error, TEXT("Invalid image format: %s"), *FilePath);
            return nullptr;
        }

        TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

        // 解码图片数据
        if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
        {
            TArray<uint8> UncompressedBGRA;
            if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
            {
                // 创建纹理
                UTexture2D* Texture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);
                if (!Texture)
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to create texture"));
                    return nullptr;
                }

                // 填充纹理数据
                void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
                FMemory::Memcpy(TextureData, UncompressedBGRA.GetData(), UncompressedBGRA.Num());
                Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

                // 更新纹理资源
                Texture->UpdateResource();
                return Texture;
            }
        }

        UE_LOG(LogTemp, Error, TEXT("Failed to decode image: %s"), *FilePath);
        return nullptr;
    }
	