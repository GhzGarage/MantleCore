// Copyright Joshua Eger. All Rights Reserved.
#include "MantleHairCardsFit.h"

#include "MantleHairCapSplit.h"
#include "MantleHairPieceFit.h"
#include "MantleHairScalpFit.h"

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

	FMantleHairPieceFit::FResult Piece;
	if (!FMantleHairPieceFit::Fit(Samples, SizingPoints, Piece, Error))
	{
		Out.Warnings.Add(FString::Printf(TEXT("the hair could not be fitted to the head (%s), so it uploads as it is."), *Error));
		return Out;
	}
	FMantleHairPieceFit::ApplyTo(Piece, Out.Positions);
	Out.Notes.Add(Piece.Describe());
	if (!Piece.bMeetsOutsideBar)
	{
		Out.Notes.Add(TEXT("We couldn't find a size and position that keeps this hair outside the head, so check that it sits on the head."));
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
	const FMantleHairCapSplit::FResult Split = FMantleHairCapSplit::Split(Out.Positions, HairTriangles, HairSections, Input.SectionNames);
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
		Report.bMeetsOutsideBar = Piece.bMeetsOutsideBar;
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

	Report.bMeetsOutsideBar = Piece.bMeetsOutsideBar;
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
