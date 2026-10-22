// Copyright Joshua Eger. All Rights Reserved.
#include "MantleCharacterSplit.h"

#include "MantleAvatarConverter.h"
#include "MantleAvatarStripFixer.h"
#include "MantleCosmeticExport.h"
#include "MantleRuntime.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Rendering/SkeletalMeshModel.h"

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleCharacterSplitPrivate
{
	bool SplitHasWord(const FString& Name, std::initializer_list<const TCHAR*> Words)
	{
		const TArray<FString> Found = FMantleOutfitSections::Words(Name);
		for (const TCHAR* Word : Words)
		{
			if (Found.Contains(Word))
			{
				return true;
			}
		}
		return false;
	}

	/** The garment piece a name's own words say ("SK_Clothes_Shirt" is a top); Unknown when none or several. */
	EMantleOutfitPiece SplitPieceOfName(const FString& Name)
	{
		FMantleOutfitSection ByName;
		ByName.SlotName = Name;
		return FMantleOutfitSections::Classify(ByName);
	}

	const TCHAR* SplitRoleName(const EMantleSplitRole Role)
	{
		switch (Role)
		{
		case EMantleSplitRole::Body: return TEXT("body");
		case EMantleSplitRole::Garment: return TEXT("garment");
		case EMantleSplitRole::Hair: return TEXT("hair");
		case EMantleSplitRole::Accessory: return TEXT("accessory");
		default: return TEXT("left out");
		}
	}

	/** One section placed: its role, slot and why. */
	struct FSectionPlace
	{
		EMantleSplitRole Role = EMantleSplitRole::LeftOut;
		FString Slot;
		FString Why;
	};

	FSectionPlace SplitPlaceSection(const FMantleSplitSource& Mesh, const int32 Section, const EMantleOutfitPiece MeshPiece)
	{
		FSectionPlace Out;
		const FMantleOutfitSection& Named = Mesh.Sections[Section];
		const FString Region = FMantleCharacterSplit::RegionOf(Mesh.SectionBones.IsValidIndex(Section) ? TConstArrayView<FString>(Mesh.SectionBones[Section]) : TConstArrayView<FString>());
		const FString HairSlot = FMantleAvatarAssemblyFixer::HairSlotForName(Named.SlotName + TEXT(" ") + Named.MaterialName);
		if (!HairSlot.IsEmpty())
		{
			Out.Role = EMantleSplitRole::Hair;
			Out.Slot = HairSlot;
			Out.Why = TEXT("its name says hair");
			return Out;
		}
		const EMantleOutfitPiece Piece = FMantleOutfitSections::Classify(Named);
		if (Piece == EMantleOutfitPiece::Skin)
		{
			Out.Role = EMantleSplitRole::Body;
			Out.Slot = TEXT("avatar");
			Out.Why = TEXT("it reads as the character's skin");
			return Out;
		}
		if (!FMantleOutfitSections::PieceSlot(Piece).IsEmpty())
		{
			Out.Role = EMantleSplitRole::Garment;
			Out.Slot = FMantleOutfitSections::PieceSlot(Piece);
			Out.Why = FString::Printf(TEXT("its materials read as a %s"), FMantleOutfitSections::PieceName(Piece));
			return Out;
		}
		// The face's own parts (eyes, teeth, a tongue, the mouth) go with the body.
		if (FMantleAvatarStrip::NamesFaceDetail(Named.SlotName, Named.MaterialName))
		{
			Out.Role = EMantleSplitRole::Body;
			Out.Slot = TEXT("avatar");
			Out.Why = TEXT("it is the character's eyes, teeth or mouth");
			return Out;
		}
		// Words the classifier doesn't know: underwear and the like.
		const FString Words = Named.SlotName + TEXT(" ") + Named.MaterialName + TEXT(" ") + Mesh.MeshName;
		if (SplitHasWord(Words, { TEXT("corset"), TEXT("bra"), TEXT("bustier"), TEXT("bodice"), TEXT("tank"), TEXT("blouse"), TEXT("sweater") }))
		{
			Out.Role = EMantleSplitRole::Garment;
			Out.Slot = TEXT("top");
			Out.Why = TEXT("its name says a top");
			return Out;
		}
		if (SplitHasWord(Words, { TEXT("underwear"), TEXT("panties"), TEXT("panty"), TEXT("briefs"), TEXT("boxers"), TEXT("leggings"), TEXT("tights") }))
		{
			Out.Role = EMantleSplitRole::Garment;
			Out.Slot = TEXT("bottom");
			Out.Why = TEXT("its name says a bottom");
			return Out;
		}
		// Unknown by its materials: the mesh's name, the words of an accessory, a prop, then the bones.
		if (FMantleOutfitSections::IsProp(Named))
		{
			Out.Why = TEXT("it looks like a prop or a weapon");
			return Out;
		}
		if (FMantleCharacterSplit::NamesAccessory(Named.SlotName + TEXT(" ") + Named.MaterialName) || FMantleCharacterSplit::NamesAccessory(Mesh.MeshName))
		{
			Out.Role = EMantleSplitRole::Accessory;
			Out.Slot = TEXT("accessory");
			Out.Why = TEXT("its name says an accessory");
			return Out;
		}
		if (!FMantleOutfitSections::PieceSlot(MeshPiece).IsEmpty())
		{
			Out.Role = EMantleSplitRole::Garment;
			Out.Slot = FMantleOutfitSections::PieceSlot(MeshPiece);
			Out.Why = FString::Printf(TEXT("its mesh is named for a %s"), FMantleOutfitSections::PieceName(MeshPiece));
			return Out;
		}
		if (Region == TEXT("feet"))
		{
			Out.Role = EMantleSplitRole::Garment;
			Out.Slot = TEXT("footwear");
			Out.Why = TEXT("it is skinned to the feet");
			return Out;
		}
		if (Region == TEXT("head") || Region == TEXT("hands"))
		{
			Out.Role = EMantleSplitRole::Accessory;
			Out.Slot = TEXT("accessory");
			Out.Why = FString::Printf(TEXT("it is skinned to the %s"), *Region);
			return Out;
		}
		Out.Why = TEXT("nothing in its names or bones says what it is");
		return Out;
	}
}

