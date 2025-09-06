// Copyright 2023 Stendhal Syndrome Studio. All Rights Reserved.

#pragma once

#include "LipSyncFrameSequence.h"

class SequencePostprocessing
{
public:
	SequencePostprocessing() = delete;

	template <typename T1, typename T2, typename T3>
	static auto Lerp(T1 A, T2 B, T3 T)
	{
	    return A + T * (B - A);
	}

	static TArray<float> Grow(const TArray<float>& V, int32 K);

	static void Postprocessing(TArray<FLipSyncFrame> &SequenceFrames);
	
	static void Postprocessing(int32 StartIndex, int32 EndIndex, int32 IntervalLen, TArray<FLipSyncFrame> &SequenceFrames);
	// todo: need the best verification
	static bool VisemesScoresValidator(const FLipSyncFrame &Frame, int32 CurrentPos, int32 &Pos);
private:
	static bool GetStartInterval(int32 &Start, int32 &End, int32 &IntervalLen, const TArray<FLipSyncFrame> &SequenceFrames);

	static bool GetEndInterval(int32 &Start, int32 &End, int32 &IntervalLen, const TArray<FLipSyncFrame> &SequenceFrames);
};
