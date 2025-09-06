#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformMemory.h"

/**
 * RAII音频缓冲区管理器
 * 自动管理音频数据的内存分配和释放
 */
class METAHUMANCPP_API FScopedAudioBuffer
{
private:
    uint8* Buffer = nullptr;
    int32 Size = 0;

public:
    explicit FScopedAudioBuffer(int32 BufferSize)
        : Size(BufferSize)
    {
        if (BufferSize > 0)
        {
            Buffer = static_cast<uint8*>(FMemory::Malloc(BufferSize));
        }
    }

    ~FScopedAudioBuffer()
    {
        if (Buffer)
        {
            FMemory::Free(Buffer);
            Buffer = nullptr;
        }
    }

    // 禁用拷贝构造和赋值
    FScopedAudioBuffer(const FScopedAudioBuffer&) = delete;
    FScopedAudioBuffer& operator=(const FScopedAudioBuffer&) = delete;

    // 支持移动语义
    FScopedAudioBuffer(FScopedAudioBuffer&& Other) noexcept
        : Buffer(Other.Buffer), Size(Other.Size)
    {
        Other.Buffer = nullptr;
        Other.Size = 0;
    }

    FScopedAudioBuffer& operator=(FScopedAudioBuffer&& Other) noexcept
    {
        if (this != &Other)
        {
            if (Buffer)
            {
                FMemory::Free(Buffer);
            }
            Buffer = Other.Buffer;
            Size = Other.Size;
            Other.Buffer = nullptr;
            Other.Size = 0;
        }
        return *this;
    }

    uint8* Get() const { return Buffer; }
    int32 GetSize() const { return Size; }
    bool IsValid() const { return Buffer != nullptr && Size > 0; }

    // 安全的数据复制
    bool CopyFrom(const uint8* SourceData, int32 SourceSize)
    {
        if (!Buffer || !SourceData || SourceSize <= 0 || SourceSize > Size)
        {
            return false;
        }
        FMemory::Memcpy(Buffer, SourceData, SourceSize);
        return true;
    }
};

/**
 * 音频缓冲区对象池
 * 减少频繁的内存分配和释放
 */
class METAHUMANCPP_API FAudioBufferPool
{
private:
    TArray<TArray<float>> AvailableBuffers;
    mutable FCriticalSection PoolMutex;
    int32 MaxPoolSize = 50; // 最大池大小
    int32 DefaultBufferSize = 1024; // 默认缓冲区大小

public:
    explicit FAudioBufferPool(int32 InMaxPoolSize = 50, int32 InDefaultBufferSize = 1024)
        : MaxPoolSize(InMaxPoolSize), DefaultBufferSize(InDefaultBufferSize)
    {
        // 预分配一些缓冲区
        AvailableBuffers.Reserve(MaxPoolSize);
        for (int32 i = 0; i < FMath::Min(10, MaxPoolSize); ++i)
        {
            TArray<float> Buffer;
            Buffer.Reserve(DefaultBufferSize);
            AvailableBuffers.Add(MoveTemp(Buffer));
        }
    }

    ~FAudioBufferPool()
    {
        FScopeLock Lock(&PoolMutex);
        AvailableBuffers.Empty();
    }

    // 获取缓冲区
    TArray<float> AcquireBuffer(int32 RequestedSize = 0)
    {
        FScopeLock Lock(&PoolMutex);
        
        int32 RequiredSize = RequestedSize > 0 ? RequestedSize : DefaultBufferSize;
        
        // 尝试从池中获取合适大小的缓冲区
        for (int32 i = 0; i < AvailableBuffers.Num(); ++i)
        {
            if (AvailableBuffers[i].Max() >= RequiredSize)
            {
                TArray<float> Buffer = MoveTemp(AvailableBuffers[i]);
                AvailableBuffers.RemoveAtSwap(i);
                Buffer.Reset(); // 保留容量但清空数据
                return Buffer;
            }
        }
        
        // 池中没有合适的，创建新的
        TArray<float> Buffer;
        Buffer.Reserve(RequiredSize);
        return Buffer;
    }

    // 释放缓冲区回池中
    void ReleaseBuffer(TArray<float>&& Buffer)
    {
        FScopeLock Lock(&PoolMutex);
        
        // 如果池未满，回收缓冲区
        if (AvailableBuffers.Num() < MaxPoolSize)
        {
            Buffer.Reset(); // 清空数据但保留容量
            AvailableBuffers.Add(MoveTemp(Buffer));
        }
        // 如果池满了，直接销毁缓冲区（让析构函数处理）
    }

    // 获取池统计信息
    void GetPoolStats(int32& OutAvailableCount, int32& OutMaxSize) const
    {
        FScopeLock Lock(&PoolMutex);
        OutAvailableCount = AvailableBuffers.Num();
        OutMaxSize = MaxPoolSize;
    }
};

/**
 * 智能SoundWave创建器
 * 安全管理SoundWave的内存
 */
class METAHUMANCPP_API FSoundWaveBuilder
{
private:
    TArray<uint8> AudioData;
    int32 SampleRate = 16000;
    int32 NumChannels = 1;
    bool bIsValid = false;

public:
    FSoundWaveBuilder& WithAudioData(const TArray<uint8>& InAudioData)
    {
        AudioData = InAudioData;
        bIsValid = AudioData.Num() > 0;
        return *this;
    }

    FSoundWaveBuilder& WithSampleRate(int32 InSampleRate)
    {
        SampleRate = InSampleRate;
        return *this;
    }

    FSoundWaveBuilder& WithChannels(int32 InNumChannels)
    {
        NumChannels = InNumChannels;
        return *this;
    }

    USoundWave* Build(UObject* Outer = nullptr)
    {
        if (!bIsValid || AudioData.Num() == 0)
        {
            return nullptr;
        }

        USoundWave* SoundWave = NewObject<USoundWave>(Outer ? Outer : GetTransientPackage());
        if (!SoundWave)
        {
            return nullptr;
        }

        // 设置音频格式参数
        SoundWave->NumChannels = NumChannels;
        SoundWave->SetSampleRate(SampleRate);
        SoundWave->Duration = static_cast<float>(AudioData.Num()) / (SampleRate * sizeof(int16) * NumChannels);
        
        // 设置音频格式
        SoundWave->SoundGroup = SOUNDGROUP_Default;
        SoundWave->bLooping = false;
        
        // 安全设置RawPCMData
        SoundWave->RawPCMDataSize = AudioData.Num();
        if (SoundWave->RawPCMData != nullptr)
        {
            FMemory::Free(SoundWave->RawPCMData);
            SoundWave->RawPCMData = nullptr;
        }
        
        if (SoundWave->RawPCMDataSize > 0)
        {
            SoundWave->RawPCMData = static_cast<uint8*>(FMemory::Malloc(SoundWave->RawPCMDataSize));
            if (SoundWave->RawPCMData)
            {
                FMemory::Memcpy(SoundWave->RawPCMData, AudioData.GetData(), SoundWave->RawPCMDataSize);
                SoundWave->TotalSamples = AudioData.Num() / sizeof(int16);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("FSoundWaveBuilder: Failed to allocate memory for RawPCMData"));
                SoundWave->RawPCMDataSize = 0;
                return nullptr;
            }
        }

        return SoundWave;
    }
};