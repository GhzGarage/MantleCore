// Copyright Joshua Eger. All Rights Reserved.
#include "MantleHairPieceFit.h"

#include "Algo/Sort.h"
#include "Async/ParallelFor.h"
#include "Math/RandomStream.h"

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleHairPieceFitPrivate
{
	/** numpy.percentile with linear interpolation, on an ascending array. P is 0 to 1. */
	double PieceFitPercentile(const TArray<double>& Sorted, double P)
	{
		if (Sorted.IsEmpty()) return 0.0;
		const double Rank = P * (Sorted.Num() - 1);
		const int32 Lo = FMath::Clamp(FMath::FloorToInt(Rank), 0, Sorted.Num() - 1);
		const int32 Hi = FMath::Min(Lo + 1, Sorted.Num() - 1);
		return Sorted[Lo] + (Sorted[Hi] - Sorted[Lo]) * (Rank - Lo);
	}

	double PieceFitRound(double Value, double Scale)
	{
		return FMath::RoundToDouble(Value * Scale) / Scale;
	}

	/** What one grid candidate scored. */
	struct FPieceFitScore
	{
		double Outside = 0.0;
		double Gap25 = 0.0;
	};
}

// ---------------------------------------------------------------- head samples

bool FMantleHairHeadSamples::Build(TConstArrayView<FVector3f> Positions, TConstArrayView<uint32> Indices, FString& OutError)
{
	using namespace MantleHairPieceFitPrivate;
	*this = FMantleHairHeadSamples();
	if (Positions.IsEmpty() || Indices.IsEmpty() || Indices.Num() % 3 != 0)
	{
		OutError = TEXT("the head mesh has no triangles");
		return false;
	}
	for (const uint32 I : Indices)
	{
		if (I >= static_cast<uint32>(Positions.Num())) { OutError = TEXT("the head mesh has a triangle that names a missing vertex"); return false; }
	}
	const int32 NumTris = Indices.Num() / 3;
	const int32 PerTriangle = FMath::Clamp(250000 / NumTris, 3, MaxSamplesPerTriangle);
	FRandomStream Stream(7);
	Points.Reserve(NumTris * (PerTriangle + 1));
	Normals.Reserve(NumTris * (PerTriangle + 1));
	Top = -TNumericLimits<double>::Max();
	for (int32 T = 0; T < NumTris; ++T)
	{
		const FVector3f& A3 = Positions[static_cast<int32>(Indices[T * 3])];
		const FVector3f& B3 = Positions[static_cast<int32>(Indices[T * 3 + 1])];
		const FVector3f& C3 = Positions[static_cast<int32>(Indices[T * 3 + 2])];
		const FVector A(A3.X, A3.Y, A3.Z), B(B3.X, B3.Y, B3.Z), C(C3.X, C3.Y, C3.Z);
		Top = FMath::Max(Top, FMath::Max3(A.Z, B.Z, C.Z));
		FVector N = (B - A) ^ (C - A);
		const double Length = N.Size();
		if (Length < 1.e-12) continue;
		N /= Length;
		Points.Add((A + B + C) / 3.0);
		Normals.Add(N);
		for (int32 K = 0; K < PerTriangle; ++K)
		{
			double U = Stream.FRand(), V = Stream.FRand();
			if (U + V > 1.0) { U = 1.0 - U; V = 1.0 - V; }
			Points.Add(A * (1.0 - U - V) + B * U + C * V);
			Normals.Add(N);
		}
	}
	if (Points.IsEmpty())
	{
		OutError = TEXT("the head mesh has only degenerate triangles");
		return false;
	}

	// Outward is the side the top quarter of the head faces: flip every normal if those face inward.
	TArray<double> Heights;
	Heights.SetNumUninitialized(Points.Num());
	for (int32 I = 0; I < Points.Num(); ++I) Heights[I] = Points[I].Z;
	TArray<double> Sorted = Heights;
	Sorted.Sort();
	const double Above60 = PieceFitPercentile(Sorted, 0.60);
	const double Above75 = PieceFitPercentile(Sorted, 0.75);
	FVector Centre = FVector::ZeroVector;
	int32 NumCentre = 0;
	for (int32 I = 0; I < Points.Num(); ++I) if (Heights[I] > Above60) { Centre += Points[I]; ++NumCentre; }
	if (NumCentre > 0)
	{
		Centre /= NumCentre;
		double Facing = 0.0;
		int32 NumUp = 0;
		for (int32 I = 0; I < Points.Num(); ++I) if (Heights[I] > Above75) { Facing += (Points[I] - Centre) | Normals[I]; ++NumUp; }
		if (NumUp > 0 && Facing / NumUp < 0.0)
		{
			for (FVector& N : Normals) N = -N;
		}
	}

	Order.SetNumUninitialized(Points.Num());
	for (int32 I = 0; I < Order.Num(); ++I) Order[I] = I;
	BuildRange(0, Order.Num(), 0);
	return true;
}

