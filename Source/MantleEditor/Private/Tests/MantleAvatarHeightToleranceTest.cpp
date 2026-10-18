// Copyright Joshua Eger. All Rights Reserved.
#include "MantleAvatarHeight.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Converted avatars may land within +-1 cm of the driver height, presets within +-0.5. Pure; no assets.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleConvertedHeightToleranceTest,
	"Mantle.Editor.AvatarHeight.ConvertedTolerance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleConvertedHeightToleranceTest::RunTest(const FString& Parameters)
{
	const double Target = 165.6;
	const double Converted = FMantleAvatarHeight::ConvertedToleranceCm;
	TestEqual(TEXT("converted tolerance is 1 cm"), Converted, 1.0);
	TestTrue(TEXT("on target passes"), FMantleAvatarHeight::IsWithin(165.6, Target, Converted));
	TestTrue(TEXT("0.88 cm under passes (Sophie)"), FMantleAvatarHeight::IsWithin(164.72, Target, Converted));
	TestTrue(TEXT("0.99 cm over passes"), FMantleAvatarHeight::IsWithin(166.59, Target, Converted));
	TestFalse(TEXT("1.1 cm over fails"), FMantleAvatarHeight::IsWithin(166.7, Target, Converted));
	TestFalse(TEXT("1.1 cm under fails"), FMantleAvatarHeight::IsWithin(164.5, Target, Converted));
	TestFalse(TEXT("unmeasured fails"), FMantleAvatarHeight::IsWithin(-1.0, Target, Converted));
	TestFalse(TEXT("presets keep 0.5: 0.88 cm under fails"), FMantleAvatarHeight::IsWithin(164.72, Target));
	TestTrue(TEXT("presets keep 0.5: 0.4 cm over passes"), FMantleAvatarHeight::IsWithin(166.0, Target));
	TestTrue(TEXT("the line for 0.88 cm under is not a warning at the converted tolerance"),
		FMantleAvatarHeight::BuiltHeightLine(164.72, Target, Converted).StartsWith(TEXT("BUILT HEIGHT")));
	TestTrue(TEXT("the line for 1.1 cm over warns and names +- 1.0"),
		FMantleAvatarHeight::BuiltHeightLine(166.7, Target, Converted).EndsWith(TEXT("+- 1.0")));
	return true;
}

#endif
