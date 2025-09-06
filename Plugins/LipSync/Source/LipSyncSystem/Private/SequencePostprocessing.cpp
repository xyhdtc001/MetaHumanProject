// Copyright 2023 Stendhal Syndrome Studio. All Rights Reserved.
#include "SequencePostprocessing.h"

TArray<float> SequencePostprocessing::Grow(const TArray<float>& V, int32 K)
{
	TArray<float> res;
	res.SetNumZeroed(K);
	for (int32 i = 0; i + 1 < V.Num(); ++i) {
		for (int32 j = 0; j != K; ++j) {
			res[i * K + j] = Lerp(V[i], V[i + 1], float(j) / K);
		}
	}
	res.Last() = V.Last();
	return res;
}

void SequencePostprocessing::Postprocessing(TArray<FLipSyncFrame>& SequenceFrames)
{
	constexpr int32 NONE_VALUE{-1};
	int32 StartIndex = NONE_VALUE;
	int32 EndIndex = NONE_VALUE;
	int32 IntervalLen = NONE_VALUE;
	if (GetStartInterval(StartIndex, EndIndex, IntervalLen, SequenceFrames))
	{
		Postprocessing(StartIndex, EndIndex, IntervalLen, SequenceFrames);
	}
	if (GetEndInterval(StartIndex, EndIndex, IntervalLen, SequenceFrames))
	{
		Postprocessing(StartIndex, EndIndex, IntervalLen, SequenceFrames);
	}
}

bool SequencePostprocessing::VisemesScoresValidator(const FLipSyncFrame& Frame, int32 CurrentPos, int32& Pos)
{
	if (Frame.VisemeScores[0] != 1.f)
	{
		Pos = CurrentPos;
		return true;
	}
	return false;
}

bool SequencePostprocessing::GetStartInterval(int32& Start, int32& End, int32& IntervalLen,
	const TArray<FLipSyncFrame>& SequenceFrames)
{
	for (int32 i = 1; i < SequenceFrames.Num(); ++i)
	{
		if (VisemesScoresValidator(SequenceFrames[i], i, End))
			break;
	}
	Start = 1;
	IntervalLen = End - Start;
	return IntervalLen > 0;
}

bool SequencePostprocessing::GetEndInterval(int32& Start, int32& End, int32& IntervalLen,
	const TArray<FLipSyncFrame>& SequenceFrames)
{
	for (int32 i = SequenceFrames.Num() - 1; i > 0; --i)
	{
		if (VisemesScoresValidator(SequenceFrames[i], i, Start))
			break;
	}
	End = SequenceFrames.Num() - 1;
	IntervalLen = End - Start;
	return IntervalLen > 0;
}

void SequencePostprocessing::Postprocessing(int32 StartIndex, int32 EndIndex, int32 IntervalLen,
	TArray<FLipSyncFrame>& SequenceFrames)
{
	TArray<TArray<float>> InterpolatedVisemesScores;
	TArray<float> &StartVisemesScores = SequenceFrames[StartIndex].VisemeScores;
	TArray<float> &EndVisemeScores = SequenceFrames[EndIndex].VisemeScores;
	for (int32 i = 0; i < StartVisemesScores.Num(); ++i)
	{
		const float Start = StartVisemesScores[i];
		const float End = EndVisemeScores[i];
		auto Interpolated = Grow({Start, End}, IntervalLen);
		InterpolatedVisemesScores.Add(Interpolated);

		int k = 0;
		for (int32 j = StartIndex + 1; j <= EndIndex; ++j)
		{
			SequenceFrames[j].VisemeScores[i] = Interpolated[k++];
		}
	}

}
