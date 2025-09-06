// Copyright 2022 Stendhal Syndrome Studio. All Rights Reserved.
#include "LipSyncWrapper.h"
#include "Interfaces/IPluginManager.h"

#include <Core.h>

DECLARE_LOG_CATEGORY_EXTERN(LogLssWrapper, Log, All);
DEFINE_LOG_CATEGORY(LogLssWrapper);

typedef struct {
	int frameNumber;
	int frameDelay;
	float* visemes;
	unsigned int visemesLength;
	
	float laughterScore;
	float* laughterCategories;
	unsigned int laughterCategoriesLength;
} LipSyncFrame;

enum LipSyncAudioDataType {
	AudioDataType_S16_Mono,
	AudioDataType_S16_Stereo,
	AudioDataType_F32_Mono,
	AudioDataType_F32_Stereo,
};

typedef enum {
	Success = 0,
	// ERRORS
	/// An unknown error has occurred
	Error_Unknown = -2200,
	/// Unable to create a context
	Error_CannotCreateContext = -2201,
	/// An invalid parameter, e.g. NULL pointer or out of range
	Error_InvalidParam = -2202,
	/// An unsupported sample rate was declared
	Error_BadSampleRate = -2203,
	/// The DLL or shared library could not be found
	Error_MissingDLL = -2204,
	/// Mismatched versions between header and libs
	Error_BadVersion = -2205,
	/// An undefined function
	Error_UndefinedFunction = -2206
  } LipSyncResult;

typedef enum {
	/// Silent viseme
	Viseme_sil,
	/// PP viseme (corresponds to p,b,m phonemes in worlds like \a put , \a bat, \a mat)
	Viseme_PP,
	/// FF viseme (corrseponds to f,v phonemes in the worlds like \a fat, \a vat)
	Viseme_FF,
	/// TH viseme (corresponds to th phoneme in words like \a think, \a that)
	Viseme_TH,
	/// DD viseme (corresponds to t,d phonemes in words like \a tip or \a doll)
	Viseme_DD,
	/// kk viseme (corresponds to k,g phonemes in words like \a call or \a gas)
	Viseme_kk,
	/// CH viseme (corresponds to tS,dZ,S phonemes in words like \a chair, \a join, \a she)
	Viseme_CH,
	/// SS viseme (corresponds to s,z phonemes in words like \a sir or \a zeal)
	Viseme_SS,
	/// nn viseme (corresponds to n,l phonemes in worlds like \a lot or \a not)
	Viseme_nn,
	/// RR viseme (corresponds to r phoneme in worlds like \a red)
	Viseme_RR,
	/// aa viseme (corresponds to A: phoneme in worlds like \a car)
	Viseme_aa,
	/// E viseme (corresponds to e phoneme in worlds like \a bed)
	Viseme_E,
	/// I viseme (corresponds to ih phoneme in worlds like \a tip)
	Viseme_ih,
	/// O viseme (corresponds to oh phoneme in worlds like \a toe)
	Viseme_oh,
	/// U viseme (corresponds to ou phoneme in worlds like \a book)
	Viseme_ou,

	/// Total number of visemes
	Viseme_Count
  } LipSyncViseme;

#define DEFINE_DLL_METHOD(name, returnType, ...) typedef returnType (*_##name##)(__VA_ARGS__);\
_##name name##_ = NULL;

DEFINE_DLL_METHOD(LipSync_Initialize, int32, int32 sampleRate, int32 bufferSize);

DEFINE_DLL_METHOD(LipSync_CreateContextWithModelFile, int32,
	uint32 &context,
	LipSyncContextProvider provider,
	const char* modelPath,
	int32 sampleRate,
	bool enableAcceleration);

DEFINE_DLL_METHOD(LipSync_ProcessFrameEx, int32,
	uint32 context,
	const void* audioBuffer,
	int sampleCount,
	LipSyncAudioDataType dataType,
	int &frameNumber,
	int &frameDelay,
	float *visemes,
	int visemeCount,
	float &laughterScore,
	float *laughterCategories,
	int laughterCategoriesLength);

DEFINE_DLL_METHOD(LipSync_DestroyContext, int32, uint32 context);

