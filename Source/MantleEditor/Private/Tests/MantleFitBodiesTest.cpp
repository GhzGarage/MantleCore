// Copyright Joshua Eger. All Rights Reserved.
#include "MantleAvatarContract.h"
#include "MantleCreatorBodyMap.h"
#include "MantleFitBodies.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Garment variants are fitted to Epic's standard body types. The pure parts: the type names, the list read from limits.json,
// the variant ids and the sex choice. Nothing here makes a MetaHuman character.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleFitBodiesTest,
	"Mantle.Editor.FitBodies.Pure",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleFitBodiesTest::RunTest(const FString& Parameters)
{
	// Type names map to Epic's enum, and nothing else does.
	EMetaHumanBodyType Type = EMetaHumanBodyType::Count;
	TestTrue(TEXT("f_med_nrw parses"), FMantleFitBodies::ParseBodyType(TEXT("f_med_nrw"), Type));
	TestTrue(TEXT("f_med_nrw is Epic's f_med_nrw"), Type == EMetaHumanBodyType::f_med_nrw);
	TestTrue(TEXT("m_med_ovw parses"), FMantleFitBodies::ParseBodyType(TEXT("m_med_ovw"), Type));
	TestTrue(TEXT("m_med_ovw is Epic's m_med_ovw"), Type == EMetaHumanBodyType::m_med_ovw);
	TestTrue(TEXT("f_tal_unw parses"), FMantleFitBodies::ParseBodyType(TEXT("f_tal_unw"), Type));
	TestTrue(TEXT("f_tal_unw is Epic's f_tal_unw"), Type == EMetaHumanBodyType::f_tal_unw);
	TestTrue(TEXT("m_srt_unw parses"), FMantleFitBodies::ParseBodyType(TEXT("m_srt_unw"), Type));
	TestTrue(TEXT("m_srt_unw is Epic's m_srt_unw"), Type == EMetaHumanBodyType::m_srt_unw);
	for (const TCHAR* Bad : { TEXT(""), TEXT("aera"), TEXT("bo"), TEXT("med_nrw"), TEXT("x_med_nrw"), TEXT("f_med"), TEXT("f_big_nrw"), TEXT("f_med_xxx"), TEXT("F_MED_NRW") })
	{
		TestFalse(FString::Printf(TEXT("\"%s\" is not a standard body type"), Bad), FMantleFitBodies::ParseBodyType(Bad, Type));
	}

	// The list: valid, distinct, capped, in order.
	const TArray<FString> Cleaned = FMantleFitBodies::SanitizeBodyTypes({ TEXT("med_unw"), TEXT("nope"), TEXT("med_nrw"), TEXT("med_unw"), TEXT("f_med_ovw"), TEXT("tal_ovw") });
	TestEqual(TEXT("invalid and repeated entries are dropped"), FString::Join(Cleaned, TEXT(",")), FString(TEXT("med_unw,med_nrw,tal_ovw")));
	const TArray<FString> Capped = FMantleFitBodies::SanitizeBodyTypes({ TEXT("med_unw"), TEXT("med_nrw"), TEXT("med_ovw"), TEXT("srt_nrw") }, 2);
	TestEqual(TEXT("the list is capped"), Capped.Num(), 2);

	// The shipped garment-fit.json lists the three medium-height builds, within the backend's 1 to 8 variants.
	FString ContractError;
	if (const FMantleAvatarContract* Contract = FMantleAvatarContract::Get(&ContractError))
	{
		TestEqual(TEXT("garment-fit.json bodyTypes"), FString::Join(Contract->GarmentFitBodyTypes, TEXT(",")), FString(TEXT("med_unw,med_nrw,med_ovw")));
		TestTrue(TEXT("one to eight variants"), Contract->GarmentFitBodyTypes.Num() >= 1 && Contract->GarmentFitBodyTypes.Num() <= 8);
		TestTrue(TEXT("every listed type is a standard body"), FMantleFitBodies::SanitizeBodyTypes(Contract->GarmentFitBodyTypes).Num() == Contract->GarmentFitBodyTypes.Num());
		TestTrue(TEXT("the sex margin is not negative"), Contract->GarmentFitSexMarginCm >= 0.0f);
	}
	else
	{
		AddError(FString::Printf(TEXT("the avatar contract did not load: %s"), *ContractError));
	}

	// Variant ids are the type names, valid for the backend (1 to 32 lowercase letters, digits, _ or -, starting with a letter).
	const TArray<FString> Suffixes = { TEXT("med_unw"), TEXT("med_nrw"), TEXT("med_ovw") };
	TestEqual(TEXT("female ids"), FString::Join(FMantleFitBodies::VariantIds(EMantleFitSex::Female, Suffixes), TEXT(",")), FString(TEXT("f_med_unw,f_med_nrw,f_med_ovw")));
	TestEqual(TEXT("male ids"), FString::Join(FMantleFitBodies::VariantIds(EMantleFitSex::Male, Suffixes), TEXT(",")), FString(TEXT("m_med_unw,m_med_nrw,m_med_ovw")));
	TestEqual(TEXT("Auto is read as female"), FMantleFitBodies::VariantIds(EMantleFitSex::Auto, Suffixes)[0], FString(TEXT("f_med_unw")));
	for (const FString& Id : FMantleFitBodies::VariantIds(EMantleFitSex::Male, Suffixes))
	{
		EMetaHumanBodyType IdType = EMetaHumanBodyType::Count;
		TestTrue(FString::Printf(TEXT("%s is a standard body type"), *Id), FMantleFitBodies::ParseBodyType(Id, IdType));
		TestTrue(FString::Printf(TEXT("%s is a valid variant id"), *Id), Id.Len() >= 1 && Id.Len() <= 32 && FChar::IsLower(Id[0]) && Id == Id.ToLower());
	}

	// The sex: male only when clearly closer to the male body; unclear or unreadable is female.
	FString Why;
	TestTrue(TEXT("close to the female body is female"), FMantleFitBodies::ChooseSex({ 0.9f, 2.0f }, { 4.0f, 5.0f }, 0.25f, Why) == EMantleFitSex::Female);
	TestTrue(TEXT("close to the male body is male"), FMantleFitBodies::ChooseSex({ 4.0f, 5.0f }, { 1.1f, 2.0f }, 0.25f, Why) == EMantleFitSex::Male);
	TestTrue(TEXT("the nearest size of each decides"), FMantleFitBodies::ChooseSex({ 5.0f, 0.8f, 6.0f }, { 3.0f, 3.5f, 4.0f }, 0.25f, Why) == EMantleFitSex::Female);
	TestTrue(TEXT("too close to call is female"), FMantleFitBodies::ChooseSex({ 1.0f }, { 0.9f }, 0.25f, Why) == EMantleFitSex::Female);
	TestTrue(TEXT("unreadable sizes are skipped"), FMantleFitBodies::ChooseSex({ -1.0f, 4.0f }, { -1.0f, 1.0f }, 0.25f, Why) == EMantleFitSex::Male);
	TestTrue(TEXT("nothing readable is female"), FMantleFitBodies::ChooseSex({ -1.0f }, { -1.0f }, 0.25f, Why) == EMantleFitSex::Female);
	TestTrue(TEXT("no sizes is female"), FMantleFitBodies::ChooseSex({}, {}, 0.25f, Why) == EMantleFitSex::Female);
	TestTrue(TEXT("one side unreadable is female"), FMantleFitBodies::ChooseSex({ -1.0f }, { 1.0f }, 0.25f, Why) == EMantleFitSex::Female);
	TestFalse(TEXT("the choice comes with a reason"), Why.IsEmpty());
	return true;
}

