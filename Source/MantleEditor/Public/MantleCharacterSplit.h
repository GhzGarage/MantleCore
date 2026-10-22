// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MantleFusedClothing.h"
#include "MantleOutfitSections.h"

class USkeletalMesh;

/** What a piece of a clothed character is, once split (#55). */
enum class EMantleSplitRole : uint8
{
	/** The bare body (and head, hands, feet): the avatar item. */
	Body,
	/** Clothing: a top, bottom or footwear item. */
	Garment,
	/** Hair, eyebrows, eyelashes, facial hair: its own item. */
	Hair,
	/** Something worn that is not one of the three garments: a hat, gloves, a belt, a bag. */
	Accessory,
	/** Not offered (props, anything the names and bones don't place): the creator decides. */
	LeftOut,
};

/** One skeletal mesh of the character, as its names and bones describe it (FMantleCharacterSplit::Gather reads it from a mesh). */
struct MANTLEEDITOR_API FMantleSplitSource
{
	/** The mesh's asset name, "SK_Vampire_Corset". */
	FString MeshName;

	/** Its material sections (slot, material and base material names) and the triangles each has. */
	TArray<FMantleOutfitSection> Sections;
	TArray<int32> Triangles;

	/** Per section, the bones that carry its skin (its render bone map), for the body region it sits on. */
	TArray<TArray<FString>> SectionBones;

	/** The fused-clothing verdict on its bare-looking skin (FMantleFusedClothing); FusedWhy says why when clothing is built in. */
	EMantleFusedVerdict Fused = EMantleFusedVerdict::Unknown;
	FString FusedWhy;
};

/** One item the split proposes. */
struct MANTLEEDITOR_API FMantleSplitPiece
{
	/** "Vampire Corset", or "Vampire Corset: Belt" for one section of a mesh. */
	FString Name;
	EMantleSplitRole Role = EMantleSplitRole::LeftOut;

	/** The upload slot: "avatar" for the body, "top", "bottom", "footwear", a hair slot ("hair", "eyebrows", "eyelashes", "facial_hair"), "accessory"; empty when left out. */
	FString Slot;

	/** Where it comes from: the mesh, and its sections (empty: every section of the mesh). */
	int32 MeshIndex = INDEX_NONE;
	TArray<int32> Sections;

	/** Draw order from the body outwards: 0 body, 1 underwear, 2 trousers/skirts/shoes, 3 tops, 5 outerwear, 6 accessories, 7 hair. */
	int32 Layer = 0;

	/** The body region its bones say: "head", "hands", "feet", "body". */
	FString Region;

	/** Why it was placed there, in plain words. */
	FString Why;
};

/** What FMantleCharacterSplit::Propose decided. */
struct MANTLEEDITOR_API FMantleSplitProposal
{
	/** The pieces, body first, then by layer. */
	TArray<FMantleSplitPiece> Pieces;

	/** True when there is a bare body to fit from and at least one garment: the character can become body + clothing items. */
	bool bSplittable = false;

	/** When it is not splittable: why, in plain words ("it stays a fixed avatar"). */
	FString WhyNot;

	/** The proposal as log lines: "MANTLE_SPLIT <Label>: splittable ..." then one per piece. */
	TArray<FString> Describe(const FString& Label) const;
};

/**
 * #55, step 1, "propose the split": from a character's meshes (separate skeletal meshes, else the material sections of one),
 * the items it could become: the bare body, each piece of clothing, the hair, the accessories, in layer order. Pure: names
 * and bone names only, no objects (tested). Nothing is made or written.
 *
 * Rules, in this order:
 *   - hair: a mesh or section whose name says hair, eyebrows, lashes or a beard is a hair item (FMantleAvatarAssemblyFixer::HairSlotForName);
 *   - skin: sections that read as the character's skin (FMantleOutfitSections) make the body; a mesh named nude, naked,
 *     body, head, face, hand or foot (all of it skin) is a body part;
 *   - clothing: a mesh (or section) the classifier reads as top, bottom or footwear is a garment; one with several pieces in
 *     it splits by section;
 *   - the rest: an accessory word (hat, glove, belt, bag ...) or the bones (hands, head) make an accessory; a prop word, or
 *     nothing that places it, leaves it out;
 *   - splittable only when there is a body to fit from (skin that is not fused with clothing: FMantleFusedClothing) and a garment.
 * A one-mesh character whose clothes are painted into its skin stays a fixed avatar.
 */
class MANTLEEDITOR_API FMantleCharacterSplit
{
public:
	/** The layer a piece of Slot is drawn at, nudged by the words of its name (underwear lower, jackets and armour higher). Pure (tested). */
	static int32 LayerOf(EMantleSplitRole Role, const FString& Slot, const FString& Name);

	/** The body region a set of bone names says: "head", "hands", "feet" when most of them are, else "body". Pure (tested). */
	static FString RegionOf(TConstArrayView<FString> BoneNames);

	/** Whether a name says an accessory: hat, cap, glove, belt, bag, scarf, mask, glasses, necklace, bracelet, ring, earring, cape, quiver. Pure (tested). */
	static bool NamesAccessory(const FString& Name);

	/** The proposal for Meshes. Pure (tested). */
	static FMantleSplitProposal Propose(TConstArrayView<FMantleSplitSource> Meshes);

	/**
	 * Mesh as a source: its LOD 0 sections' names, triangles and bone maps, and the fused-clothing verdict on its skin
	 * (FMantleAvatarStrip::Plan). False when the mesh has no sections.
	 */
	static bool Gather(USkeletalMesh& Mesh, FMantleSplitSource& Out);

	/**
	 * The character Main belongs to: Main and the other skeletal meshes beside it (its folder, not a Converted folder), as sources, then Propose.
	 * OutSources gets them in the order the proposal's MeshIndex counts.
	 */
	static FMantleSplitProposal ProposeFor(USkeletalMesh& Main, TArray<FMantleSplitSource>& OutSources);
};