FString FMantleCharacterSplit::RegionOf(const TConstArrayView<FString> BoneNames)
{
	int32 Head = 0;
	int32 Hands = 0;
	int32 Feet = 0;
	for (const FString& Bone : BoneNames)
	{
		const FString Lower = Bone.ToLower();
		auto Has = [&Lower](std::initializer_list<const TCHAR*> Words)
		{
			for (const TCHAR* Word : Words)
			{
				if (Lower.Contains(Word))
				{
					return true;
				}
			}
			return false;
		};
		if (Has({ TEXT("head"), TEXT("neck"), TEXT("face"), TEXT("jaw"), TEXT("eye") }))
		{
			++Head;
		}
		else if (Has({ TEXT("hand"), TEXT("finger"), TEXT("thumb"), TEXT("pinky"), TEXT("index"), TEXT("middle"), TEXT("ring_") }))
		{
			++Hands;
		}
		else if (Has({ TEXT("foot"), TEXT("toe"), TEXT("ball_") }))
		{
			++Feet;
		}
	}
	const int32 Total = BoneNames.Num();
	if (Total == 0)
	{
		return TEXT("body");
	}
	auto Most = [Total](const int32 Count) { return Count * 10 >= Total * 6; };
	return Most(Head) ? TEXT("head") : Most(Hands) ? TEXT("hands") : Most(Feet) ? TEXT("feet") : TEXT("body");
}

