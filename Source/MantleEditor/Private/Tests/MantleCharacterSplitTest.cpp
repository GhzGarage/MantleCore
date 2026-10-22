// Copyright Joshua Eger. All Rights Reserved.
#include "MantleCharacterSplit.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// #55 step 1: the split's rules on names and bones. No assets.

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleCharacterSplitTestPrivate
{
	FMantleSplitSource SplitTestMesh(const TCHAR* MeshName, std::initializer_list<const TCHAR*> SectionNames)
	{
		FMantleSplitSource Source;
		Source.MeshName = MeshName;
		for (const TCHAR* Name : SectionNames)
		{
			FMantleOutfitSection Section;
			Section.SlotName = Name;
			Section.MaterialName = Name;
			Source.Sections.Add(Section);
			Source.Triangles.Add(1000);
			Source.SectionBones.AddDefaulted();
		}
		return Source;
	}

	const FMantleSplitPiece* SplitFind(const FMantleSplitProposal& Proposal, const EMantleSplitRole Role, const TCHAR* Slot)
	{
		return Proposal.Pieces.FindByPredicate([Role, Slot](const FMantleSplitPiece& Piece) { return Piece.Role == Role && Piece.Slot == Slot; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleCharacterSplitSeparateMeshesTest,
	"Mantle.Editor.CharacterSplit.SeparateMeshes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleCharacterSplitSeparateMeshesTest::RunTest(const FString& Parameters)
{
	using namespace MantleCharacterSplitTestPrivate;
	// A Vampire-style character: a bare body, clothing meshes, hair.
	const TArray<FMantleSplitSource> Meshes = {
		SplitTestMesh(TEXT("SK_Girl_Nude"), { TEXT("M_Body_Skin") }),
		SplitTestMesh(TEXT("SK_Girl_Shirt"), { TEXT("M_Shirt") }),
		SplitTestMesh(TEXT("SK_Girl_Pants"), { TEXT("M_Pants") }),
		SplitTestMesh(TEXT("SK_Girl_Shoes"), { TEXT("M_Shoes") }),
		SplitTestMesh(TEXT("SK_Girl_Jacket"), { TEXT("M_Jacket_Shirt") }),
		SplitTestMesh(TEXT("SK_Girl_Hair"), { TEXT("M_Hair_Cards") }),
	};
	const FMantleSplitProposal Proposal = FMantleCharacterSplit::Propose(Meshes);
	TestTrue(TEXT("a bare body and clothing split"), Proposal.bSplittable);
	TestEqual(TEXT("six pieces"), Proposal.Pieces.Num(), 6);
	TestTrue(TEXT("the body is first"), Proposal.Pieces[0].Role == EMantleSplitRole::Body);
	TestNotNull(TEXT("a top"), SplitFind(Proposal, EMantleSplitRole::Garment, TEXT("top")));
	TestNotNull(TEXT("a bottom"), SplitFind(Proposal, EMantleSplitRole::Garment, TEXT("bottom")));
	TestNotNull(TEXT("footwear"), SplitFind(Proposal, EMantleSplitRole::Garment, TEXT("footwear")));
	TestNotNull(TEXT("hair is its own item"), SplitFind(Proposal, EMantleSplitRole::Hair, TEXT("hair")));
	const FMantleSplitPiece* Jacket = Proposal.Pieces.FindByPredicate([](const FMantleSplitPiece& Piece) { return Piece.Name.Contains(TEXT("Jacket")); });
	const FMantleSplitPiece* Shirt = Proposal.Pieces.FindByPredicate([](const FMantleSplitPiece& Piece) { return Piece.Name == TEXT("Girl Shirt"); });
	TestTrue(TEXT("a jacket is drawn over a shirt"), Jacket && Shirt && Jacket->Layer > Shirt->Layer);
	TestEqual(TEXT("hair is last"), Proposal.Pieces.Last().Role, EMantleSplitRole::Hair);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleCharacterSplitSectionsTest,
	"Mantle.Editor.CharacterSplit.Sections",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleCharacterSplitSectionsTest::RunTest(const FString& Parameters)
{
	using namespace MantleCharacterSplitTestPrivate;
	// One mesh, several material sections: skin, a shirt, trousers and a belt.
	const FMantleSplitProposal Sections = FMantleCharacterSplit::Propose({ SplitTestMesh(TEXT("SK_Hero"), { TEXT("M_Skin"), TEXT("M_Shirt"), TEXT("M_Trousers"), TEXT("M_Belt") }) });
	TestTrue(TEXT("sections split a one-mesh character"), Sections.bSplittable);
	TestNotNull(TEXT("its skin is the body"), SplitFind(Sections, EMantleSplitRole::Body, TEXT("avatar")));
	TestNotNull(TEXT("its shirt is a top"), SplitFind(Sections, EMantleSplitRole::Garment, TEXT("top")));
	TestNotNull(TEXT("its trousers are a bottom"), SplitFind(Sections, EMantleSplitRole::Garment, TEXT("bottom")));
	TestNotNull(TEXT("its belt is an accessory"), SplitFind(Sections, EMantleSplitRole::Accessory, TEXT("accessory")));

	// One mesh, one section: nothing to split.
	const FMantleSplitProposal One = FMantleCharacterSplit::Propose({ SplitTestMesh(TEXT("SK_Hero"), { TEXT("M_Character") }) });
	TestFalse(TEXT("a one-section character stays a fixed avatar"), One.bSplittable);
	TestTrue(TEXT("and says so"), One.WhyNot.Contains(TEXT("fixed avatar")));

	// A body with clothing built into it (the fused-clothing verdict): not splittable.
	FMantleSplitSource Fused = SplitTestMesh(TEXT("SK_Hero"), { TEXT("M_Skin"), TEXT("M_Shirt") });
	Fused.Fused = EMantleFusedVerdict::FusedClothing;
	Fused.FusedWhy = TEXT("clothing looks built into the body");
	const FMantleSplitProposal Welded = FMantleCharacterSplit::Propose({ Fused });
	TestFalse(TEXT("clothing built into the skin is not splittable"), Welded.bSplittable);
	TestTrue(TEXT("and names the clothing"), Welded.WhyNot.Contains(TEXT("built into the body")));

	// Clothes and no body: nothing to fit from.
	const FMantleSplitProposal NoBody = FMantleCharacterSplit::Propose({ SplitTestMesh(TEXT("SK_Hero_Shirt"), { TEXT("M_Shirt") }), SplitTestMesh(TEXT("SK_Hero_Pants"), { TEXT("M_Pants") }) });
	TestFalse(TEXT("no bare body, no split"), NoBody.bSplittable);

	// A bare body alone: no clothing to offer.
	const FMantleSplitProposal NoClothes = FMantleCharacterSplit::Propose({ SplitTestMesh(TEXT("SK_Hero_Body"), { TEXT("M_Skin") }) });
	TestFalse(TEXT("a body with no clothing does not split"), NoClothes.bSplittable);

	// A bow is not clothing.
	const FMantleSplitProposal Prop = FMantleCharacterSplit::Propose({ SplitTestMesh(TEXT("SK_Hero_Body"), { TEXT("M_Skin") }), SplitTestMesh(TEXT("SK_Hero_Shirt"), { TEXT("M_Shirt") }),
		SplitTestMesh(TEXT("SK_Hero_Bow"), { TEXT("M_Bow") }) });
	TestTrue(TEXT("a prop is left out, the rest splits"), Prop.bSplittable && SplitFind(Prop, EMantleSplitRole::LeftOut, TEXT("")) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleCharacterSplitBonesTest,
	"Mantle.Editor.CharacterSplit.Bones",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleCharacterSplitBonesTest::RunTest(const FString& Parameters)
{
	using namespace MantleCharacterSplitTestPrivate;
	TestEqual(TEXT("head bones are the head"), FMantleCharacterSplit::RegionOf({ TEXT("head"), TEXT("neck_01"), TEXT("neck_02") }), FString(TEXT("head")));
	TestEqual(TEXT("finger bones are the hands"), FMantleCharacterSplit::RegionOf({ TEXT("hand_l"), TEXT("index_01_l"), TEXT("thumb_01_l"), TEXT("middle_01_l") }), FString(TEXT("hands")));
	TestEqual(TEXT("foot bones are the feet"), FMantleCharacterSplit::RegionOf({ TEXT("foot_l"), TEXT("ball_l"), TEXT("calf_l") }), FString(TEXT("feet")));
	TestEqual(TEXT("a mix is the body"), FMantleCharacterSplit::RegionOf({ TEXT("spine_01"), TEXT("spine_02"), TEXT("head") }), FString(TEXT("body")));

	// A mesh the names don't place is placed by its bones: skinned to the feet it is footwear, to the head an accessory.
	FMantleSplitSource Feet = SplitTestMesh(TEXT("SK_Hero_Thing"), { TEXT("M_Thing") });
	Feet.SectionBones[0] = { TEXT("foot_l"), TEXT("ball_l"), TEXT("foot_r") };
	FMantleSplitSource Head = SplitTestMesh(TEXT("SK_Hero_Thing2"), { TEXT("M_Thing2") });
	Head.SectionBones[0] = { TEXT("head"), TEXT("neck_01") };
	const FMantleSplitProposal Proposal = FMantleCharacterSplit::Propose({ SplitTestMesh(TEXT("SK_Hero_Body"), { TEXT("M_Skin") }), Feet, Head });
	TestNotNull(TEXT("skinned to the feet: footwear"), SplitFind(Proposal, EMantleSplitRole::Garment, TEXT("footwear")));
	TestNotNull(TEXT("skinned to the head: an accessory"), SplitFind(Proposal, EMantleSplitRole::Accessory, TEXT("accessory")));
	TestTrue(TEXT("a hat is an accessory by name"), FMantleCharacterSplit::NamesAccessory(TEXT("SK_Hero_Hat")));
	TestFalse(TEXT("a shirt is not"), FMantleCharacterSplit::NamesAccessory(TEXT("SK_Hero_Shirt")));
	TestTrue(TEXT("underwear is drawn under trousers"), FMantleCharacterSplit::LayerOf(EMantleSplitRole::Garment, TEXT("bottom"), TEXT("Underwear")) < FMantleCharacterSplit::LayerOf(EMantleSplitRole::Garment, TEXT("bottom"), TEXT("Pants")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
