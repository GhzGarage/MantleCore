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

	/** Tiles (each Tile x Tile, row-major) into a sheet Columns wide; missing tiles stay black. Pure. */
	static TArray<FColor> Tile(const TArray<TArray<FColor>>& Tiles, int32 Tile, int32 Columns, int32& OutWidth, int32& OutHeight);

	/**
	 * The whole check, as the command runs it. Body: "" or "male" (Epic's standard male medium-height normal-weight
	 * MetaHuman, made in memory), "female", or a skeletal mesh path on a Mantle avatar skeleton. False with OutError only when
	 * nothing could be checked (no such animation, no contract); a refused emote is a finished check (its notes say why).
	 */
	static bool Run(UAnimSequence* Sequence, const FString& Directory, const FString& Label, const FString& Body, int32 TileSize, FString& OutError);
};