bool FMantleCharacterSplit::NamesAccessory(const FString& Name)
{
	return MantleCharacterSplitPrivate::SplitHasWord(Name, { TEXT("hat"), TEXT("cap"), TEXT("helmet"), TEXT("glove"), TEXT("gloves"), TEXT("gauntlet"), TEXT("bracer"),
		TEXT("belt"), TEXT("sash"), TEXT("bag"), TEXT("backpack"), TEXT("purse"), TEXT("scarf"), TEXT("mask"), TEXT("glasses"), TEXT("goggles"), TEXT("necklace"),
		TEXT("bracelet"), TEXT("ring"), TEXT("earring"), TEXT("earrings"), TEXT("cape"), TEXT("cloak"), TEXT("quiver"), TEXT("pauldron"), TEXT("pauldrons") });
}

int32 FMantleCharacterSplit::LayerOf(const EMantleSplitRole Role, const FString& Slot, const FString& Name)
{
	using namespace MantleCharacterSplitPrivate;
	switch (Role)
	{
	case EMantleSplitRole::Body:
		return 0;
	case EMantleSplitRole::Hair:
		return 7;
	case EMantleSplitRole::Accessory:
		return 6;
	case EMantleSplitRole::LeftOut:
		return 8;
	default:
		break;
	}
	if (SplitHasWord(Name, { TEXT("underwear"), TEXT("bra"), TEXT("panty"), TEXT("panties"), TEXT("lingerie"), TEXT("bikini"), TEXT("boxer"), TEXT("briefs") }))
	{
		return 1;
	}
	if (SplitHasWord(Name, { TEXT("jacket"), TEXT("coat"), TEXT("cape"), TEXT("cloak"), TEXT("armor"), TEXT("armour"), TEXT("breastplate"), TEXT("robe"), TEXT("vest"), TEXT("poncho") }))
	{
		return 5;
	}
	return Slot == TEXT("top") ? 3 : 2;
}

