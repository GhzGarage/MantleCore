// Copyright Joshua Eger. All Rights Reserved.
//
// Maintainer command: Mantle.Dev.SaveOutfitFits <Outfit> <Folder> [female|male|auto] [slot]
// Fits an Outfit onto the standard bodies exactly as CheckCosmetic does (FMantleEditorActions::MakeCosmeticDraft, then
// FMantleParametricOutfit::Prepare per fit body), and saves each resized garment and each body's merged body and head
// under Folder, so the fit can be rendered (MantleAvatars' render_many.py). Nothing is uploaded. Logs MANTLE_SAVE_FITS lines.
#include "MantleAvatarContract.h"
#include "MantleCosmeticExport.h"
#include "MantleEditorActions.h"
#include "MantleFitBodies.h"
#include "MantleGarmentOutfit.h"
#include "MantleParametricOutfit.h"

#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogMantleSaveOutfitFits, Log, All);

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleDevSaveOutfitFitsPrivate
{
	void Run(const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogMantleSaveOutfitFits, Error, TEXT("MANTLE_SAVE_FITS usage: Mantle.Dev.SaveOutfitFits <Outfit> <Folder> [female|male|auto] [slot]"));
			return;
		}
		FString Path = Args[0];
		if (!FPackageName::GetShortName(Path).Contains(TEXT(".")))
		{
			Path += TEXT(".") + FPackageName::GetShortName(Path);
		}
		UObject* Outfit = LoadObject<UObject>(nullptr, *Path);
		const FMantleAvatarContract* Contract = FMantleAvatarContract::Get();
		EMantleFitSex Sex = EMantleFitSex::Auto;
		if (!Outfit || !Contract || !FMantleParametricOutfit::IsSource(Outfit) || (Args.Num() > 2 && !FMantleEditorActions::ParseFitSex(Args[2], Sex)))
		{
			UE_LOG(LogMantleSaveOutfitFits, Error, TEXT("MANTLE_SAVE_FITS failed: %s is not an Outfit (or no contract, or a bad sex)"), *Path);
			return;
		}
		const FString Folder = Args[1];
		const FString Slot = Args.Num() > 3 ? Args[3] : FString(TEXT("top"));
		const FMantleCosmeticDraft Draft = FMantleEditorActions::MakeCosmeticDraft(EMantleCosmeticClass::Garment, Outfit, Slot, FString(), *Contract, Sex);
		UE_LOG(LogMantleSaveOutfitFits, Display, TEXT("MANTLE_SAVE_FITS %s: %d fit bodies (%s)"), *Outfit->GetPathName(), Draft.OutfitTargets.Num(), *Draft.FitBodiesStatus);
		for (const FMantleOutfitAuthoringTarget& Target : Draft.OutfitTargets)
		{
			FMantleCosmeticDraft Input = Draft;
			Input.OutfitTargets.Reset();
			Input.OutfitSourceSize.Reset();
			Input.OutfitAuthoringCharacter = Target.Character;
			Input.bOutfitAuthoringInMemory = Target.bInMemory;
			Input.AuthoringBody = Target.Body;
			Input.AuthoringFace = Target.Face;
			FMantleCosmeticDraft Prepared;
			TArray<TStrongObjectPtr<UObject>> KeepAlive;
			FString Error;
			USkeletalMesh* Garment = FMantleParametricOutfit::Prepare(Input, *Contract, Prepared, KeepAlive, Error) ? Cast<USkeletalMesh>(Prepared.Source.Get()) : nullptr;
			USkeletalMesh* Saved = Garment ? FMantleGarmentOutfit::SaveCopy(*Garment, Folder, FString::Printf(TEXT("SK_%s_%s"), *Target.Id, *Outfit->GetName()), Error) : nullptr;
			UE_LOG(LogMantleSaveOutfitFits, Display, TEXT("MANTLE_SAVE_FITS garment %s %s %s"), *Target.Id, Saved ? *Saved->GetPathName() : TEXT("FAILED"), *Error);
			const FMantleFitBody* Body = Draft.FitBodies.IsValid()
				? Draft.FitBodies->Bodies.FindByPredicate([&Target](const FMantleFitBody& Candidate) { return Candidate.Id == Target.Id; }) : nullptr;
			if (Body && Body->MergedHeadAndBodyMesh.IsValid())
			{
				Error.Reset();
				USkeletalMesh* SavedBody = FMantleGarmentOutfit::SaveCopy(*Body->MergedHeadAndBodyMesh.Get(), Folder, FString::Printf(TEXT("SK_%s_Body"), *Target.Id), Error);
				UE_LOG(LogMantleSaveOutfitFits, Display, TEXT("MANTLE_SAVE_FITS body %s %s %s"), *Target.Id, SavedBody ? *SavedBody->GetPathName() : TEXT("FAILED"), *Error);
			}
		}
		UE_LOG(LogMantleSaveOutfitFits, Display, TEXT("MANTLE_SAVE_FITS done"));
	}

	FAutoConsoleCommand GMantleDevSaveOutfitFitsCommand(
		TEXT("Mantle.Dev.SaveOutfitFits"),
		TEXT("Mantle.Dev.SaveOutfitFits <Outfit> <Folder> [female|male|auto] [slot] -- fits the Outfit onto the standard bodies as CheckCosmetic does and saves each resized ")
		TEXT("garment and body under Folder, for rendering. Uploads nothing."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