void FMantleHairHeadSamples::BuildRange(int32 Lo, int32 Hi, int32 Axis)
{
	if (Hi - Lo <= 1) return;
	TArrayView<int32> Range = MakeArrayView(Order.GetData() + Lo, Hi - Lo);
	Algo::Sort(Range, [this, Axis](int32 A, int32 B) { return Points[A][Axis] < Points[B][Axis]; });
	const int32 Mid = (Lo + Hi) / 2;
	BuildRange(Lo, Mid, (Axis + 1) % 3);
	BuildRange(Mid + 1, Hi, (Axis + 1) % 3);
}

void FMantleHairHeadSamples::NearestRange(int32 Lo, int32 Hi, int32 Axis, const FVector& P, int32& BestId, double& BestD2) const
{
	if (Lo >= Hi) return;
	const int32 Mid = (Lo + Hi) / 2;
	const int32 Id = Order[Mid];
	const FVector& Q = Points[Id];
	const double D2 = FVector::DistSquared(P, Q);
	if (D2 < BestD2) { BestD2 = D2; BestId = Id; }
	const double Diff = P[Axis] - Q[Axis];
	const int32 Next = (Axis + 1) % 3;
	if (Diff < 0.0)
	{
		NearestRange(Lo, Mid, Next, P, BestId, BestD2);
		if (Diff * Diff < BestD2) NearestRange(Mid + 1, Hi, Next, P, BestId, BestD2);
	}
	else
	{
		NearestRange(Mid + 1, Hi, Next, P, BestId, BestD2);
		if (Diff * Diff < BestD2) NearestRange(Lo, Mid, Next, P, BestId, BestD2);
	}
}

void FMantleHairHeadSamples::Nearest(const FVector& P, int32& OutId, double& OutDistSquared) const
{
	OutId = INDEX_NONE;
	OutDistSquared = TNumericLimits<double>::Max();
	NearestRange(0, Order.Num(), 0, P, OutId, OutDistSquared);
}

// ---------------------------------------------------------------- whole-piece fit

FMantleHairPieceFit::FSearch FMantleHairPieceFit::FSearch::Default()
{
	FSearch Out;
	// numpy.arange(0.80, 1.45, 0.025), arange(-6, 6.1, 1), arange(-12, 10.1, 1).
	for (int32 I = 0; I < 26; ++I) Out.Scales.Add(0.80 + 0.025 * I);
	for (int32 I = -6; I <= 6; ++I) Out.OffsetsY.Add(I);
	for (int32 I = -12; I <= 10; ++I) Out.TopOffsets.Add(I);
	return Out;
}

FString FMantleHairPieceFit::FResult::Describe() const
{
	return FString::Printf(TEXT("Hair fitted to the head: sized to %.0f%% of its imported size, moved %.1f cm %s and %.1f cm %s; %.0f%% of the hair sits outside the skin"),
		TotalScale() * 100.0, FMath::Abs(OffsetY), OffsetY >= 0.0 ? TEXT("forward") : TEXT("back"),
		FMath::Abs(OffsetZ), OffsetZ >= 0.0 ? TEXT("up") : TEXT("down"), OutsideFraction * 100.0);
}

bool FMantleHairPieceFit::Fit(const FMantleHairHeadSamples& Head, TConstArrayView<FVector3f> Points, FResult& Out, FString& Error)
{
	return Fit(Head, Points, FSearch::Default(), Out, Error);
}