// The learned point map from Creator's in-memory body to the reference body: written, read back, and refused when it is not one-to-one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleCreatorBodyMapTest,
	"Mantle.Editor.FitBodies.CreatorBodyMap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleCreatorBodyMapTest::RunTest(const FString& Parameters)
{
	FMantleCreatorBodyMap Map;
	Map.SourceUVHash = 0x49f70128;
	Map.ReferenceUVHash = 0x4514a950;
	Map.PreviewOfReference = { 2, 0, 3, 1 };
	Map.MaxErrorCmAera = 0.01;
	Map.MaxErrorCmBo = 0.02;
	Map.Generated = TEXT("2026-10-19T00:00:00Z");

	FMantleCreatorBodyMap Read;
	FString Error;
	TestTrue(TEXT("a written map reads back"), FMantleCreatorBodyMap::Parse(Map.ToJson(), Read, Error));
	TestEqual(TEXT("source hash"), Read.SourceUVHash, Map.SourceUVHash);
	TestEqual(TEXT("reference hash"), Read.ReferenceUVHash, Map.ReferenceUVHash);
	TestEqual(TEXT("map"), FString::JoinBy(Read.PreviewOfReference, TEXT(","), [](const int32 Index) { return FString::FromInt(Index); }), FString(TEXT("2,0,3,1")));
	TestTrue(TEXT("applies to the layout it was learned on"), Read.Applies(0x49f70128, 4));
	TestFalse(TEXT("not to another UV hash"), Read.Applies(0x4514a950, 4));
	TestFalse(TEXT("not to another vertex count"), Read.Applies(0x49f70128, 5));

	Map.PreviewOfReference = { 2, 2, 3, 1 };
	TestFalse(TEXT("a map that is not one-to-one is refused"), FMantleCreatorBodyMap::Parse(Map.ToJson(), Read, Error));
	Map.PreviewOfReference = { 2, 0, 4, 1 };
	TestFalse(TEXT("an index past the vertex count is refused"), FMantleCreatorBodyMap::Parse(Map.ToJson(), Read, Error));
	TestFalse(TEXT("text that is not JSON is refused"), FMantleCreatorBodyMap::Parse(TEXT("nope"), Read, Error));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
