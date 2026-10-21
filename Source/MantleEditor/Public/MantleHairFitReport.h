// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * #51: the plain words the hair head-fit preview shows, decided from what the fit measured. Pure: the export fills the
 * input, the panel shows the output. The creator never types a scale or an offset; they read these and press "Looks good"
 * or "Adjust" (which uploads the hair as authored, so they can place it by hand).
 */
/** The geometry behind the before/after preview, in the export's space (Unreal, cm): the hair as authored and as fitted, and the head it was fitted to. */
struct MANTLEEDITOR_API FMantleHairFitMeshes
{
	TArray<FVector3f> BeforePositions;
	TArray<FVector3f> AfterPositions;
	/** Triangles (three indices each) into the hair positions. */
	TArray<uint32> HairTriangles;
	TArray<FVector3f> HeadPositions;
	TArray<uint32> HeadTriangles;
	bool IsValid() const { return AfterPositions.Num() > 0 && BeforePositions.Num() == AfterPositions.Num() && HeadPositions.Num() > 0; }
};

struct MANTLEEDITOR_API FMantleHairFitReport
{
	/** Shown above the before/after picture whenever the fit ran. */
	static const TCHAR* Headline() { return TEXT("We sized your hair to fit the standard head"); }
	static const TCHAR* ConfirmLabel() { return TEXT("Looks good"); }
	static const TCHAR* AdjustLabel() { return TEXT("Adjust"); }

	/** The share of card roots within 5 mm of the skin (percent) under which the fit is called low confidence. */
	static constexpr double LowRootPercent = 50.0;
	/** The share of card vertices left inside the skin (percent) over which the fit is called low confidence. */
	static constexpr double LowInsidePercent = 2.0;

	struct FInput
	{
		/** The whole-piece fit found a size and position that keeps the hair outside the head. */
		bool bMeetsOutsideBar = true;
		/** The scalp fit asked the creator to check (it had to move a lot of hair a long way). */
		bool bScalpCheckFit = false;
		/** Cards only (no cap): the share of card roots within 5 mm of the skin after the fit, percent; negative = not measured. */
		double RootsWithin5mmAfter = -1.0;
		int32 CardVertices = 0;
		int32 CardInsideAfter = 0;
		bool bHadCap = false;
		/** The mesh has a section that reads like a head ("head", "face", "skin", "body"), baked into the hair file. */
		bool bBakedHead = false;
		FString BakedHeadSection;
	};

	struct FResult
	{
		FString Headline;
		bool bLowConfidence = false;
		bool bBakedHead = false;
		/** Plain sentences after the headline: the low-confidence and baked-head messages. */
		TArray<FString> Messages;
	};

	static FResult Describe(const FInput& Input);

	/** True when a material/section name reads like a head, not hair: head, face, skin or body, and not a cap. */
	static bool IsBakedHeadName(const FString& SectionName);
};
