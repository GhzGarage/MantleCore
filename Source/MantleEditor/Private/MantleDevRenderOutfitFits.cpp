// Copyright Joshua Eger. All Rights Reserved.
//
// Maintainer command: Mantle.Dev.RenderOutfitFits <Outfit> <Directory> [female|male|auto] [slot] [size]
// Fits an Outfit onto the standard bodies exactly as CheckCosmetic does (FMantleEditorActions::MakeCosmeticDraft, then
// FMantleParametricOutfit::Prepare per fit body) and renders each fit as the catalog thumbnail does (FMantleCosmeticThumbnail,
// GarmentSubject: the engine's thumbnail scene, reference pose, framed on the garment, on that size's own body and face), from
// the front and from the back, into <Directory>/<Outfit>_<size id>_{front,back}.png. Nothing is saved or uploaded. Logs
// MANTLE_RENDER_FITS lines.
#include "MantleAvatarContract.h"
#include "MantleCosmeticExport.h"
#include "MantleCosmeticThumbnail.h"
#include "MantleEditorActions.h"
#include "MantleParametricOutfit.h"
#include "MantlePng.h"

#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogMantleRenderOutfitFits, Log, All);

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleDevRenderOutfitFitsPrivate
{
	void Run(const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogMantleRenderOutfitFits, Error, TEXT("MANTLE_RENDER_FITS usage: Mantle.Dev.RenderOutfitFits <Outfit> <Directory> [female|male|auto] [slot] [size]"));
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
		FString WhyNot;
		if (!Outfit || !Contract || !FMantleParametricOutfit::IsSource(Outfit) || (Args.Num() > 2 && !FMantleEditorActions::ParseFitSex(Args[2], Sex))
			|| !FMantleCosmeticThumbnail::CanCapture(&WhyNot))
		{
			UE_LOG(LogMantleRenderOutfitFits, Error, TEXT("MANTLE_RENDER_FITS failed: %s is not an Outfit, there is no contract, the sex is wrong, or nothing can render (%s)"), *Path, *WhyNot);
			return;
		}
		const FString Directory = Args[1];
		const FString Slot = Args.Num() > 3 ? Args[3] : FString(TEXT("top"));
		const int32 Size = Args.Num() > 4 ? FMath::Clamp(FCString::Atoi(*Args[4]), 128, 2048) : 1024;
		const FMantleCosmeticDraft Draft = FMantleEditorActions::MakeCosmeticDraft(EMantleCosmeticClass::Garment, Outfit, Slot, FString(), *Contract, Sex);
		UE_LOG(LogMantleRenderOutfitFits, Display, TEXT("MANTLE_RENDER_FITS %s: %d fit bodies (%s)"), *Outfit->GetPathName(), Draft.OutfitTargets.Num(), *Draft.FitBodiesStatus);
		for (const FMantleOutfitAuthoringTarget& Target : Draft.OutfitTargets)
		{
			// The fit CheckCosmetic makes for this body (FMantleCosmeticExport::Build's per-source draft).
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
			if (!Garment)
			{
				UE_LOG(LogMantleRenderOutfitFits, Error, TEXT("MANTLE_RENDER_FITS %s FAILED: %s"), *Target.Id, *Error);
				continue;
			}
			// The catalog thumbnail's subject: the garment on this size's own body and face.
			FMantleThumbnailSubject Subject = FMantleCosmeticThumbnail::GarmentSubject(Garment, Target.Body.Get(), Target.Face.Get());
			for (const TCHAR* View : { TEXT("front"), TEXT("back") })
			{
				FMantleThumbnailSubject Shot = Subject;
				if (FCString::Strcmp(View, TEXT("back")) == 0)
				{
					Shot.View.Yaw += 180.0f;
				}
				TArray<FColor> Pixels;
				const FString File = FPaths::Combine(Directory, FString::Printf(TEXT("%s_%s_%s.png"), *Outfit->GetName(), *Target.Id, View));
				const bool bWritten = FMantleCosmeticThumbnail::Capture(Shot, Size, Pixels, Error)
					&& FFileHelper::SaveArrayToFile(FMantlePng::EncodeRGBA8(Size, Size, Pixels), *File);
				UE_LOG(LogMantleRenderOutfitFits, Display, TEXT("MANTLE_RENDER_FITS %s %s %s %s"), *Target.Id, View, bWritten ? *File : TEXT("FAILED"), bWritten ? TEXT("") : *Error);
			}
		}
		UE_LOG(LogMantleRenderOutfitFits, Display, TEXT("MANTLE_RENDER_FITS done"));
	}

	FAutoConsoleCommand GMantleDevRenderOutfitFitsCommand(
		TEXT("Mantle.Dev.RenderOutfitFits"),
		TEXT("Mantle.Dev.RenderOutfitFits <Outfit> <Directory> [female|male|auto] [slot] [size] -- fits the Outfit onto the standard bodies as CheckCosmetic does and renders ")
		TEXT("each as the catalog thumbnail does, front and back, into Directory. Saves and uploads nothing."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
