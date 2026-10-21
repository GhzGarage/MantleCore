// Copyright Joshua Eger. All Rights Reserved.
#include "MantleHairCapSplit.h"
#include "MantleHairFitReport.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Cap detection (#51) and the preview's message conditions. Synthetic meshes, no assets.

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleHairCapSplitTestPrivate
{
	/** A flat grid of Side x Side quads, Size cm across, at height Z, as an open sheet: Side*Side*2 triangles. */
	void CapTestSheet(int32 Side, float Size, float Z, TArray<FVector3f>& P, TArray<uint32>& I)
	{
		const uint32 Base = P.Num();
		for (int32 Y = 0; Y <= Side; ++Y) for (int32 X = 0; X <= Side; ++X) P.Add(FVector3f(Size * X / Side, Size * Y / Side, Z));
		const auto Idx = [Base, Side](int32 X, int32 Y) { return Base + static_cast<uint32>(Y * (Side + 1) + X); };
		for (int32 Y = 0; Y < Side; ++Y) for (int32 X = 0; X < Side; ++X)
			I.Append({Idx(X, Y), Idx(X + 1, Y), Idx(X + 1, Y + 1), Idx(X, Y), Idx(X + 1, Y + 1), Idx(X, Y + 1)});
	}

	/** Count small cards, 1 cm x 5 cm, two triangles each, spaced apart (separate pieces). */
	void CapTestCards(int32 Count, TArray<FVector3f>& P, TArray<uint32>& I)
	{
		for (int32 C = 0; C < Count; ++C)
		{
			const uint32 B = P.Num();
			const float X = 3.f * C;
			P.Add(FVector3f(X, 100.f, 0.f)); P.Add(FVector3f(X + 1.f, 100.f, 0.f)); P.Add(FVector3f(X + 1.f, 105.f, 0.f)); P.Add(FVector3f(X, 105.f, 0.f));
			I.Append({B, B + 1, B + 2, B, B + 2, B + 3});
		}
	}

	/** A big closed shell: the same sheet twice at one place, so every edge, the rim included, is used by two triangles. */
	void CapTestClosedShell(TArray<FVector3f>& P, TArray<uint32>& I)
	{
		CapTestSheet(10, 20.f, 0.f, P, I);
		CapTestSheet(10, 20.f, 0.f, P, I);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleHairCapSplitFindsCapTest,
	"Mantle.Editor.HairCapSplit.FindsTheCapAndSplitsItFromTheCards",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleHairCapSplitFindsCapTest::RunTest(const FString& Parameters)
{
	using namespace MantleHairCapSplitTestPrivate;
	TArray<FVector3f> P; TArray<uint32> I;
	CapTestCards(30, P, I);
	const int32 CardTriangles = I.Num() / 3;
	CapTestSheet(10, 20.f, 0.f, P, I);
	const int32 CapTriangles = I.Num() / 3 - CardTriangles;

	// The cards come first, the cap after, in sections "Cards" and "Hair_Mat".
	TArray<int32> Sections;
	for (int32 T = 0; T < CardTriangles; ++T) Sections.Add(0);
	for (int32 T = 0; T < CapTriangles; ++T) Sections.Add(1);
	const TArray<FString> Names = {TEXT("Cards"), TEXT("Hair_Mat")};

	const FMantleHairCapSplit::FResult Result = FMantleHairCapSplit::Split(P, I, Sections, Names);
	TestTrue(TEXT("a cap is found by its size"), Result.bFound);
	TestEqual(TEXT("cap triangles"), Result.CapIndices.Num() / 3, CapTriangles);
	TestEqual(TEXT("card triangles"), Result.CardIndices.Num() / 3, CardTriangles);
	TestEqual(TEXT("one cap piece"), Result.CapPieceCount, 1);
	TestEqual(TEXT("the cap's section is named"), Result.CapSection, FString(TEXT("Hair_Mat")));
	TestEqual(TEXT("pieces counted"), Result.PieceCount, 31);
	bool bCapUsesCardVertex = false;
	for (const uint32 V : Result.CapIndices) if (V < 30u * 4u) bCapUsesCardVertex = true;
	TestFalse(TEXT("the cap's triangles use only the cap's vertices"), bCapUsesCardVertex);

	// A cap too small for the size test is still found when its section is named like one.
	TArray<FVector3f> P2; TArray<uint32> I2;
	CapTestCards(30, P2, I2);
	CapTestSheet(8, 6.f, 0.f, P2, I2); // 128 triangles over 36 cm2 against cards of 5 cm2: ratio 7, under 15, over 4
	TArray<int32> Sections2;
	for (int32 T = 0; T < I2.Num() / 3; ++T) Sections2.Add(T < 60 ? 0 : 1);
	TestFalse(TEXT("unnamed, a small cap is not taken for one"), FMantleHairCapSplit::Split(P2, I2, Sections2, {TEXT("Cards"), TEXT("Mat")}).bFound);
	TestTrue(TEXT("named like a cap, it is"), FMantleHairCapSplit::Split(P2, I2, Sections2, {TEXT("Cards"), TEXT("HairCap")}).bFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleHairCapSplitNoCapTest,
	"Mantle.Editor.HairCapSplit.LeavesCardsOnlyMeshesAlone",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleHairCapSplitNoCapTest::RunTest(const FString& Parameters)
{
	using namespace MantleHairCapSplitTestPrivate;
	{
		TArray<FVector3f> P; TArray<uint32> I;
		CapTestCards(40, P, I);
		const FMantleHairCapSplit::FResult Result = FMantleHairCapSplit::Split(P, I, {}, {});
		TestFalse(TEXT("cards only: no cap"), Result.bFound);
		TestEqual(TEXT("every triangle stays a card"), Result.CardIndices.Num(), I.Num());
		TestTrue(TEXT("the note says there is none"), Result.Describe().Contains(TEXT("none found")));
	}
	{
		// One big open sheet and one card: too few pieces to be split from anything.
		TArray<FVector3f> P; TArray<uint32> I;
		CapTestCards(1, P, I);
		CapTestSheet(10, 20.f, 0.f, P, I);
		TestFalse(TEXT("two pieces are not enough"), FMantleHairCapSplit::Split(P, I, {}, {}).bFound);
	}
	{
		// A big closed shell is not a cap (a cap has an open edge).
		TArray<FVector3f> P; TArray<uint32> I;
		CapTestCards(30, P, I);
		CapTestClosedShell(P, I);
		TestFalse(TEXT("closed: not a cap"), FMantleHairCapSplit::Split(P, I, {}, {}).bFound);
	}
	{
		// Four big open sheets are a pack of sheets, not a cap.
		TArray<FVector3f> P; TArray<uint32> I;
		CapTestCards(30, P, I);
		for (int32 S = 0; S < 4; ++S) CapTestSheet(10, 20.f, 50.f * (S + 1), P, I);
		TestFalse(TEXT("too many big pieces: nothing split"), FMantleHairCapSplit::Split(P, I, {}, {}).bFound);
	}
	{
		// A triangle that names a missing vertex leaves everything as cards.
		TArray<FVector3f> P = {FVector3f::ZeroVector, FVector3f(1, 0, 0), FVector3f(0, 1, 0)};
		const TArray<uint32> I = {0, 1, 7};
		const FMantleHairCapSplit::FResult Result = FMantleHairCapSplit::Split(P, I, {}, {});
		TestFalse(TEXT("bad index: not found"), Result.bFound);
		TestEqual(TEXT("bad index: cards untouched"), Result.CardIndices.Num(), 3);
	}
	TestTrue(TEXT("cap names"), FMantleHairCapSplit::IsCapName(TEXT("Hair_Cap")) && FMantleHairCapSplit::IsCapName(TEXT("scalp")) && !FMantleHairCapSplit::IsCapName(TEXT("Cards")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleHairFitReportTest,
	"Mantle.Editor.HairFitReport.MessageConditions",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleHairFitReportTest::RunTest(const FString& Parameters)
{
	// A clean fit: the headline only.
	FMantleHairFitReport::FInput Good;
	Good.RootsWithin5mmAfter = 95.0;
	Good.CardVertices = 1000;
	FMantleHairFitReport::FResult Result = FMantleHairFitReport::Describe(Good);
	TestEqual(TEXT("headline"), Result.Headline, FString(TEXT("We sized your hair to fit the standard head")));
	TestFalse(TEXT("clean fit: confident"), Result.bLowConfidence);
	TestFalse(TEXT("clean fit: no baked head"), Result.bBakedHead);
	TestEqual(TEXT("clean fit: no messages"), Result.Messages.Num(), 0);
	TestEqual(TEXT("confirm label"), FString(FMantleHairFitReport::ConfirmLabel()), FString(TEXT("Looks good")));
	TestEqual(TEXT("adjust label"), FString(FMantleHairFitReport::AdjustLabel()), FString(TEXT("Adjust")));

	// Each low-confidence condition on its own.
	{ FMantleHairFitReport::FInput In = Good; In.bMeetsOutsideBar = false; TestTrue(TEXT("no outside fit"), FMantleHairFitReport::Describe(In).bLowConfidence); }
	{ FMantleHairFitReport::FInput In = Good; In.bScalpCheckFit = true; TestTrue(TEXT("long lifts"), FMantleHairFitReport::Describe(In).bLowConfidence); }
	{ FMantleHairFitReport::FInput In = Good; In.CardInsideAfter = 50; TestTrue(TEXT("5% still inside"), FMantleHairFitReport::Describe(In).bLowConfidence); }
	{ FMantleHairFitReport::FInput In = Good; In.CardInsideAfter = 10; TestFalse(TEXT("1% inside is fine"), FMantleHairFitReport::Describe(In).bLowConfidence); }
	{ FMantleHairFitReport::FInput In = Good; In.RootsWithin5mmAfter = 20.0; TestTrue(TEXT("roots float"), FMantleHairFitReport::Describe(In).bLowConfidence); }
	{ FMantleHairFitReport::FInput In = Good; In.RootsWithin5mmAfter = 20.0; In.bHadCap = true; TestFalse(TEXT("roots do not count with a cap"), FMantleHairFitReport::Describe(In).bLowConfidence); }
	{ FMantleHairFitReport::FInput In = Good; In.RootsWithin5mmAfter = -1.0; TestFalse(TEXT("roots unmeasured"), FMantleHairFitReport::Describe(In).bLowConfidence); }
	{
		FMantleHairFitReport::FInput In = Good; In.bMeetsOutsideBar = false;
		const FMantleHairFitReport::FResult Low = FMantleHairFitReport::Describe(In);
		TestEqual(TEXT("low confidence says so once"), Low.Messages.Num(), 1);
		TestTrue(TEXT("and points to Adjust"), Low.Messages[0].Contains(TEXT("Adjust")));
	}

	// A baked-in head.
	{
		FMantleHairFitReport::FInput In = Good; In.bBakedHead = true; In.BakedHeadSection = TEXT("Head_Mat");
		const FMantleHairFitReport::FResult Baked = FMantleHairFitReport::Describe(In);
		TestTrue(TEXT("baked head flagged"), Baked.bBakedHead);
		TestEqual(TEXT("baked head message"), Baked.Messages.Num(), 1);
		TestTrue(TEXT("it names the section"), Baked.Messages[0].Contains(TEXT("Head_Mat")));
		TestFalse(TEXT("a baked head alone is not low confidence"), Baked.bLowConfidence);
	}
	TestTrue(TEXT("head names"), FMantleHairFitReport::IsBakedHeadName(TEXT("M_Head")) && FMantleHairFitReport::IsBakedHeadName(TEXT("Face_skin")) && FMantleHairFitReport::IsBakedHeadName(TEXT("Body")));
	TestFalse(TEXT("cap and scalp are not heads"), FMantleHairFitReport::IsBakedHeadName(TEXT("Head_Cap")) || FMantleHairFitReport::IsBakedHeadName(TEXT("Scalp_skin")));
	TestFalse(TEXT("cards are not heads"), FMantleHairFitReport::IsBakedHeadName(TEXT("Hair_Cards")));
	return true;
}

#endif
