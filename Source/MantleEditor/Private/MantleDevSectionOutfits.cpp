// Copyright Joshua Eger. All Rights Reserved.
//
// Maintainer command: Mantle.Dev.SectionOutfits <Body> <Directory> [TurnOff keys, comma-separated]
// #55 step 2 end to end, as the Cosmetics panel runs it: Convert <Body> (the bare-skin line confirmed, the Triangles, Atlas and Bake fixes
// off unless TurnOff says otherwise), then the clothing in the body's own material sections is cut into pieces
// (FMantleGarmentOutfit::MakeSectionMeshes) and made into MetaHuman Outfits on the MetaHuman Convert made (MakeFromWork); each Outfit is
// then drawn as the catalog thumbnail does (Mantle.Dev.RenderOutfitFits) into <Directory>. Needs a renderer. Saves what Convert and the
// Outfits write under <body folder>/Converted; uploads nothing. Logs MANTLE_SECTION_OUTFITS lines.
#include "MantleAvatarConverter.h"
#include "MantleAvatarContract.h"
#include "MantleGarmentOutfit.h"
#include "MantleMetaHumanPose.h"

#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogMantleSectionOutfits, Log, All);

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleDevSectionOutfitsPrivate
{
	void Run(const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogMantleSectionOutfits, Error, TEXT("MANTLE_SECTION_OUTFITS usage: Mantle.Dev.SectionOutfits <Body> <Directory> [TurnOff]"));
			return;
		}
		FString ContractError;
		const FMantleAvatarContract* Contract = FMantleAvatarContract::Get(&ContractError);
		if (!Contract)
		{
			UE_LOG(LogMantleSectionOutfits, Error, TEXT("MANTLE_SECTION_OUTFITS failed: no contract (%s)"), *ContractError);
			return;
		}
		FMantleConvertOptions Options;
		FMantleConvertContext Context;
		FString Error;
		if (!FMantleAvatarConverter::MakeContextForPath(Args[0], Options, *Contract, Context, Error))
		{
			UE_LOG(LogMantleSectionOutfits, Error, TEXT("MANTLE_SECTION_OUTFITS failed: %s"), *Error);
			return;
		}
		TArray<FString> Off;
		(Args.Num() > 2 ? Args[2] : FString(TEXT("Triangles.0.Mesh,Atlas.0.0,Bake.0.Mesh"))).ParseIntoArray(Off, TEXT(","), true);
		TArray<FMantleConvertFix> Fixes = FMantleAvatarConverter::Analyze(Context);
		const TArray<FString> Keys = FMantleAvatarConverter::FixKeys(Fixes);
		for (int32 Index = 0; Index < Fixes.Num(); ++Index)
		{
			if (Off.Contains(Keys[Index]))
			{
				Fixes[Index].bEnabled = false;
			}
			if (Keys[Index].EndsWith(TEXT(".BareSkin")))
			{
				Fixes[Index].bEnabled = true;
			}
		}
		const FMantleConvertResult Result = FMantleAvatarConverter::Convert(Context, Fixes, Options, *Contract);
		UE_LOG(LogMantleSectionOutfits, Display, TEXT("MANTLE_SECTION_OUTFITS convert: %d applied, %d failed"), Result.Applied.Num(), Result.Failed.Num());
		for (const FMantleConvertOutcome& Failed : Result.Failed)
		{
			UE_LOG(LogMantleSectionOutfits, Display, TEXT("MANTLE_SECTION_OUTFITS convert failed %s: %s"), *Failed.Key, *Failed.Reason);
		}
		const FMantleConvertMetaHumanWork* Work = Context.MetaHuman.Find(0);
		const FString Why = FMantleGarmentOutfit::WhyNoOutfits(Context, 0, /*bSectionPieces*/ true);
		if (!Work || !Why.IsEmpty())
		{
			UE_LOG(LogMantleSectionOutfits, Display, TEXT("MANTLE_SECTION_OUTFITS no Outfits: %s"), Work ? *Why : TEXT("Convert made no MetaHuman"));
			return;
		}
		const int32 Cut = FMantleGarmentOutfit::MakeSectionMeshes(Context);
		TArray<int32> Indices;
		TArray<USkeletalMesh*> Meshes;
		for (int32 Index = 0; Index < Context.Garments.Num(); ++Index)
		{
			const FMantleConvertGarment& Garment = Context.Garments[Index];
			if (Garment.Sections.Num() > 0 && Garment.Mesh.IsValid() && Garment.Error.IsEmpty())
			{
				Indices.Add(Index);
				Meshes.Add(Garment.Mesh.Get());
			}
			else if (Garment.Sections.Num() > 0)
			{
				UE_LOG(LogMantleSectionOutfits, Display, TEXT("MANTLE_SECTION_OUTFITS piece %s not cut: %s"), *Garment.Name, *Garment.Error);
			}
		}
		UE_LOG(LogMantleSectionOutfits, Display, TEXT("MANTLE_SECTION_OUTFITS cut %d pieces"), Cut);
		if (Meshes.Num() == 0)
		{
			return;
		}
		const FMantleConvertPart& Avatar = Context.Parts[0];
		const FString BodyName = Avatar.Original.IsValid() ? Avatar.Original->GetName() : Avatar.Name;
		const FString Folder = Context.OutputFolder / (FMantleMetaHumanSteps::AssetTag(BodyName) + TEXT("_Outfit"));
		FMantleGarmentOutfitResult Made;
		FString Code;
		FString Refusal;
		const bool bMade = FMantleGarmentOutfit::MakeFromWork(*Work, BodyName, Folder, Meshes, Made, Code, Refusal);
		UE_LOG(LogMantleSectionOutfits, Display, TEXT("MANTLE_SECTION_OUTFITS Outfits on %s: %s (body gap %.2f cm)"), *Made.Character, bMade ? TEXT("made") : *Refusal, Made.BodyGapCm);
		for (int32 Piece = 0; Piece < Indices.Num(); ++Piece)
		{
			const FMantleConvertGarment& Garment = Context.Garments[Indices[Piece]];
			const FMantleGarmentOutfitPiece* Result2 = bMade && Made.Pieces.IsValidIndex(Piece) ? &Made.Pieces[Piece] : nullptr;
			UE_LOG(LogMantleSectionOutfits, Display, TEXT("MANTLE_SECTION_OUTFITS %s (%s): outfit=%s error=%s far=%d mean=%.2f cm"), *Garment.Name, *Garment.Slot,
				Result2 ? *Result2->Outfit : TEXT(""), Result2 ? *Result2->Error : *Refusal, Result2 ? Result2->FarVertices : 0, Result2 ? Result2->MeanSkinCm : 0.0);
			if (Result2 && !Result2->Outfit.IsEmpty() && GEngine)
			{
				GEngine->Exec(nullptr, *FString::Printf(TEXT("Mantle.Dev.RenderOutfitFits %s %s auto"), *Result2->Outfit, *Args[1]));
			}
		}
		UE_LOG(LogMantleSectionOutfits, Display, TEXT("MANTLE_SECTION_OUTFITS done"));
	}

	FAutoConsoleCommand GMantleDevSectionOutfitsCommand(
		TEXT("Mantle.Dev.SectionOutfits"),
		TEXT("Mantle.Dev.SectionOutfits <Body> <Directory> [TurnOff] -- Convert the body, cut the clothing in its own sections into pieces, make them MetaHuman Outfits and draw each as the catalog thumbnail does"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