bool ULipSyncWrapper::LoadDll()
{
	const FString BaseDir = IPluginManager::Get().FindPlugin("LipSyncSystem")->GetBaseDir();
	
	const FString LibraryPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(
			*BaseDir,
			TEXT("Source/ThirdParty/LipSyncSystemLibrary/Win64")
			)
		);
	LibraryHandle = !LibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(
		*FPaths::Combine( *LibraryPath, TEXT( "KkLipSync.dll" ))
		) : nullptr;
	if (!LibraryHandle)
	{
		UE_LOG(LogLssWrapper, Error, TEXT("Error on load LSS library!"));
		return false;
	}
	LipSync_Initialize_ = (_LipSync_Initialize)FPlatformProcess::GetDllExport(LibraryHandle, L"ovrLipSyncDll_Initialize");
	LipSync_CreateContextWithModelFile_ = (_LipSync_CreateContextWithModelFile)FPlatformProcess::GetDllExport(LibraryHandle, L"ovrLipSyncDll_CreateContextWithModelFile");
	LipSync_ProcessFrameEx_ = (_LipSync_ProcessFrameEx)FPlatformProcess::GetDllExport(LibraryHandle, L"ovrLipSyncDll_ProcessFrameEx");
	LipSync_DestroyContext_ = (_LipSync_DestroyContext)FPlatformProcess::GetDllExport(LibraryHandle, L"ovrLipSyncDll_DestroyContext");
	bIsDllLoaded = LipSync_Initialize_ && LipSync_CreateContextWithModelFile_ && LipSync_ProcessFrameEx_ && LipSync_DestroyContext_;
	if (!bIsDllLoaded)
	{
		UE_LOG(LogLssWrapper, Error, TEXT("Error on load methods from the LSS library!"));
	}
	return bIsDllLoaded;
}

bool ULipSyncWrapper::Init(	LipSyncContextProvider ProviderKind,
	int SampleRate,
	int BufferSize,
	FString ModelPath,
	bool EnableAcceleration)
{
	if (!ULipSyncWrapper::bIsDllLoaded)
	{
		UE_LOG(LogLssWrapper, Error, TEXT("Error on load LSS library!"));
		return false;
	}
	auto rc = LipSync_Initialize_(SampleRate, BufferSize);
	if (rc != Success)
	{
		UE_LOG(LogLssWrapper, Error, TEXT("Can't initialize LipSyncSystem: %d"), rc);
		return false;
	}
	if (!FPaths::FileExists(ModelPath))
	{
		UE_LOG(LogLssWrapper, Error, TEXT("Model file not found: %s"), *ModelPath);
		return false;		
	}
	rc = LipSync_CreateContextWithModelFile_(
		LipSyncContext,
		ProviderKind,
		TCHAR_TO_ANSI(*ModelPath),
		SampleRate,
		EnableAcceleration);
	
	if (rc != Success)
	{
		UE_LOG(LogLssWrapper, Error, TEXT("Can't create LipSync context: %d"), rc);
		return false;
	}
	return true;
}

ULipSyncWrapper::~ULipSyncWrapper()
{
	if (!ULipSyncWrapper::bIsDllLoaded)
	{
		return;
	}
	LipSync_DestroyContext_(LipSyncContext);
}

void ULipSyncWrapper::ProcessFrame(const int16_t *AudioBuffer, int AudioBufferSize, TArray<float> &Visemes,
											 float &LaughterScore, int32_t &FrameDelay, bool Stereo)
{
	if (!ULipSyncWrapper::bIsDllLoaded)
	{
		UE_LOG(LogLssWrapper, Error, TEXT("Error on load LSS library!"));
		return;
	}
	if (Visemes.Num() != Viseme_Count)
	{
		Visemes.SetNumZeroed(Viseme_Count);
	}
	LipSyncFrame frame = {};
	frame.visemes = Visemes.GetData();
	frame.visemesLength = Visemes.Num();
	const auto rc = LipSync_ProcessFrameEx_(
		LipSyncContext,
		AudioBuffer,
		AudioBufferSize,
		Stereo ? AudioDataType_S16_Stereo : AudioDataType_S16_Mono,
		frame.frameNumber,
		frame.frameDelay,
		frame.visemes,
		frame.visemesLength,
		frame.laughterScore,
		frame.laughterCategories,
		frame.laughterCategoriesLength);
	if (rc != Success)
	{
		UE_LOG(LogLssWrapper, Error, TEXT("Failed to process frame: %d"), rc);
		return;
	}
	LaughterScore = frame.laughterScore;
	FrameDelay = frame.frameDelay;
}

void ULipSyncWrapper::UnloadDll()
{
	if (bIsDllLoaded)
	{
		FPlatformProcess::FreeDllHandle(LibraryHandle);
		LibraryHandle = nullptr;
	}
}
