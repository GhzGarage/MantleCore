// Copyright Joshua Eger. All Rights Reserved.
#include "MantleHairPieceFit.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The whole-piece fit (unit scale, scale/offset search) and the head samples it scores against.
// Synthetic head (a sphere), synthetic hair (a cap shell with a ring that hugs the skin), no assets.

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleHairPieceFitTestPrivate
{
	/** A UV sphere (radius 9 cm at (0, 0, 160), so its top vertex is at 169), outward windings, or inward when bInward. */
	void PieceFitSphere(bool bInward, TArray<FVector3f>& Positions, TArray<uint32>& Indices)
	{
		const int32 Rings = 24, Segments = 36;
		const double Radius = 9.0;
		const FVector Centre(0.0, 0.0, 160.0);
		Positions.Add(FVector3f(0.f, 0.f, static_cast<float>(Centre.Z + Radius)));
		for (int32 R = 1; R < Rings; ++R)
		{
			const double Theta = UE_DOUBLE_PI * R / Rings;
			for (int32 S = 0; S < Segments; ++S)
			{
				const double Phi = 2.0 * UE_DOUBLE_PI * S / Segments;
				const FVector P = Centre + Radius * FVector(FMath::Sin(Theta) * FMath::Cos(Phi), FMath::Sin(Theta) * FMath::Sin(Phi), FMath::Cos(Theta));
				Positions.Add(FVector3f(static_cast<float>(P.X), static_cast<float>(P.Y), static_cast<float>(P.Z)));
			}
		}
		const uint32 Bottom = Positions.Num();
		Positions.Add(FVector3f(0.f, 0.f, static_cast<float>(Centre.Z - Radius)));
		const auto Ring = [Segments](int32 R, int32 S) { return static_cast<uint32>(1 + (R - 1) * Segments + (S % Segments)); };
		const auto AddTriangle = [&](uint32 A, uint32 B, uint32 C)
		{
			const FVector3f& A3 = Positions[static_cast<int32>(A)];
			const FVector3f& B3 = Positions[static_cast<int32>(B)];
			const FVector3f& C3 = Positions[static_cast<int32>(C)];
			const FVector PA(A3.X, A3.Y, A3.Z), PB(B3.X, B3.Y, B3.Z), PC(C3.X, C3.Y, C3.Z);
			const bool bOutward = (((PB - PA) ^ (PC - PA)) | ((PA + PB + PC) / 3.0 - Centre)) > 0.0;
			if (bOutward != bInward) Indices.Append({A, B, C}); else Indices.Append({A, C, B});
		};
		for (int32 S = 0; S < Segments; ++S) AddTriangle(0, Ring(1, S), Ring(1, S + 1));
		for (int32 R = 1; R < Rings - 1; ++R) for (int32 S = 0; S < Segments; ++S)
		{
			AddTriangle(Ring(R, S), Ring(R + 1, S), Ring(R, S + 1));
			AddTriangle(Ring(R, S + 1), Ring(R + 1, S), Ring(R + 1, S + 1));
		}
		for (int32 S = 0; S < Segments; ++S) AddTriangle(Ring(Rings - 1, S), Bottom, Ring(Rings - 1, S + 1));
	}

	/**
	 * Hair in the head's frame: a cap of radius 9 cm centred 1 cm above the head's centre (so its top is 1 cm above the
	 * head's top, and its rim hovers 0.4 cm off the skin), 17.1 cm wide, plus a ring of points 1.5 mm off the skin low on
	 * the back of the head. Any shift that is not exactly right sinks the ring, so the right transform is the only one
	 * that keeps 97% outside and hugs the skin.
	 */
	void PieceFitHair(TArray<FVector3f>& Out)
	{
		const double Radius = 9.0;
		const FVector Centre(0.0, 0.0, 161.0);
		const double ThetaMax = FMath::Asin(0.95);
		const auto Add = [&Out](const FVector& P) { Out.Add(FVector3f(static_cast<float>(P.X), static_cast<float>(P.Y), static_cast<float>(P.Z))); };
		Add(Centre + FVector(0.0, 0.0, Radius));
		const int32 CapRings = 12;
		for (int32 R = 1; R <= CapRings; ++R)
		{
			const double Theta = ThetaMax * R / CapRings;
			const int32 Count = 2 * FMath::Max(1, FMath::RoundToInt(18.0 * FMath::Sin(Theta)));
			for (int32 J = 0; J < Count; ++J)
			{
				const double Phi = 2.0 * UE_DOUBLE_PI * J / Count;
				Add(Centre + Radius * FVector(FMath::Sin(Theta) * FMath::Cos(Phi), FMath::Sin(Theta) * FMath::Sin(Phi), FMath::Cos(Theta)));
			}
		}
		const FVector HeadCentre(0.0, 0.0, 160.0);
		const double RingTheta = FMath::DegreesToRadians(115.0);
		for (int32 J = 0; J < 120; ++J)
		{
			const double Phi = 2.0 * UE_DOUBLE_PI * J / 120;
			Add(HeadCentre + 9.15 * FVector(FMath::Sin(RingTheta) * FMath::Cos(Phi), FMath::Sin(RingTheta) * FMath::Sin(Phi), FMath::Cos(RingTheta)));
		}
	}

	/** A search small enough to reason about: only the right answer keeps the ring off the skin and hugs it. */
	FMantleHairPieceFit::FSearch PieceFitSmallSearch()
	{
		FMantleHairPieceFit::FSearch Search;
		Search.Scales = {0.95, 1.15};
		Search.OffsetsY = {0.0, 1.0, 2.0};
		Search.TopOffsets = {1.0, 2.0, 3.0};
		return Search;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleHairPieceFitSearchGridTest,
	"Mantle.Editor.HairPieceFit.DefaultSearchIsThePrototypesGrid",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleHairPieceFitSearchGridTest::RunTest(const FString& Parameters)
{
	const FMantleHairPieceFit::FSearch Search = FMantleHairPieceFit::FSearch::Default();
	TestEqual(TEXT("scales"), Search.Scales.Num(), 26);
	TestNearlyEqual(TEXT("first scale"), Search.Scales[0], 0.8, 1.e-9);
	TestNearlyEqual(TEXT("last scale"), Search.Scales.Last(), 1.425, 1.e-9);
	TestEqual(TEXT("y offsets"), Search.OffsetsY.Num(), 13);
	TestNearlyEqual(TEXT("first y offset"), Search.OffsetsY[0], -6.0, 1.e-9);
	TestNearlyEqual(TEXT("last y offset"), Search.OffsetsY.Last(), 6.0, 1.e-9);
	TestEqual(TEXT("top offsets"), Search.TopOffsets.Num(), 23);
	TestNearlyEqual(TEXT("first top offset"), Search.TopOffsets[0], -12.0, 1.e-9);
	TestNearlyEqual(TEXT("last top offset"), Search.TopOffsets.Last(), 10.0, 1.e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleHairPieceFitHeadSamplesFaceOutTest,
	"Mantle.Editor.HairPieceFit.HeadSamplesFaceOutWhateverTheWinding",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleHairPieceFitHeadSamplesFaceOutTest::RunTest(const FString& Parameters)
{
	using namespace MantleHairPieceFitTestPrivate;
	for (const bool bInward : {false, true})
	{
		TArray<FVector3f> Positions; TArray<uint32> Indices;
		PieceFitSphere(bInward, Positions, Indices);
		FMantleHairHeadSamples Head; FString Error;
		if (!TestTrue(TEXT("head builds"), Head.Build(Positions, Indices, Error))) { AddError(Error); return false; }
		TestNearlyEqual(TEXT("top of the head"), Head.TopCm(), 169.0, 1.e-4);
		int32 Wrong = 0;
		for (int32 I = 0; I < Head.Num(); ++I)
		{
			if (((Head.Point(I) - FVector(0.0, 0.0, 160.0)) | Head.Normal(I)) <= 0.0) ++Wrong;
		}
		TestEqual(bInward ? TEXT("inward-wound sphere: samples facing in") : TEXT("outward-wound sphere: samples facing in"), Wrong, 0);

		// The nearest sample to a point is within a few millimetres of it, on the near side.
		int32 Id = INDEX_NONE; double D2 = 0.0;
		Head.Nearest(FVector(0.0, 30.0, 160.0), Id, D2);
		TestTrue(TEXT("nearest sample is on the +Y side"), Id != INDEX_NONE && Head.Point(Id).Y > 8.0);
		TestTrue(TEXT("nearest sample is about 21 cm away"), FMath::Abs(FMath::Sqrt(D2) - 21.0) < 0.1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleHairPieceFitRecoversKnownTransformTest,
	"Mantle.Editor.HairPieceFit.RecoversAKnownScaleAndOffset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleHairPieceFitRecoversKnownTransformTest::RunTest(const FString& Parameters)
{
	using namespace MantleHairPieceFitTestPrivate;
	TArray<FVector3f> HeadPositions; TArray<uint32> HeadIndices;
	PieceFitSphere(false, HeadPositions, HeadIndices);
	FMantleHairHeadSamples Head; FString Error;
	if (!TestTrue(TEXT("head builds"), Head.Build(HeadPositions, HeadIndices, Error))) { AddError(Error); return false; }

	// The hair as it should end up, then moved off the head by a known scale (1.25) and offset (Y +1 cm, Z -35 cm, undone by
	// the fit): P0 = (Fitted - (0, 1, -35)) / 1.25. The fit's own unit scale squares the 17.1 cm width to 18 cm, so the
	// searched scale it must find is 17.1 / 18 = 0.95, and the offsets are 1 and -35.
	TArray<FVector3f> Fitted; PieceFitHair(Fitted);
	const double KnownScale = 1.25;
	TArray<FVector3f> Piece;
	const float Inverse = static_cast<float>(1.0 / KnownScale);
	for (const FVector3f& P : Fitted) Piece.Add(FVector3f(P.X * Inverse, (P.Y - 1.f) * Inverse, (P.Z + 35.f) * Inverse));

	FMantleHairPieceFit::FResult Fit;
	if (!TestTrue(TEXT("fit runs"), FMantleHairPieceFit::Fit(Head, Piece, PieceFitSmallSearch(), Fit, Error))) { AddError(Error); return false; }
	TestTrue(TEXT("outside the skin by the bar"), Fit.bMeetsOutsideBar);
	TestNearlyEqual(TEXT("unit scale"), Fit.UnitScale, KnownScale / 0.95, 1.e-4);
	TestNearlyEqual(TEXT("searched scale"), Fit.Scale, 0.95, 1.e-9);
	TestNearlyEqual(TEXT("total scale"), Fit.TotalScale(), KnownScale, 1.e-4);
	TestNearlyEqual(TEXT("y offset"), Fit.OffsetY, 1.0, 1.e-9);
	TestNearlyEqual(TEXT("top offset"), Fit.TopOffset, 1.0, 1.e-9);
	TestNearlyEqual(TEXT("z offset"), Fit.OffsetZ, -35.0, 0.011);
	TestEqual(TEXT("candidates"), Fit.CandidatesScored, 18);

	// Applying it puts every point back where the hair should end up.
	double Worst = 0.0;
	for (int32 I = 0; I < Piece.Num(); ++I) Worst = FMath::Max(Worst, static_cast<double>((Fit.Apply(Piece[I]) - Fitted[I]).Size()));
	TestTrue(TEXT("fitted points land on the target within 0.1 mm"), Worst < 0.01);

	TArray<FVector3f> Moved = Piece;
	FMantleHairPieceFit::ApplyTo(Fit, Moved);
	TestTrue(TEXT("ApplyTo agrees with Apply"), Moved.Num() == Piece.Num() && (Moved[5] - Fit.Apply(Piece[5])).Size() < 1.e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleHairPieceFitIdempotentTest,
	"Mantle.Editor.HairPieceFit.AnAlreadyFittedPieceStaysPut",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleHairPieceFitIdempotentTest::RunTest(const FString& Parameters)
{
	using namespace MantleHairPieceFitTestPrivate;
	TArray<FVector3f> HeadPositions; TArray<uint32> HeadIndices;
	PieceFitSphere(false, HeadPositions, HeadIndices);
	FMantleHairHeadSamples Head; FString Error;
	if (!TestTrue(TEXT("head builds"), Head.Build(HeadPositions, HeadIndices, Error))) { AddError(Error); return false; }

	// The piece already sits where the fit puts it: fitting it again changes nothing.
	TArray<FVector3f> Piece; PieceFitHair(Piece);
	FMantleHairPieceFit::FResult Fit;
	if (!TestTrue(TEXT("fit runs"), FMantleHairPieceFit::Fit(Head, Piece, PieceFitSmallSearch(), Fit, Error))) { AddError(Error); return false; }
	TestNearlyEqual(TEXT("total scale stays 1"), Fit.TotalScale(), 1.0, 1.e-4);
	TestNearlyEqual(TEXT("y offset stays 0"), Fit.OffsetY, 0.0, 1.e-9);
	TestNearlyEqual(TEXT("z offset stays 0"), Fit.OffsetZ, 0.0, 0.011);
	double Worst = 0.0;
	for (const FVector3f& P : Piece) Worst = FMath::Max(Worst, static_cast<double>((Fit.Apply(P) - P).Size()));
	TestTrue(TEXT("no point moves more than 0.1 mm"), Worst < 0.01);

	// And a second pass over the fitted result agrees with the first.
	TArray<FVector3f> Again = Piece;
	FMantleHairPieceFit::ApplyTo(Fit, Again);
	FMantleHairPieceFit::FResult Second;
	if (!TestTrue(TEXT("second fit runs"), FMantleHairPieceFit::Fit(Head, Again, PieceFitSmallSearch(), Second, Error))) { AddError(Error); return false; }
	TestNearlyEqual(TEXT("second pass total scale"), Second.TotalScale(), 1.0, 1.e-4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleHairPieceFitDefaultSearchTest,
	"Mantle.Editor.HairPieceFit.DefaultSearchFindsAFitOutsideTheSkin",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleHairPieceFitDefaultSearchTest::RunTest(const FString& Parameters)
{
	using namespace MantleHairPieceFitTestPrivate;
	TArray<FVector3f> HeadPositions; TArray<uint32> HeadIndices;
	PieceFitSphere(false, HeadPositions, HeadIndices);
	FMantleHairHeadSamples Head; FString Error;
	if (!TestTrue(TEXT("head builds"), Head.Build(HeadPositions, HeadIndices, Error))) { AddError(Error); return false; }

	// The same hair, twice the size and far away: the full prototype grid still finds a fit that clears the bar.
	TArray<FVector3f> Piece; PieceFitHair(Piece);
	for (FVector3f& P : Piece) P = FVector3f(P.X * 2.f + 3.f, P.Y * 2.f - 50.f, P.Z * 2.f + 40.f);
	FMantleHairPieceFit::FResult Fit;
	if (!TestTrue(TEXT("fit runs"), FMantleHairPieceFit::Fit(Head, Piece, Fit, Error))) { AddError(Error); return false; }
	TestEqual(TEXT("candidates"), Fit.CandidatesScored, 26 * 13 * 23);
	TestTrue(TEXT("a fit reaches the outside bar"), Fit.bMeetsOutsideBar && Fit.OutsideFraction >= FMantleHairPieceFit::MinOutsideFraction);
	TestTrue(TEXT("the searched scale is on the grid"), Fit.Scale >= 0.8 - 1.e-9 && Fit.Scale <= 1.425 + 1.e-9);
	float MinX = TNumericLimits<float>::Max(), MaxX = -TNumericLimits<float>::Max();
	for (const FVector3f& P : Piece) { const float X = Fit.Apply(P).X; MinX = FMath::Min(MinX, X); MaxX = FMath::Max(MaxX, X); }
	TestTrue(TEXT("the fitted piece is the searched scale times 18 cm wide"), FMath::Abs((MaxX - MinX) - 18.0 * Fit.Scale) < 0.01);
	TestTrue(TEXT("Describe gives words"), Fit.Describe().StartsWith(TEXT("Hair fitted to the head")));

	// Nothing to fit, nothing to fit to.
	FMantleHairPieceFit::FResult Empty;
	TestFalse(TEXT("empty hair fails"), FMantleHairPieceFit::Fit(Head, TConstArrayView<FVector3f>(), Empty, Error));
	TestFalse(TEXT("no head fails"), FMantleHairPieceFit::Fit(FMantleHairHeadSamples(), Piece, Empty, Error));
	return true;
}

#endif
