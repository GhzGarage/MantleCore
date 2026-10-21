// Copyright Joshua Eger. All Rights Reserved.
#include "MantleHairCardsFit.h"

#include "MantleHairCapSplit.h"
#include "MantleHairPieceFit.h"
#include "MantleHairScalpFit.h"

double FMantleHairCardsFit::MedianDistanceToSkin(const FMantleScalpHeadSurface& Head, TConstArrayView<FVector3f> Positions, TConstArrayView<uint32> Triangles)
{
	TArray<double> Distances;
	for (const uint32 V : Triangles)
	{
		if (Positions.IsValidIndex(static_cast<int32>(V))) Distances.Add(FMath::Abs(Head.Query(FVector(Positions[V])).SignedDistance));
	}
	if (Distances.IsEmpty()) return 0.0;
	Distances.Sort();
	return Distances[Distances.Num() / 2];
}

bool FMantleHairCardsFit::CoversScalp(TConstArrayView<FVector3f> HeadPositions, TConstArrayView<FVector3f> Positions, TConstArrayView<uint32> Triangles)
{
	float Top = -TNumericLimits<float>::Max();
	for (const FVector3f& P : HeadPositions) Top = FMath::Max(Top, P.Z);
	FBox3f Scalp(ForceInit), Cap(ForceInit);
	for (const FVector3f& P : HeadPositions) if (P.Z > Top - FMantleHairHeadSamples::ScalpTopCm) Scalp += P;
	for (const uint32 V : Triangles) if (Positions.IsValidIndex(static_cast<int32>(V))) Cap += Positions[V];
	if (!Scalp.IsValid || !Cap.IsValid) return false;
	const FVector3f ScalpSize = Scalp.GetSize(), CapSize = Cap.GetSize();
	return CapSize.X >= CapMinScalpSpan * ScalpSize.X && CapSize.Y >= CapMinScalpSpan * ScalpSize.Y;
}

