// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class FMantleAvatarContract;
class UObject;
class UAnimSequence;
class USkeletalMesh;
class USkeleton;
struct FMantleAnimClipData;

/**
 * #535: an emote made on a MetaHuman, UE5 Mannequin or (#593) UE4 Mannequin body, retargeted onto the UEFN mannequin
 * when it is uploaded. Emotes play on the hidden UEFN driver and every body follows it through the client's retargeters,
 * so the cook takes UEFN bones only (backend/internal/cook/animclip.go). Here, at upload, a clip whose skeleton is the contract's
 * metahuman, ue5mannequin or ue4mannequin skeleton goes through the plugin's reverse retargeter
 * (RTG_Mantle_MetaHuman_to_UEFN, RTG_Mantle_UE5Mannequin_to_UEFN and RTG_Mantle_UE4Mannequin_to_UEFN, which ship in the
 * plugin content) with IK Rig's batch retarget into TransientFolder, is sampled as
 * MAN1 exactly like a UEFN clip, and the temporary copy is deleted. Nothing is saved and the creator's clip is untouched.
 * #609: a clip made on a UEFN Mannequin body needs no reverse retargeter: its skeleton is the emote skeleton itself (uefn,
 * tried first by IdentifySkeleton), so it is sampled as it is, the identity the reverse of RTG_UEFN_to_Mantle_UEFNMannequin
 * would be, and plays on every body through their forward retargeters.
 *
 * UIKRetargetBatchOperation::RunBatchRetarget is BlueprintCallable, so it is called by reflection (its UFunction, its
 * input struct's properties by name), the way the avatar preview reaches IK Rig's retarget node: MantleEditor takes no
 * module dependency on IKRigEditor. The target is the project's UEFN mannequin when it has one, else the one the plugin
 * builds in memory from its own IK rig (FMantleBuiltInMannequin::UEFNMannequin), so no mannequin has to be added. Game thread.
 */
struct FMantleEmoteRetarget
{
	/**
	 * Where the retargeted copy lives while it is sampled: a folder under /Game (the asset tools create nothing under
	 * /Temp), in memory only, never saved; everything the retarget made there is dropped again right after.
	 */
	static const TCHAR* const TransientFolder;

	/**
	 * The plugin's reverse retargeter for an appearance skeleton id, an object path: metahuman, ue5mannequin and
	 * ue4mannequin (case-sensitive). Null for any other id.
	 */
	static const TCHAR* RetargeterPathFor(const FString& SkeletonId);

	/**
	 * Which contract skeleton Skeleton is, by its bones and their parents (FMantleCosmeticChecks::MatchSkeleton): the
	 * emote skeleton (uefn: no retarget) first, then metahuman, ue5mannequin and ue4mannequin. Empty when it is none of them.
	 */
	static FString IdentifySkeleton(const USkeleton* Skeleton, const FMantleAvatarContract& Contract);

	/**
	 * A mesh on Skeleton to retarget from (its proportions): the skeleton's preview mesh, else a skeletal mesh in the
	 * project on that skeleton, a "Body" first, then by package path. Null when there is none.
	 */
	static USkeletalMesh* FindSourceMesh(USkeleton* Skeleton);

	/**
	 * Retargets Sequence (on SkeletonId's skeleton, played on SourceMesh) onto TargetMesh (a UEFN mannequin) through
	 * RetargeterPathFor(SkeletonId), samples the copy with FMantleAnimClipWriter::Sample, and deletes the copy. False with
	 * a sentence for the creator when anything is missing or the retarget does not give one animation on the target's
	 * skeleton with Sequence's length (within a frame).
	 */
	static bool RetargetAndSample(UAnimSequence* Sequence, const FString& SkeletonId, USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh,
		const FMantleAvatarContract& Contract, FMantleAnimClipData& OutClip, FString& OutError);

	/**
	 * The retarget itself, through any retargeter: IK Rig's batch retarget of Sequence (played on SourceMesh) onto
	 * TargetMesh through Retargeter into TransientFolder, checked to be one animation on TargetMesh's skeleton whose length
	 * is Sequence's within LengthTolerance seconds, handed to Use while it exists, then deleted. False with a sentence when
	 * the retarget fails or Use does (Use sets OutUseError, which becomes OutError). RetargetAndSample runs through it.
	 */
	static bool RetargetThrough(UAnimSequence* Sequence, USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh, UObject* Retargeter,
		double LengthTolerance, TFunctionRef<bool(UAnimSequence& Retargeted, FString& OutUseError)> Use, FString& OutError);

	/**
	 * #584: the body the clip plays on while it is retargeted (its proportions shape the result), first of: the
	 * sequence's own preview mesh, PickedBody (the panel's "Animated on" body), FindSourceMesh(its skeleton); each only
	 * when it is on the sequence's skeleton. OutFrom, when given, says which one it was, for the notes.
	 */
	static USkeletalMesh* ChooseSourceMesh(UAnimSequence* Sequence, USkeletalMesh* PickedBody, FString* OutFrom = nullptr);

	/**
	 * RetargetAndSample from ChooseSourceMesh(Sequence, PickedBody) onto FMantleBuiltInMannequin::UEFNMannequin.
	 * OutNote says what was done, for the package's notes.
	 */
	static bool Sample(UAnimSequence* Sequence, const FString& SkeletonId, const FMantleAvatarContract& Contract, FMantleAnimClipData& OutClip,
		FString& OutError, FString& OutNote, USkeletalMesh* PickedBody = nullptr);
};