bool FMantleHairPieceFit::Fit(const FMantleHairHeadSamples& Head, TConstArrayView<FVector3f> Points, const FSearch& Search, FResult& Out, FString& Error)
{
	using namespace MantleHairPieceFitPrivate;
	Out = FResult();
	if (!Head.IsValid()) { Error = TEXT("there is no head to fit the hair to"); return false; }
	if (Points.IsEmpty()) { Error = TEXT("the hair has no vertices to fit"); return false; }
	if (Search.Scales.IsEmpty() || Search.OffsetsY.IsEmpty() || Search.TopOffsets.IsEmpty()) { Error = TEXT("the hair fit has no candidates to try"); return false; }

	double MinX = TNumericLimits<double>::Max(), MaxX = -TNumericLimits<double>::Max(), MaxZ = -TNumericLimits<double>::Max();
	for (const FVector3f& P : Points)
	{
		MinX = FMath::Min<double>(MinX, P.X);
		MaxX = FMath::Max<double>(MaxX, P.X);
		MaxZ = FMath::Max<double>(MaxZ, P.Z);
	}
	const double Width = MaxX - MinX;
	if (Width < 1.e-6) { Error = TEXT("the hair has no width to size against the head"); return false; }
	const double Unit = ReferenceWidthCm / Width;
	const double CardsTop = MaxZ * Unit;

	// An even stride over the piece scores each candidate, so the result is repeatable.
	const int32 NumPoints = Points.Num();
	const int32 Scored = FMath::Min(NumPoints, MaxFitPoints);
	TArray<FVector> Sample;
	Sample.SetNumUninitialized(Scored);
	for (int32 I = 0; I < Scored; ++I)
	{
		const int32 At = static_cast<int32>(static_cast<int64>(I) * NumPoints / Scored);
		const FVector3f& P = Points[At];
		Sample[I] = FVector(P.X, P.Y, P.Z) * Unit;
	}

	const int32 NumScales = Search.Scales.Num(), NumY = Search.OffsetsY.Num(), NumTop = Search.TopOffsets.Num();
	const double HeadTop = Head.TopCm();

	const auto Evaluate = [&](double Scale, double OffsetY, double OffsetZ)
	{
		FPieceFitScore Score;
		TArray<double> Gaps;
		Gaps.SetNumUninitialized(Scored);
		int32 Outside = 0;
		for (int32 I = 0; I < Scored; ++I)
		{
			const FVector Q = Sample[I] * Scale + FVector(0.0, OffsetY, OffsetZ);
			int32 Id = INDEX_NONE;
			double D2 = 0.0;
			Head.Nearest(Q, Id, D2);
			Gaps[I] = FMath::Sqrt(D2);
			if (((Q - Head.Point(Id)) | Head.Normal(Id)) > 0.0) ++Outside;
		}
		Gaps.Sort();
		Score.Outside = static_cast<double>(Outside) / Scored;
		Score.Gap25 = PieceFitPercentile(Gaps, 0.25);
		return Score;
	};
	const auto OffsetZFor = [&](double Scale, double TopOffset) { return HeadTop - Scale * CardsTop + TopOffset; };

	const int32 Total = NumScales * NumY * NumTop;
	TArray<FPieceFitScore> Scores;
	Scores.SetNum(Total);
	ParallelFor(Total, [&](int32 Index)
	{
		const int32 T = Index % NumTop;
		const int32 Y = (Index / NumTop) % NumY;
		const int32 S = Index / (NumTop * NumY);
		const double Scale = Search.Scales[S];
		Scores[Index] = Evaluate(Scale, Search.OffsetsY[Y], OffsetZFor(Scale, Search.TopOffsets[T]));
	});

	// Candidates outside the skin by at least the bar rank by their 25th percentile gap; the rest by their outside share. Index order is
	// scale, then Y, then top offset, all ascending, so a strict less-than breaks ties toward the smallest scale, then Y, then Z.
	int32 Best = 0;
	for (int32 Index = 1; Index < Total; ++Index)
	{
		const FPieceFitScore& A = Scores[Index];
		const FPieceFitScore& B = Scores[Best];
		const bool bMetA = A.Outside >= MinOutsideFraction;
		const bool bMetB = B.Outside >= MinOutsideFraction;
		bool bBetter;
		if (bMetA != bMetB) bBetter = bMetA;
		else if (bMetA) bBetter = A.Gap25 < B.Gap25;
		else bBetter = A.Outside > B.Outside;
		if (bBetter) Best = Index;
	}

	const int32 BestTop = Best % NumTop;
	const int32 BestY = (Best / NumTop) % NumY;
	const int32 BestScale = Best / (NumTop * NumY);
	const double Scale = Search.Scales[BestScale];
	Out.UnitScale = Unit;
	Out.Scale = PieceFitRound(Scale, 10000.0);
	Out.OffsetY = Search.OffsetsY[BestY];
	Out.OffsetZ = PieceFitRound(OffsetZFor(Scale, Search.TopOffsets[BestTop]), 100.0);
	Out.TopOffset = Search.TopOffsets[BestTop];
	Out.OutsideFraction = Scores[Best].Outside;
	Out.bMeetsOutsideBar = Scores[Best].Outside >= MinOutsideFraction;
	Out.CandidatesScored = Total;
	return true;
}

void FMantleHairPieceFit::ApplyTo(const FResult& Fit, TArray<FVector3f>& Points)
{
	for (FVector3f& P : Points) P = Fit.Apply(P);
}
