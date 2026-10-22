// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class UObject;
class USkeletalMesh;
class UStaticMesh;
struct FMantleStripPlan;
struct FMantleSilhouette;

/** What the clean head cut kept and how it was sized (FMantleMetaHumanHeadOnly::MakeCleanHead). */
struct MANTLEEDITOR_API FMantleCleanHead
{
	/** The clean head: a static mesh in the source's bind-pose space scaled by Scale (component space, cm, facing +Y). */
	TWeakObjectPtr<UStaticMesh> Mesh;
	/** The source's eye height above its feet (source cm, before Scale). */
	double EyeHeightCm = 0.0;
	/** Where the eye height came from: the eye slots, or the head joint when the character has none named so. */
	bool bEyeSlots = false;
	/** The standing height the eye height implies (EyeHeightCm / EyeToHeight) and the scale that brings it to the target. */
	double SourceHeightCm = 0.0;
	double Scale = 1.0;
	/** The head joint, scaled as the mesh (the portrait's aim); unset when the skeleton has no head bone. */
	TOptional<FVector> HeadJoint;
	/** The slots kept as the face, and the triangles kept and dropped (hair and helmet bones; above the brow or behind the face). */
	TArray<FString> FaceSlots;
	int32 KeptTriangles = 0;
	int32 DroppedHair = 0;
	int32 DroppedPosition = 0;
	/** One line for the notes and the log. */
	FString Describe() const;
};

/**
 * #770, "keep the face, choose a body": a character with no bare body under its clothing or armour (Paragon heroes, one
 * clothed mesh) keeps its face only. Its head is cut clean (MakeCleanHead), conformed on its own (MetaHuman's head_only
 * pipeline, FMantleMetaHumanConform::ConformHeadOnly) and set on MetaHuman's own parametric body at Mantle's default
 * height, which players shape with the body sliders (#849).
 */
class MANTLEEDITOR_API FMantleMetaHumanHeadOnly
{
public:
	/** Eye level as a share of standing height: sizes the head for the body it goes on. */
	static constexpr double EyeToHeight = 0.936;
	/** The face mask: nothing more than this above eye height (forehead up to under the hairline, cm). */
	static constexpr double FaceAboveEyesCm = 5.0;
	/** The face mask: nothing more than this behind the front of the eyes (roughly in front of the ears, cm). */
	static constexpr double FaceBehindEyesCm = 9.0;
	/** A slot is part of the head when this share of its skin weight is on the head bone or below it. */
	static constexpr double HeadSlotShare = 0.7;
	/** A triangle is the head's when each of its corners has this share of its weight on the head bone or below it. */
	static constexpr double HeadTriangleShare = 0.5;
	/** A triangle is hair or helmet when each of its corners has this share of its weight on such bones. */
	static constexpr double HairBoneShare = 0.5;
	/** Without eye slots, the eyes are taken this share of the way from the head joint to the top of the head. */
	static constexpr double EyeShareAboveHeadJoint = 0.35;
	/** Skin slots must span at least this share of the mesh's height. */
	static constexpr double CoveredSpanShare = 0.6;
	/** Skin may start no higher than this share of the height above the lowest point. */
	static constexpr double CoveredFeetShare = 0.25;
	/**
	 * Whether a covered body is read from Plan: no slot reads as bare skin, or the skin slots don't reach from the feet
	 * to the head (they span less than CoveredSpanShare of the mesh's height, or start more than CoveredFeetShare of it
	 * above its lowest point). OutWhy says which, in plain words. Pure (tested).
	 */
	static bool IsCovered(const FMantleStripPlan& Plan, FString& OutWhy);
	/** A head slot that isn't the face's skin: hair, a helmet, a ponytail, tassels, a hat, a mask, glasses, a band. Pure. */
	static bool NamesNotFace(const FString& SlotName);
	/** A slot named as skin ("skin", any case): never taken for hair cards, however it is drawn. Pure. */
	static bool NamesSkin(const FString& SlotName);
	/** A bone hair or a helmet hangs from: hair, pony, braid, tassel, helmet, plume, bang. Pure. */
	static bool NamesHairBone(const FString& BoneName);
	/** A slot that holds the eyes (its name ends in "eye" or "eyes", any case, before any " (Instance)"). Pure. */
	static bool NamesEyes(const FString& SlotName);
	/** The head bone among Names: "head" itself, else the shallowest bone whose name ends in "head" (not "headtop" or "head_end"). Pure. */
	static int32 FindHeadBone(TConstArrayView<FName> Names, TConstArrayView<int32> Parents);
	/**
	 * The clean head of Source's LOD 0 (bind pose), written as a static mesh Folder/SM_<Name>_FaceOnly:
	 *   1. slots and triangles: slots drawn opaque and not NamesNotFace (eye slots always); within them, the triangles whose
	 *      corners each have HeadTriangleShare of their weight under the head bone;
	 *   2. bones: within them, triangles whose three corners each have HairBoneShare on NamesHairBone bones dropped;
	 *   3. position: only the face's front kept, nothing FaceAboveEyesCm above eye height or FaceBehindEyesCm behind the
	 *      front of the eyes (eye height from the eye slots, else EyeShareAboveHeadJoint of the way to the head's top);
	 *   4. sized: scaled about the origin so the standing height the eyes imply becomes TargetHeightCm.
	 * Created gets what was made. False with why (no head bone, nothing left of the face).
	 */
	static bool MakeCleanHead(USkeletalMesh& Source, double TargetHeightCm, const FString& Folder, const FString& Name, FMantleCleanHead& Out,
		TArray<TWeakObjectPtr<UObject>>& Created, FString& OutError);
	/**
	 * Source's build (#50): its LOD 0 (bind pose) torso widths and depths at the chest, waist and hips, as shares of StandingHeightCm
	 * (a clean head's SourceHeightCm). False with why when the geometry can't be read or has no torso at those heights.
	 */
	static bool MeasureBuild(USkeletalMesh& Source, double StandingHeightCm, FMantleSilhouette& Out, FString& OutError);
};
