// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MantleOutfitSections.h"
#include "Templates/SharedPointer.h"

class FMantleAvatarContract;
class FMantlePlacePublishSession;
class UObject;
class UWorld;
enum class EMantleCosmeticClass : uint8;
struct FMantleCosmeticDraft;

/**
 * The rows of the Cosmetics panel's Sections list for a garment from a skeletal mesh (#483): the
 * material slots LOD 0 draws, each row's material slot index, and the ticks the chosen slot starts with.
 */
struct MANTLEEDITOR_API FMantleCosmeticOutfitRows
{
	TArray<FMantleOutfitSection> Sections;
	TArray<int32> Slots;
	TArray<bool> Keep;
};

/**
 * The Tools > Mantle actions whose state lives in this module's private classes (the pasted
 * sign-in, the Mantle Publish panel), for the Mantle toolset for AI agents (MantleAgentTools,
 * #434), and the Cosmetics panel's draft defaults, which the panel and the toolset share so the
 * two cannot drift. Deliberately narrow: each call is what the menu does, without its dialogs,
 * and nothing here returns or accepts a token.
 */
struct MANTLEEDITOR_API FMantleEditorActions
{
	/** FMantleCreatorSignIn::IsSignedIn: a request would carry a creator token (pasted, or MANTLE_AUTH_TOKEN). */
	static bool IsSignedIn();

	/**
	 * What Publish (bGoLive) and Save to Mantle run once signed in: the background publish under
	 * the Mantle Publish panel (SMantlePublishPanel::Start). While one runs it only shows the panel.
	 * Callers check World and FMantleApiClient::IsConfigured first: Start answers those with a dialog.
	 */
	static void StartPublish(UWorld* World, bool bGoLive, const FString& Notes);

	/** The publish running now, or the last one this editor ran; null before the first. */
	static TSharedPtr<FMantlePlacePublishSession> GetPublishSession();

	/** True while a publish or save runs. */
	static bool IsPublishRunning();

	/**
	 * The draft Tools > Mantle > Cosmetics... holds once the creator has picked Class, Slot, Source
	 * and Name, every other setting at the panel's default: a fresh draft with the Apply* defaults
	 * below applied in the panel's order. An empty Name takes the asset's, as the panel does.
	 */
	static FMantleCosmeticDraft MakeCosmeticDraft(EMantleCosmeticClass Class, UObject* Source, const FString& Slot,
		const FString& Name, const FMantleAvatarContract& Contract);

	/**
	 * A newly picked source (the panel's SetSource): the body a garment or hair was authored on,
	 * found near it when none is chosen yet (needs Contract), and the asset's name when Name is empty.
	 */
	static void ApplyCosmeticSourceDefaults(FMantleCosmeticDraft& Draft, const FMantleAvatarContract* Contract);

	/**
	 * Hair's source details: a groom's default cards entry, and for a skeletal mesh its first
	 * section named for lashes when no section is chosen yet. Nothing for other classes.
	 */
	static void ApplyCosmeticSectionDefaults(FMantleCosmeticDraft& Draft);

	/** Avatars' socket overrides at the body's own sockets (needs Contract); cleared for every other class. */
	static void ApplyCosmeticSocketDefaults(FMantleCosmeticDraft& Draft, const FMantleAvatarContract* Contract);

	/**
	 * #483, garments from a skeletal mesh: KeepSlots for the draft's slot (FMantleOutfitSections::InitialKeep,
	 * through CosmeticKeepSlots); cleared otherwise. Returns the rows it read, empty when it cleared.
	 */
	static FMantleCosmeticOutfitRows ApplyCosmeticOutfitDefaults(FMantleCosmeticDraft& Draft);

	/**
	 * Clothing from a MetaHuman Outfit with no fit sources yet: Mantle's standard fit bodies, Epic's Aera and Bo presets
	 * converted into the project (the converter's copy /Game/MetaHumans/<Name>/Converted/<Name>/<Name> and its build), as OutfitTargets "aera" and
	 * "bo", so every upload carries a size for slim and broad wearers. Each is added only when its character, converted
	 * body and face are all in the project; a missing one is skipped.
	 */
	static void ApplyCosmeticFitTargetDefaults(FMantleCosmeticDraft& Draft);

	/**
	 * #594, avatars: the jaw picker. On a body whose skeleton has the jaw slot (the UE5, UE4 or UEFN Mannequin, as
	 * FMantleCosmeticExport::AvatarSkeletonId identifies it; needs Contract), OutCandidates gets the body's bones below
	 * head (FMantleCosmeticChecks::JawCandidates) and Draft.JawBone the one whose name has "jaw" in it
	 * (FMantleCosmeticChecks::PreselectJawBone), else none; returns true. On any other body (a MetaHuman's jaw is on its
	 * face) or class, no candidates, Draft.JawBone none, and false: the panel hides the picker.
	 */
	static bool ApplyCosmeticJawDefaults(FMantleCosmeticDraft& Draft, const FMantleAvatarContract* Contract, TArray<FName>& OutCandidates);

	/**
	 * #595, avatars: the Secondary motion section. On a body whose skeleton has the secondary motion slots (the UE5, UE4 or
	 * UEFN Mannequin, as FMantleCosmeticExport::AvatarSkeletonId identifies it; needs Contract), OutCandidates gets the body's
	 * extra bones a chain may start at (FMantleCosmeticChecks::SecondaryCandidates, less the jaw pick) and
	 * Draft.SecondaryMotion the suggested mappings (SuggestSecondaryMappings); returns true. Run after
	 * ApplyCosmeticJawDefaults. On any other body or class, none of either, and false: the panel hides the section.
	 */
	static bool ApplyCosmeticSecondaryDefaults(FMantleCosmeticDraft& Draft, const FMantleAvatarContract* Contract, TArray<FName>& OutCandidates);

	/** Avatars' section roles from MetaHuman's slot names at the body's upload LOD (needs Contract); unchanged otherwise. */
	static void ApplyCosmeticRoleDefaults(FMantleCosmeticDraft& Draft, const FMantleAvatarContract* Contract);

	/** KeepSlots from ticks by row: unset when every row is ticked (the whole mesh), else the ticked rows' Slots. */
	static TOptional<TArray<int32>> CosmeticKeepSlots(TConstArrayView<bool> Keep, TConstArrayView<int32> Slots);
};
