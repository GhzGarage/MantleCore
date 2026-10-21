// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MantleHairFitReport.h"

/**
 * #51: the whole upload-time head fit of bought hair cards, in one call: split the cap from the cards (FMantleHairCapSplit),
 * size and seat the piece on the head (FMantleHairPieceFit, stage 1), settle it on the scalp with the cap's lower edge
 * wrapped (FMantleScalpFit, stage 2), and word the result for the preview (FMantleHairFitReport). Positions only; topology,
 * UVs and normals are untouched. A failure leaves the positions as they were and says so in Warnings. Pure: no UObjects.
 */
struct MANTLEEDITOR_API FMantleHairCardsFit
{
	struct FInput
	{
		/** The head's skin triangles, in cm. */
		TConstArrayView<FVector3f> HeadPositions;
		TConstArrayView<uint32> HeadIndices;

		/** The hair mesh: one position array, a triangle list, each triangle's section and the section names. */
		TConstArrayView<FVector3f> HairPositions;
		TConstArrayView<uint32> HairIndices;
		TConstArrayView<int32> TriangleSections;
		TConstArrayView<FString> SectionNames;
	};

	struct FResult
	{
		/** The fitted positions (same count as the input); the input's when the fit did not run. */
		TArray<FVector3f> Positions;
		bool bFitted = false;
		bool bCapFound = false;
		FMantleHairFitReport::FResult Report;
		/** Lines for the package's notes and warnings the creator should read (a failure sentence each). */
		TArray<FString> Notes;
		TArray<FString> Warnings;
	};

	static FResult Run(const FInput& Input);
};