FMantleSplitProposal FMantleCharacterSplit::Propose(const TConstArrayView<FMantleSplitSource> Meshes)
{
	using namespace MantleCharacterSplitPrivate;
	FMantleSplitProposal Out;
	bool bFused = false;
	FString FusedWhy;
	for (int32 MeshIndex = 0; MeshIndex < Meshes.Num(); ++MeshIndex)
	{
		const FMantleSplitSource& Mesh = Meshes[MeshIndex];
		const FString Display = FMantleAvatarAssemblyFixer::DisplayName(Mesh.MeshName);
		// A mesh named for hair is a hair item, whatever its sections are called.
		const FString MeshHair = FMantleAvatarAssemblyFixer::HairSlotForName(Mesh.MeshName);
		if (!MeshHair.IsEmpty())
		{
			FMantleSplitPiece& Piece = Out.Pieces.AddDefaulted_GetRef();
			Piece.Name = Display;
			Piece.Role = EMantleSplitRole::Hair;
			Piece.Slot = MeshHair;
			Piece.MeshIndex = MeshIndex;
			Piece.Why = TEXT("its name says hair");
			continue;
		}
		// Name words that say a body part (the whole mesh is skin) or a garment.
		const bool bBodyNamed = SplitHasWord(Mesh.MeshName, { TEXT("nude"), TEXT("naked"), TEXT("body"), TEXT("torso"), TEXT("head"), TEXT("face"), TEXT("hand"), TEXT("hands"),
			TEXT("foot"), TEXT("feet"), TEXT("legs"), TEXT("arms") });
		EMantleOutfitPiece MeshPiece = SplitPieceOfName(Mesh.MeshName);
		if (MeshPiece == EMantleOutfitPiece::Skin)
		{
			MeshPiece = EMantleOutfitPiece::Unknown;
		}
		TArray<FSectionPlace> Places;
		for (int32 Section = 0; Section < Mesh.Sections.Num(); ++Section)
		{
			Places.Add(SplitPlaceSection(Mesh, Section, MeshPiece));
		}
		// A body-named mesh whose sections the classifier could not place is body (a "SK_Girl_Body" with "Material0").
		bool bAnySkin = false;
		for (const FSectionPlace& Place : Places)
		{
			bAnySkin |= Place.Role == EMantleSplitRole::Body;
		}
		if (bBodyNamed && MeshPiece == EMantleOutfitPiece::Unknown)
		{
			for (FSectionPlace& Place : Places)
			{
				if (Place.Role == EMantleSplitRole::LeftOut || Place.Role == EMantleSplitRole::Accessory)
				{
					Place.Role = EMantleSplitRole::Body;
					Place.Slot = TEXT("avatar");
					Place.Why = TEXT("its mesh is named for the body");
					bAnySkin = true;
				}
			}
		}
		if (bAnySkin && Mesh.Fused == EMantleFusedVerdict::FusedClothing)
		{
			bFused = true;
			FusedWhy = Mesh.FusedWhy;
		}
		// One piece per role and slot in this mesh; a mesh that is one role is named for itself.
		TArray<TPair<EMantleSplitRole, FString>> Seen;
		for (const FSectionPlace& Place : Places)
		{
			const TPair<EMantleSplitRole, FString> Key(Place.Role, Place.Slot);
			if (Seen.Contains(Key))
			{
				continue;
			}
			Seen.Add(Key);
			FMantleSplitPiece& Piece = Out.Pieces.AddDefaulted_GetRef();
			Piece.Role = Place.Role;
			Piece.Slot = Place.Slot;
			Piece.MeshIndex = MeshIndex;
			Piece.Why = Place.Why;
			TArray<FString> Bones;
			for (int32 Section = 0; Section < Places.Num(); ++Section)
			{
				if (Places[Section].Role == Place.Role && Places[Section].Slot == Place.Slot)
				{
					Piece.Sections.Add(Section);
					if (Mesh.SectionBones.IsValidIndex(Section))
					{
						Bones.Append(Mesh.SectionBones[Section]);
					}
				}
			}
			Piece.Region = RegionOf(Bones);
			const bool bWholeMesh = Piece.Sections.Num() == Places.Num();
			Piece.Name = bWholeMesh ? Display : FString::Printf(TEXT("%s: %s"), *Display, Place.Role == EMantleSplitRole::Body ? TEXT("body") : Place.Slot.IsEmpty() ? TEXT("other") : *Place.Slot);
			if (bWholeMesh)
			{
				Piece.Sections.Reset();
			}
		}
	}
	for (FMantleSplitPiece& Piece : Out.Pieces)
	{
		Piece.Layer = LayerOf(Piece.Role, Piece.Slot, Piece.Name);
	}
	Out.Pieces.StableSort([](const FMantleSplitPiece& A, const FMantleSplitPiece& B) { return A.Layer < B.Layer; });

	bool bBody = false;
	bool bGarment = false;
	for (const FMantleSplitPiece& Piece : Out.Pieces)
	{
		bBody |= Piece.Role == EMantleSplitRole::Body;
		bGarment |= Piece.Role == EMantleSplitRole::Garment;
	}
	if (bFused)
	{
		Out.WhyNot = FString::Printf(TEXT("%s, so there is no bare body to fit from: it stays a fixed avatar"), *FusedWhy);
	}
	else if (!bBody)
	{
		Out.WhyNot = TEXT("no part of it reads as a bare body to fit from: it stays a fixed avatar");
	}
	else if (!bGarment)
	{
		Out.WhyNot = TEXT("no separate clothing was found: it stays a fixed avatar");
	}
	Out.bSplittable = Out.WhyNot.IsEmpty();
	return Out;
}

TArray<FString> FMantleSplitProposal::Describe(const FString& Label) const
{
	TArray<FString> Lines;
	Lines.Add(bSplittable ? FString::Printf(TEXT("MANTLE_SPLIT %s: splittable, %d pieces"), *Label, Pieces.Num())
		: FString::Printf(TEXT("MANTLE_SPLIT %s: not splittable (%s), %d pieces seen"), *Label, *WhyNot, Pieces.Num()));
	for (const FMantleSplitPiece& Piece : Pieces)
	{
		Lines.Add(FString::Printf(TEXT("MANTLE_SPLIT %s:   layer %d %s '%s' slot=%s region=%s sections=%s (%s)"), *Label, Piece.Layer, MantleCharacterSplitPrivate::SplitRoleName(Piece.Role),
			*Piece.Name, Piece.Slot.IsEmpty() ? TEXT("-") : *Piece.Slot, *Piece.Region, Piece.Sections.Num() == 0 ? TEXT("all") : *FString::JoinBy(Piece.Sections, TEXT(","), [](const int32 Section) { return FString::FromInt(Section); }), *Piece.Why));
	}
	return Lines;
}

