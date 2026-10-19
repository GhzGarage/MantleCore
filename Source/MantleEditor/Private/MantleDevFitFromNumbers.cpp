// Copyright Joshua Eger. All Rights Reserved.
//
// Dev spike: Mantle.Dev.FitFromNumbers <OutfitAssetPath> [aera+bo] [source=preset|numbers]
// Proves (or disproves) that a MetaHuman Outfit can be resized onto fit bodies made IN MEMORY, with no converted Aera/Bo
// assets in the project and no Epic sign-in. Test only: upload behaviour is untouched.
#include "MantleAvatarContract.h"
#include "MantleCosmeticExport.h"
#include "MantleFitBodies.h"
#include "MantleParametricOutfit.h"

#include "ChaosOutfitAsset/OutfitAsset.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogMantleFitTest, Log, All);

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleDevFitFromNumbersPrivate
{
	FString OneLine(FString Text)
	{
		Text.ReplaceInline(TEXT("\r"), TEXT(" "));
		Text.ReplaceInline(TEXT("\n"), TEXT(" "));
		return Text;
	}

	/** What one resize attempt reported; Character stays registered until the caller releases it. */
	struct FAttempt
	{
		bool bMade = false;
		bool bResized = false;
		int32 Triangles = 0;
		int32 BodyVerts = 0;
		FString Error;
		FString Note;
	};

	FAttempt TryOne(const FMantleFitBodyDefinition& Definition, const EMantleFitBodySource Source, const EMantleFitBodyOuter Outer,
		UChaosOutfitAsset& Outfit, const FMantleAvatarContract& Contract, FMantleFitBody& Body)
	{
		FAttempt Attempt;
		if (!FMantleFitBodies::Make(Definition, Source, Outer, Body, Attempt.Error))
		{
			return Attempt;
		}
		Attempt.bMade = true;
		TArray<FVector3f> BodyVertices;
		if (FMantleFitBodies::ReadVertices(Body.BodyMesh.Get(), 0, BodyVertices))
		{
			Attempt.BodyVerts = BodyVertices.Num();
		}

		FMantleCosmeticDraft Draft;
		Draft.Class = EMantleCosmeticClass::Garment;
		Draft.Slot = TEXT("top");
		Draft.Source = &Outfit;
		Draft.OutfitAuthoringCharacter = Body.Character.Get();
		Draft.bOutfitAuthoringInMemory = true;
		FMantleCosmeticDraft Prepared;
		TArray<TStrongObjectPtr<UObject>> KeepAlive;
		FString Note;
		if (!FMantleParametricOutfit::Prepare(Draft, Contract, Prepared, KeepAlive, Attempt.Error, &Note))
		{
			return Attempt;
		}
		const USkeletalMesh* Resized = Cast<USkeletalMesh>(Prepared.Source.Get());
		if (!Resized)
		{
			Attempt.Error = TEXT("the resize returned no skeletal mesh");
			return Attempt;
		}
		Attempt.bResized = true;
		Attempt.Triangles = FMantleFitBodies::CountTriangles(Resized);
		Attempt.Note = Note;
		return Attempt;
	}

	void CompareWithConverted(const FMantleFitBodyDefinition& Definition, const FMantleFitBody& Body, const FMantleAvatarContract& Contract)
	{
		const FString Name = Definition.PresetName;
		if (Name.IsEmpty())
		{
			return;
		}
		const FString Package = FString::Printf(TEXT("/Game/MetaHumans/%s/Converted/%s/Unpacked/%s/Body/SKM_%s_BodyMesh"), *Name, *Name, *Name, *Name);
		if (!FPackageName::DoesPackageExist(Package))
		{
			UE_LOG(LogMantleFitTest, Display, TEXT("MANTLE_FIT_TEST compare body=%s skipped=no-converted-body (%s)"), *Definition.Id, *Package);
			return;
		}
		const USkeletalMesh* Converted = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s.SKM_%s_BodyMesh"), *Package, *Name));
		int32 ConvertedLod = Converted ? FMantleCosmeticExport::FindReferenceLOD(Converted, Contract) : INDEX_NONE;
		if (ConvertedLod == INDEX_NONE)
		{
			ConvertedLod = 0;
		}
		TArray<FVector3f> ConvertedVertices, MemoryVertices;
		if (!Converted || !FMantleFitBodies::ReadVertices(Converted, ConvertedLod, ConvertedVertices)
			|| !FMantleFitBodies::ReadVertices(Body.BodyMesh.Get(), 0, MemoryVertices))
		{
			UE_LOG(LogMantleFitTest, Display, TEXT("MANTLE_FIT_TEST compare body=%s skipped=vertices-unreadable"), *Definition.Id);
			return;
		}
		float Average = 0.0f, Largest = 0.0f;
		if (!FMantleFitBodies::CompareNearest(ConvertedVertices, MemoryVertices, Average, Largest))
		{
			UE_LOG(LogMantleFitTest, Display, TEXT("MANTLE_FIT_TEST compare body=%s skipped=empty"), *Definition.Id);
			return;
		}
		UE_LOG(LogMantleFitTest, Display, TEXT("MANTLE_FIT_TEST compare body=%s avgCm=%.3f maxCm=%.3f"), *Definition.Id, Average, Largest);
		// How each side was read, so a difference in layout is visible rather than hidden in the numbers.
		UE_LOG(LogMantleFitTest, Display,
			TEXT("MANTLE_FIT_TEST compare-detail body=%s converted=%s convertedLod=%d convertedVerts=%d memoryLod=0 memoryVerts=%d metric=nearest-vertex-of-converted-in-memory"),
			*Definition.Id, *Package, ConvertedLod, ConvertedVertices.Num(), MemoryVertices.Num());
	}

	void Run(const TArray<FString>& Args)
	{
		FString OutfitPath;
		TArray<FString> Ids;
		EMantleFitBodySource Source = EMantleFitBodySource::Numbers;
		for (const FString& Arg : Args)
		{
			if (Arg.StartsWith(TEXT("source="), ESearchCase::IgnoreCase))
			{
				Source = Arg.EndsWith(TEXT("preset"), ESearchCase::IgnoreCase) ? EMantleFitBodySource::Preset : EMantleFitBodySource::Numbers;
			}
			else if (OutfitPath.IsEmpty())
			{
				OutfitPath = Arg;
			}
			else
			{
				// -ExecCmds splits on commas, so "aera+bo" and "aera;bo" are accepted as well as "aera,bo".
				TArray<FString> Parts;
				static const TCHAR* const Delimiters[] = { TEXT(","), TEXT("+"), TEXT(";") };
				Arg.ParseIntoArray(Parts, Delimiters, UE_ARRAY_COUNT(Delimiters), true);
				Ids.Append(Parts);
			}
		}
		const auto Finish = [](const TCHAR* Why)
		{
			if (Why)
			{
				UE_LOG(LogMantleFitTest, Error, TEXT("MANTLE_FIT_TEST error=%s"), Why);
			}
			UE_LOG(LogMantleFitTest, Display, TEXT("MANTLE_FIT_TEST done"));
		};
		if (OutfitPath.IsEmpty())
		{
			Finish(TEXT("usage: Mantle.Dev.FitFromNumbers <OutfitAssetPath> [aera+bo] [source=preset|numbers]"));
			return;
		}
		if (!OutfitPath.Contains(TEXT(".")))
		{
			OutfitPath = FString::Printf(TEXT("%s.%s"), *OutfitPath, *FPackageName::GetShortName(OutfitPath));
		}
		FString ContractError;
		const FMantleAvatarContract* Contract = FMantleAvatarContract::Get(&ContractError);
		if (!Contract)
		{
			Finish(*OneLine(FString::Printf(TEXT("the avatar contract is unavailable: %s"), *ContractError)));
			return;
		}
		TStrongObjectPtr<UChaosOutfitAsset> Outfit(LoadObject<UChaosOutfitAsset>(nullptr, *OutfitPath));
		if (!Outfit.IsValid())
		{
			Finish(*OneLine(FString::Printf(TEXT("no MetaHuman Outfit Asset at %s"), *OutfitPath)));
			return;
		}
		const TArray<FMantleFitBodyDefinition> BuiltIn = FMantleFitBodies::BuiltIn();
		TArray<FMantleFitBodyDefinition> Chosen;
		if (Ids.IsEmpty())
		{
			Chosen = BuiltIn;
		}
		for (const FString& Id : Ids)
		{
			const FMantleFitBodyDefinition* Found = BuiltIn.FindByPredicate([&Id](const FMantleFitBodyDefinition& Candidate) { return Candidate.Id.Equals(Id, ESearchCase::IgnoreCase); });
			if (Found)
			{
				Chosen.Add(*Found);
			}
			else
			{
				UE_LOG(LogMantleFitTest, Error, TEXT("MANTLE_FIT_TEST body=%s ok=false character=none resized=false triangles=0 bodyVerts=0 heightCm=0 ms=0 error=unknown fit body id (aera, bo)"), *Id);
			}
		}

		for (FMantleFitBodyDefinition& Definition : Chosen)
		{
			const double Started = FPlatformTime::Seconds();
			FMantleFitBody Body;
			FAttempt Result;
			const TCHAR* CharacterKind = TEXT("none");
			// The transient package first; the unsaved /Temp package only if the transient character cannot be made or resized.
			for (const EMantleFitBodyOuter Outer : { EMantleFitBodyOuter::Transient, EMantleFitBodyOuter::TempPackage })
			{
				Result = TryOne(Definition, Source, Outer, *Outfit.Get(), *Contract, Body);
				if (Result.bMade)
				{
					CharacterKind = Outer == EMantleFitBodyOuter::Transient ? TEXT("transient") : TEXT("temp");
				}
				if (Result.bResized)
				{
					break;
				}
				UE_LOG(LogMantleFitTest, Display, TEXT("MANTLE_FIT_TEST attempt body=%s character=%s made=%s resized=false error=%s"), *Definition.Id,
					Outer == EMantleFitBodyOuter::Transient ? TEXT("transient") : TEXT("temp"), Result.bMade ? TEXT("true") : TEXT("false"), *OneLine(Result.Error));
				if (Result.bMade)
				{
					CompareWithConverted(Definition, Body, *Contract);
				}
				FMantleFitBodies::Release(Body);
			}
			const double Milliseconds = (FPlatformTime::Seconds() - Started) * 1000.0;
			UE_LOG(LogMantleFitTest, Display, TEXT("MANTLE_FIT_TEST detail body=%s source=%s fixedBodyType=%d heightSource=%s note=%s"), *Definition.Id,
				Source == EMantleFitBodySource::Preset ? TEXT("preset") : TEXT("numbers"), Body.bFixedBodyType ? 1 : 0, *Body.HeightSource, *OneLine(Result.Note));
			UE_LOG(LogMantleFitTest, Display,
				TEXT("MANTLE_FIT_TEST body=%s ok=%s character=%s resized=%s triangles=%d bodyVerts=%d heightCm=%.2f ms=%.0f error=%s"),
				*Definition.Id, Result.bResized ? TEXT("true") : TEXT("false"), CharacterKind,
				Result.bResized ? TEXT("true") : TEXT("false"), Result.Triangles, Result.BodyVerts, Body.HeightCm, Milliseconds, *OneLine(Result.Error));
			if (Result.bResized)
			{
				CompareWithConverted(Definition, Body, *Contract);
			}
			FMantleFitBodies::Release(Body);
		}
		// Drops the unsaved characters (and a /Temp package if the fallback was used).
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		Finish(nullptr);
	}

	FAutoConsoleCommand GMantleDevFitFromNumbersCommand(
		TEXT("Mantle.Dev.FitFromNumbers"),
		TEXT("Mantle.Dev.FitFromNumbers <OutfitAssetPath> [aera+bo] [source=preset|numbers] -- dev test: makes the Aera and Bo fit bodies in memory ")
		TEXT("(no converted MetaHumans, no Epic sign-in), resizes the MetaHuman Outfit onto each and logs one MANTLE_FIT_TEST line per body, then MANTLE_FIT_TEST done. ")
		TEXT("Use + between ids (-ExecCmds splits on commas). Blocks the editor until it ends."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
