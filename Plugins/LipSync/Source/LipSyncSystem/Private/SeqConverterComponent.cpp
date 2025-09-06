// Copyright 2023 Stendhal Syndrome Studio. All Rights Reserved.
#include "SeqConverterComponent.h"
#include "SeqConverterRunnable.h"

namespace
{
#if WITH_EDITOR
	TArray<uint8> FStringToBytes(const FString& String)
	{
		TArray<uint8> OutBytes;

		// Handle empty strings
		if (String.Len() > 0)
		{
			FTCHARToUTF8 Converted(*String); // Convert to UTF8
			OutBytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
		}

		return OutBytes;
	}

	TArray<uint8> uint32ToBytes(const uint32 Value, bool UseLittleEndian = true)
	{
		TArray<uint8> OutBytes;
		if (UseLittleEndian)
		{
			OutBytes.Add(Value >> 0 & 0xFF);
			OutBytes.Add(Value >> 8 & 0xFF);
			OutBytes.Add(Value >> 16 & 0xFF);
			OutBytes.Add(Value >> 24 & 0xFF);
		}
		else
		{
			OutBytes.Add(Value >> 24 & 0xFF);
			OutBytes.Add(Value >> 16 & 0xFF);
			OutBytes.Add(Value >> 8 & 0xFF);
			OutBytes.Add(Value >> 0 & 0xFF);
		}
		return OutBytes;
	}

	TArray<uint8> uint16ToBytes(const uint16 Value, bool UseLittleEndian = true)
	{
		TArray<uint8> OutBytes;
		if (UseLittleEndian)
		{
			OutBytes.Add(Value >> 0 & 0xFF);
			OutBytes.Add(Value >> 8 & 0xFF);
		}
		else
		{
			OutBytes.Add(Value >> 8 & 0xFF);
			OutBytes.Add(Value >> 0 & 0xFF);
		}
		return OutBytes;
	}

	TArray<uint8> SoundWaveToBytes(const USoundWave* Audio)
	{
		TArray<uint8> BytesArr;
		if (IsValid(Audio))
		{
			TArray<uint8> AudioBytes;
			TArray<uint8> AudioHeader;
			uint32 SampleRate;
			uint16 BitDepth = 16;
			uint16 Channels;

			if (Audio->GetImportedSoundWaveData(AudioBytes, SampleRate, Channels))
			{
				AudioHeader.Append(FStringToBytes("RIFF"));
				AudioHeader.Append(uint32ToBytes(AudioBytes.Num() + 36));
				AudioHeader.Append(FStringToBytes("WAVEfmt "));
				AudioHeader.Append(uint32ToBytes(16));
				AudioHeader.Append(uint16ToBytes(1));
				AudioHeader.Append(uint16ToBytes(Channels));
				AudioHeader.Append(uint32ToBytes(SampleRate));
				AudioHeader.Append(uint32ToBytes(SampleRate * BitDepth * Channels / 8));
				AudioHeader.Append(uint16ToBytes(BitDepth * Channels / 8));
				AudioHeader.Append(uint16ToBytes(BitDepth));
				AudioHeader.Append(FStringToBytes("data"));
				AudioHeader.Append(uint32ToBytes(AudioBytes.Num()));

				BytesArr.Append(AudioHeader);
				BytesArr.Append(AudioBytes);
			}
		}
		return BytesArr;
	}
#endif
}

USeqConverterComponent::USeqConverterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USeqConverterComponent::PutAudioData(const TArray<uint8>& AudioData)
{
	if (!bInitialized)
	{
		SeqConverterWorker_ = MakeUnique<FSequenceConverterRunnable>();
		bInitialized = true;	
	}
	SeqConverterWorker_->PutAudioData(AudioData);
}

void USeqConverterComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USeqConverterComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bInitialized)
	{
		auto Sequence = SeqConverterWorker_->GetSeq();
		if (Sequence)
		{
			OnNewSequence.Broadcast(Sequence);
		}
	}
}
