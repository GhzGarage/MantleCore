// Copyright Joshua Eger. All Rights Reserved.
#include "MantleParametricOutfit.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Which meshes beside a garment may be taken as the character's whole body (route 2 of a skeletal garment's sizes).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleParametricOutfitWholeBodyTest,
	"Mantle.Editor.ParametricOutfit.WholeBody",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleParametricOutfitWholeBodyTest::RunTest(const FString& Parameters)
{
	const TArray<FVector3f> Whole = { FVector3f(0, 0, 0), FVector3f(5, 0, 90), FVector3f(0, 0, 168) };
	const TArray<FVector3f> Feet = { FVector3f(0, 0, 0), FVector3f(10, 0, 4), FVector3f(0, 5, 12) };
	const TArray<FVector3f> Torso = { FVector3f(0, 0, 95), FVector3f(0, 0, 130), FVector3f(0, 0, 160) };
	TestTrue(TEXT("a body from feet to head is whole"), FMantleParametricOutfit::IsWholeBody(Whole));
	TestFalse(TEXT("feet alone are not a body"), FMantleParametricOutfit::IsWholeBody(Feet));
	TestFalse(TEXT("a torso alone is not a body"), FMantleParametricOutfit::IsWholeBody(Torso));
	TestFalse(TEXT("no vertices are not a body"), FMantleParametricOutfit::IsWholeBody(TArray<FVector3f>()));
	return true;
}

#endif