FMantleHairCardsFit::FResult FMantleHairCardsFit::Run(const FInput& Input)
{
	FResult Out;
	Out.Positions = TArray<FVector3f>(Input.HairPositions.GetData(), Input.HairPositions.Num());
	Out.Report.Headline = FMantleHairFitReport::Headline();

	FString Error;
	FMantleHairHeadSamples Samples;
	FMantleScalpHeadSurface Surface;
	if (!Samples.Build(Input.HeadPositions, Input.HeadIndices, Error) || !Surface.Build(Input.HeadPositions, Input.HeadIndices, Error))
	{
		Out.Warnings.Add(FString::Printf(TEXT("the hair could not be fitted to the head (%s), so it uploads as it is."), *Error));
		return Out;
	}

	// A head baked into the hair file is not hair: it must not size the piece or be pushed around as cards.
	TArray<uint32> HairTriangles;
	FMantleHairFitReport::FInput Report;
	TArray<uint8> bHairVertex;
	bHairVertex.Init(0, Input.HairPositions.Num());
	for (int32 T = 0; T + 2 < Input.HairIndices.Num(); T += 3)
	{
		const int32 Section = Input.TriangleSections.IsValidIndex(T / 3) ? Input.TriangleSections[T / 3] : INDEX_NONE;
		if (Input.SectionNames.IsValidIndex(Section) && FMantleHairFitReport::IsBakedHeadName(Input.SectionNames[Section]))
		{
			Report.bBakedHead = true;
			Report.BakedHeadSection = Input.SectionNames[Section];
			continue;
		}
		for (int32 E = 0; E < 3; ++E)
		{
			HairTriangles.Add(Input.HairIndices[T + E]);
			if (Input.HairPositions.IsValidIndex(Input.HairIndices[T + E])) bHairVertex[Input.HairIndices[T + E]] = 1;
		}
	}
	TArray<FVector3f> SizingPoints;
	for (int32 V = 0; V < Input.HairPositions.Num(); ++V) if (bHairVertex[V]) SizingPoints.Add(Input.HairPositions[V]);
	if (SizingPoints.IsEmpty())
	{
		Out.Warnings.Add(TEXT("the hair could not be fitted to the head (no hair was found in it), so it uploads as it is."));
		return Out;
	}

	// The piece is placed where its card roots meet the scalp, so it needs each card's two ends.
	const TArray<FVector3f> CardEnds = FMantleScalpFit::FindCardEnds(Input.HairPositions, HairTriangles);
	FMantleHairPieceFit::FResult Piece;
	if (!FMantleHairPieceFit::Fit(Samples, SizingPoints, CardEnds, Piece, Error))
	{
		Out.Warnings.Add(FString::Printf(TEXT("the hair could not be fitted to the head (%s), so it uploads as it is."), *Error));
		return Out;
	}
	FMantleHairPieceFit::ApplyTo(Piece, Out.Positions);
	Out.Notes.Add(Piece.Describe());
	if (!Piece.bKeepsOutOfHead)
	{
		Out.Notes.Add(TEXT("We couldn't find a size and position that keeps this hair out of the head, so check that it sits on the head."));
	}
	Out.bFitted = true;

	// Cap versus cards, on the fitted positions (ratios, so the unit does not matter).
	TArray<int32> HairSections;
	if (!Input.TriangleSections.IsEmpty())
	{
		for (int32 T = 0; T + 2 < Input.HairIndices.Num(); T += 3)
		{
			const int32 Section = Input.TriangleSections.IsValidIndex(T / 3) ? Input.TriangleSections[T / 3] : INDEX_NONE;
			if (Input.SectionNames.IsValidIndex(Section) && FMantleHairFitReport::IsBakedHeadName(Input.SectionNames[Section])) continue;
			HairSections.Add(Section);
		}
	}
	FMantleHairCapSplit::FResult Split = FMantleHairCapSplit::Split(Out.Positions, HairTriangles, HairSections, Input.SectionNames);
	if (Split.bFound)
	{
		// A cap hugs the scalp. One big piece that hangs far from the skin is a sheet of long hair, not a cap.
		const double CapMedian = MedianDistanceToSkin(Surface, Out.Positions, Split.CapIndices);
		// A cap also spreads over the top of the head. Pigtails, braids and tails are big pieces too, but narrow.
		const bool bCoversScalp = CoversScalp(Input.HeadPositions, Out.Positions, Split.CapIndices);
		if (CapMedian > CapMaxMedianDistanceCm || !bCoversScalp)
		{
			Out.Notes.Add(CapMedian > CapMaxMedianDistanceCm
				? FString::Printf(TEXT("Hair cap: the big piece hangs %.1f cm from the head, so it is treated as hair, not a cap."), CapMedian)
				: FString(TEXT("Hair cap: the big pieces do not spread over the top of the head (tails or braids), so they are treated as hair, not a cap.")));
			Split = FMantleHairCapSplit::FResult();
			Split.CardIndices = HairTriangles;
		}
	}
	Out.Notes.Add(Split.Describe());
	Out.bCapFound = Split.bFound;

	FMantleScalpFit::FMesh CardMesh;
	CardMesh.Positions = Out.Positions;
	CardMesh.Indices = Split.CardIndices;
	FMantleScalpFit::FMesh CapMesh;
	CapMesh.Positions = Out.Positions;
	CapMesh.Indices = Split.CapIndices;
	FMantleScalpFit::FResult Scalp;
	if (!FMantleScalpFit::Run(Surface, CardMesh, Split.bFound ? &CapMesh : nullptr, Scalp, Error))
	{
		Out.Warnings.Add(FString::Printf(TEXT("the hair could not be settled on the scalp (%s), so it keeps the size and position it was given."), *Error));
		Report.bKeepsOutOfHead = Piece.bKeepsOutOfHead;
		Report.bHadCap = Split.bFound;
		Out.Report = FMantleHairFitReport::Describe(Report);
		return Out;
	}

	// Cards move by the card result, the cap by the cap's wrap; a vertex the cap uses is the cap's.
	TArray<uint8> bCapVertex;
	bCapVertex.Init(0, Out.Positions.Num());
	for (const uint32 I : Split.CapIndices) bCapVertex[I] = 1;
	for (int32 V = 0; V < Out.Positions.Num(); ++V)
	{
		if (bCapVertex[V] && Scalp.CapPositions.IsValidIndex(V)) Out.Positions[V] = Scalp.CapPositions[V];
		else if (!bCapVertex[V] && Scalp.CardPositions.IsValidIndex(V)) Out.Positions[V] = Scalp.CardPositions[V];
	}
	const FMantleScalpFit::FMetrics& M = Scalp.Metrics;
	Out.Notes.Add(FString::Printf(TEXT("Hair scalp fit: %d cards, roots within 5 mm of the skin %.0f%% to %.0f%%, %d vertices lifted off the skin (largest %.1f cm)"),
		M.CardCount, M.RootsWithin5mmBefore, M.RootsWithin5mmAfter, M.CardLifted, M.CardLiftMaxCm));
	if (Scalp.bCheckFit) Out.Notes.Add(Scalp.CheckFitNote);

	Report.bKeepsOutOfHead = Piece.bKeepsOutOfHead;
	Report.bScalpCheckFit = Scalp.bCheckFit;
	Report.RootsWithin5mmAfter = M.RootsWithin5mmAfter;
	// Only the vertices the cards use: the metrics count every position, cap and built-in head included.
	TSet<uint32> CardVerticesUsed(Split.CardIndices);
	Report.CardVertices = CardVerticesUsed.Num();
	Report.CardInsideAfter = M.CardInsideAfter;
	Report.bHadCap = Split.bFound;
	Out.Report = FMantleHairFitReport::Describe(Report);
	return Out;
}
