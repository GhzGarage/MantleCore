// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
struct FMantleAnimClip;
struct FReferenceSkeleton;

/** How an emote moves the character as a whole, read from its MAN1 clip (FMantleEmoteCheck::Measure). */
struct FMantleEmoteMotion
{
	/** The root track's farthest distance from its first key (cm) and its largest turn from its first key (degrees); 0 without a root track. */
	double RootTravelCm = 0.0;
	double RootTurnDegrees = 0.0;

	/**
	 * The pelvis' farthest distance across the floor (x and y) from where it starts (cm), with the root held in its reference
	 * pose, as players' clients hold it: how far the body walks away from the character while the emote plays.
	 */
	double PelvisDriftCm = 0.0;
};

/**
 * How the converted clip's hands compare with the source's over a clip (FMantleEmoteCheck::CompareHands): the bend of
 * each finger at its first two joints (the angle between one segment and the next, so it does not depend on either
 * skeleton's bone axes), and each wrist's place relative to the chest (the clavicles' parent), in the chest's frame, and
 * its distance from the head, the source's scaled to the converted body's head height.
 */
struct FMantleHandMatch
{
	int32 Frames = 0;
	int32 Joints = 0;

	/** Mean of converted minus source bend (degrees): negative is straighter fingers than the source's, an under-curl. */
	double BendBiasDegrees = 0.0;

	/** Mean absolute difference, and the worst joint's mean absolute difference (degrees). */
	double BendErrorDegrees = 0.0;
	double BendWorstDegrees = 0.0;
	FName BendWorstJoint;

	/** Per frame: mean of converted minus source bend over the joints (degrees). */
	TArray<double> FrameBendBiasDegrees;

	/** Mean distance between the converted and the source wrist-from-chest vectors, in the chest's frame (cm). */
	double WristErrorCm = 0.0;

	/** Mean of converted minus source wrist-to-head distance (cm): negative is hands closer to the head than the source's. */
	double WristCloserCm = 0.0;
};

/**
 * #283 emote trials: Mantle.Dev.CheckEmote <AnimSequence> <Directory> [Label] [female|male|<SkeletalMesh>] [size]. Runs the
 * Cosmetics panel's real emote export (FMantleEditorActions::MakeCosmeticDraft, FMantleCosmeticExport::Build: identify the
 * skeleton, retarget onto the UEFN mannequin, sample, validate, encode) without uploading; reads the MAN1 back as players'
 * clients do (FMantleAnimClipReader); and draws a picture sheet of that clip on a standard Mantle body at several moments with
 * the catalog thumbnail capture (FMantleCosmeticThumbnail::Capture, the engine's thumbnail scene), posed as the client poses
 * it: the clip on the hidden UEFN driver (UMantleClipAnimInstance, the root held still), the body copying it through its
 * retargeter (UMantlePreviewRetargetAnimInstance). Writes <Label>_notes.txt and <Label>_sheet.png into Directory, and
 * <Label>_source.png: the original clip on the mesh it was made on at the same moments, to compare. Nothing is saved or
 * uploaded. Logs MANTLE_CHECK_EMOTE lines.
 */
struct FMantleEmoteCheck
{
	/** Moments the sheet shows. */
	static constexpr int32 SheetMoments = 6;
	static constexpr int32 SheetColumns = 3;

	/** Count moments evenly from the start to the end of a clip of Duration seconds (one, at 0, for no duration). Pure. */
	static TArray<float> Moments(float Duration, int32 Count);

	/** FMantleEmoteMotion of Clip on Skeleton (the driver's reference skeleton gives the keys a track leaves out). Pure. */
	static FMantleEmoteMotion Measure(const FMantleAnimClip& Clip, const FReferenceSkeleton& Skeleton);

	/**
	 * FMantleHandMatch of two clips sampled at the same moments: component-space poses (FTransform per bone of Source and of
	 * Converted, in each skeleton's bone order). Bones either skeleton lacks are skipped. Pure.
	 */
	static FMantleHandMatch CompareHands(TConstArrayView<TArray<FTransform>> SourcePoses, const FReferenceSkeleton& Source,
		TConstArrayView<TArray<FTransform>> ConvertedPoses, const FReferenceSkeleton& Converted);

	/** Component-space pose of Skeleton from local transforms (LocalOf gives a bone's local transform). Pure. */
	static TArray<FTransform> ComponentPose(const FReferenceSkeleton& Skeleton, TFunctionRef<FTransform(int32 Bone)> LocalOf);

	/** Tiles (each Tile x Tile, row-major) into a sheet Columns wide; missing tiles stay black. Pure. */
	static TArray<FColor> Tile(const TArray<TArray<FColor>>& Tiles, int32 Tile, int32 Columns, int32& OutWidth, int32& OutHeight);

	/**
	 * The whole check, as the command runs it. Body: "" or "male" (Epic's standard male medium-height normal-weight
	 * MetaHuman, made in memory), "female", or a skeletal mesh path on a Mantle avatar skeleton. False with OutError only when
	 * nothing could be checked (no such animation, no contract); a refused emote is a finished check (its notes say why).
	 */
	static bool Run(UAnimSequence* Sequence, const FString& Directory, const FString& Label, const FString& Body, int32 TileSize, FString& OutError);
};
