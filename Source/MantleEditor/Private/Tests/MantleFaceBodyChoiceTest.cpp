// Copyright Joshua Eger. All Rights Reserved.
#include "MantleAvatarStripFixer.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// With the face kept, the creator picks the body it goes on; Convert never guesses it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleFaceBodyChoiceTest,
	"Mantle.Editor.MetaHumanHeadOnly.BodyChoice",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleFaceBodyChoiceTest::RunTest(const FString& Parameters)
{
	auto Line = [](const FName Kind, const bool bEnabled)
	{
		FMantleConvertFix Fix;
		Fix.FixerId = MantleAvatarFixerIds::Strip;
		Fix.PartIndex = 0;
		Fix.Kind = Kind;
		Fix.bEnabled = bEnabled;
		return Fix;
	};
	const FName Face = FMantleAvatarStripFixer::KindFaceOnly;
	const FName Feminine = FMantleAvatarStripFixer::KindFeminineBody;
	const FName Masculine = FMantleAvatarStripFixer::KindMasculineBody;

	TestTrue(TEXT("face kept, no body ticked: Convert waits"),
		!FMantleAvatarStripFixer::MissingBodyChoice({ Line(Face, true), Line(Masculine, false), Line(Feminine, false) }, 0).IsEmpty());
	TestTrue(TEXT("face kept, both ticked: Convert waits"),
		!FMantleAvatarStripFixer::MissingBodyChoice({ Line(Face, true), Line(Masculine, true), Line(Feminine, true) }, 0).IsEmpty());
	TestTrue(TEXT("face kept, feminine ticked: Convert runs"),
		FMantleAvatarStripFixer::MissingBodyChoice({ Line(Face, true), Line(Masculine, false), Line(Feminine, true) }, 0).IsEmpty());
	TestTrue(TEXT("face kept, masculine ticked: Convert runs"),
		FMantleAvatarStripFixer::MissingBodyChoice({ Line(Face, true), Line(Masculine, true), Line(Feminine, false) }, 0).IsEmpty());
	TestTrue(TEXT("whole body converted: no body to pick"),
		FMantleAvatarStripFixer::MissingBodyChoice({ Line(Face, false), Line(Masculine, false), Line(Feminine, false) }, 0).IsEmpty());
	TestTrue(TEXT("another part's lines don't count"),
		FMantleAvatarStripFixer::MissingBodyChoice({ Line(Face, true), Line(Feminine, false) }, 1).IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