bool FMantleCharacterSplit::Gather(USkeletalMesh& Mesh, FMantleSplitSource& Out)
{
	Out = FMantleSplitSource();
	Out.MeshName = Mesh.GetName();
	TArray<int32> Slots;
	Out.Sections = FMantleCosmeticExport::ReadOutfitSections(&Mesh, 0, Slots);
	if (Out.Sections.Num() == 0)
	{
		return false;
	}
	// Triangles and the bones of each slot's sections, from LOD 0's imported model (no renderer needed).
	Out.Triangles.SetNumZeroed(Slots.Num());
	Out.SectionBones.SetNum(Slots.Num());
	const FReferenceSkeleton& Reference = Mesh.GetRefSkeleton();
	if (const FSkeletalMeshModel* Model = Mesh.GetImportedModel(); Model && Model->LODModels.IsValidIndex(0))
	{
		for (const FSkelMeshSection& Section : Model->LODModels[0].Sections)
		{
			const int32 Index = Slots.IndexOfByKey(static_cast<int32>(Section.MaterialIndex));
			if (Index == INDEX_NONE)
			{
				continue;
			}
			Out.Triangles[Index] += Section.NumTriangles;
			for (const uint16 Bone : Section.BoneMap)
			{
				if (Reference.IsValidIndex(Bone))
				{
					Out.SectionBones[Index].AddUnique(Reference.GetBoneName(Bone).ToString());
				}
			}
		}
	}
	// Fused clothing in the skin it holds.
	FMantleStripPlan Plan;
	FString Error;
	if (FMantleAvatarStrip::Plan(Mesh, nullptr, Plan, Error))
	{
		for (const FMantleStripSlot& Slot : Plan.Slots)
		{
			if (Slot.bSkin && Slot.Fused == EMantleFusedVerdict::FusedClothing)
			{
				Out.Fused = EMantleFusedVerdict::FusedClothing;
				Out.FusedWhy = Slot.FusedWhy;
			}
			else if (Slot.bSkin && Out.Fused != EMantleFusedVerdict::FusedClothing)
			{
				Out.Fused = Slot.Fused;
			}
		}
	}
	return true;
}

FMantleSplitProposal FMantleCharacterSplit::ProposeFor(USkeletalMesh& Main, TArray<FMantleSplitSource>& OutSources)
{
	OutSources.Reset();
	FMantleSplitSource MainSource;
	if (Gather(Main, MainSource))
	{
		OutSources.Add(MoveTemp(MainSource));
	}
	// The meshes beside it: its folder, never Convert's output.
	const FString Folder = FPackageName::GetLongPackagePath(Main.GetPackage()->GetName());
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	FARFilter Filter;
	Filter.PackagePaths.Add(*Folder);
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.PackageName == Main.GetPackage()->GetFName() || (Asset.PackagePath.ToString() + TEXT("/")).Contains(TEXT("/Converted/")))
		{
			continue;
		}
		USkeletalMesh* Other = Cast<USkeletalMesh>(Asset.GetAsset());
		FMantleSplitSource Source;
		if (Other && Gather(*Other, Source))
		{
			OutSources.Add(MoveTemp(Source));
		}
	}
	const FMantleSplitProposal Proposal = Propose(OutSources);
	for (const FString& Line : Proposal.Describe(Main.GetName()))
	{
		UE_LOG(LogMantle, Log, TEXT("%s"), *Line);
	}
	return Proposal;
}
