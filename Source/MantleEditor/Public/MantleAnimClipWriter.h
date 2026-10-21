// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MantleCosmeticValidation.h"

class FMantleAvatarContract;
class UAnimSequence;
class USkeletalMesh;

/**
 * One bone's keys in an emote: local transforms in Unreal's frame and units. Named apart from MantleRuntime's reader types
 * (MantleAnimClip.h's FMantleAnimClipTrack and FMantleAnimClipEvent), so the writer and the reader can meet in one
 * translation unit (MantleEditor builds as a unity build; the emote check decodes what it encodes).
 */
struct MANTLEEDITOR_API FMantleAnimClipTrackData
{
	FName Bone;

	/** 1 key (constant) or FrameCount keys. */
	TArray<FQuat4f> Rotations;

	/** 0 keys (keeps the reference translation), 1 (constant) or FrameCount. */
	TArray<FVector3f> Translations;
};

struct MANTLEEDITOR_API FMantleAnimClipEventData
{
	float TimeSeconds = 0.0f;
	FString Name;
};

/** An emote as MAN1 holds it (backend cook/animclip.go). */
struct MANTLEEDITOR_API FMantleAnimClipData
{
	float FrameRate = 30.0f;
	int32 FrameCount = 0;
	TArray<FMantleAnimClipTrackData> Tracks;
	TArray<FMantleAnimClipEventData> Events;

	/**
	 * A place clip's skeleton (#469, MAN1 flag bit 0): the bone table of the mesh it was sampled over, parent-first,
	 * and its hash (FMantleSkeletonHash). Empty for an emote.
	 */
	FString SkeletonHash;
	TArray<FName> SkeletonBones;
	TArray<int32> SkeletonParents;

	/** (FrameCount - 1) / FrameRate. */
	double DurationSeconds() const;
};

/**
 * The plugin's MAN1 writer (A2.4 #283): samples an emote on the UEFN driver skeleton at 30 fps and
 * encodes it the way the cook worker re-encodes it (cook/animclip.go CookAnimClip: tracks in
 * skeleton order, constant tracks collapsed to one key, rotations normalised, events by time), so
 * the bytes a creator uploads are already the canonical ones.
 *
 * A clip on another rig has to be retargeted onto the UEFN mannequin first. #535: the export does it
 * for a MetaHuman, UE5 Mannequin or UE4 Mannequin clip (FMantleEmoteRetarget: IK Rig's batch retarget through the
 * plugin's reverse retargeters) and samples the retargeted copy here; for any other rig Validate
 * names the foreign bone and says to retarget it.
 */
struct MANTLEEDITOR_API FMantleAnimClipWriter
{
	/**
	 * MAN1 bytes. Tracks are ordered by Skeleton (bones it does not have go last, in their own order). A place clip (#469,
	 * SkeletonHash set) writes its skeleton block and orders its tracks by its own bone table instead.
	 */
	static TArray<uint8> Encode(const FMantleAnimClipData& Clip, const struct FMantleContractSkeleton* Skeleton = nullptr);

	/**
	 * #469: samples Sequence over the bones of Mesh (a place skeletal mesh on its own skeleton) at FrameRate, as the cook
	 * re-encodes a place clip: one track per bone of Mesh's raw reference skeleton that the sequence animates, a rotation
	 * that never leaves the bone's reference rotation and a translation that never leaves its reference translation left
	 * out (the bone keeps them anyway), no events, and Mesh's bone table and hash as the skeleton block. A sequence that
	 * moves nothing keeps one constant track on the root. False with OutError when the sequence has no data model or no
	 * frames, or the mesh no bones.
	 */
	static bool SampleOnMesh(const UAnimSequence* Sequence, const USkeletalMesh* Mesh, float FrameRate, FMantleAnimClipData& OutClip, FString& OutError);

	/** Collapses constant tracks, normalises rotations, sorts events, as the worker does before encoding. */
	static void Canonicalize(FMantleAnimClipData& Clip);

	/** Every rule of cook.ValidateAnimClip, with its messages. */
	static void Validate(const FMantleAnimClipData& Clip, const FMantleAvatarContract& Contract, TArray<FMantleCosmeticIssue>& OutIssues);

	/**
	 * Samples Sequence at the contract's frame rate: every bone track, interpolated between the
	 * source's keys; a translation track is dropped when it never leaves the bone's reference
	 * translation (0.01 cm), which the node keeps anyway; notifies named in the event allowlist
	 * become events. False with OutError when the sequence has no data model or no frames.
	 */
	static bool Sample(const UAnimSequence* Sequence, const FMantleAvatarContract& Contract, FMantleAnimClipData& OutClip, FString& OutError);

	/** 16-bit snorm as the worker quantizes it (round(clamp(v) * 32767)). */
	static int16 QuantizeSnorm16(float Value);
};
