// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * The target head's skin as a cloud of oriented surface samples (#766 stage 1). Ported from the prototype's head_samples
 * (hair-trial pack_prep.py): random points on every triangle, each with its triangle's normal, then all normals flipped
 * once if the top quarter of the head faces inward. The whole-piece fit scores thousands of candidates against it, so
 * the query is a nearest sample (a k-d tree), not the exact distance the stage-2 head surface gives; a sample is never
 * farther than a fraction of a millimetre from the true surface, which is far below the fit's 1 cm grid.
 *
 * Space is the caller's, in centimetres. Pure: no UObjects.
 */
class MANTLEEDITOR_API FMantleHairHeadSamples
{
public:
	/** Samples per triangle at most; fewer when the head has more than about 20,000 triangles. */
	static constexpr int32 MaxSamplesPerTriangle = 12;

	bool Build(TConstArrayView<FVector3f> Positions, TConstArrayView<uint32> Indices, FString& OutError);
	bool IsValid() const { return Points.Num() > 0; }
	int32 Num() const { return Points.Num(); }

	/** The highest vertex of the head, cm. The whole-piece fit lines the hair's top up with it. */
	double TopCm() const { return Top; }

	/** The nearest sample to P: its id and the squared distance. */
	void Nearest(const FVector& P, int32& OutId, double& OutDistSquared) const;
	const FVector& Point(int32 Id) const { return Points[Id]; }
	/** Unit, pointing out of the skin. */
	const FVector& Normal(int32 Id) const { return Normals[Id]; }

private:
	void BuildRange(int32 Lo, int32 Hi, int32 Axis);
	void NearestRange(int32 Lo, int32 Hi, int32 Axis, const FVector& P, int32& BestId, double& BestD2) const;

	TArray<FVector> Points;
	TArray<FVector> Normals;
	/** Implicit tree: the median of each [Lo, Hi) range sits at its middle. */
	TArray<int32> Order;
	double Top = 0.0;
};

/**
 * #766 stage 1: the upload-time whole-piece fit of bought hair. Bought hair is made for a different head, at a different
 * scale, so before the stage-2 scalp fit (FMantleScalpFit) the whole piece gets one similarity transform onto the
 * MetaHuman head: a uniform scale plus an offset. Ported from the proven prototype (hair-trial pack_prep.py: the unit
 * normalisation in main() and fit()):
 *
 *  1. Units. The piece is first scaled about the origin so its X width is ReferenceWidthCm (18 cm, about a head's width).
 *  2. Search. A grid over a further uniform scale (0.800 to 1.425 in steps of 0.025), a Y offset (-6 to 6 cm in 1 cm steps)
 *     and a top offset (-12 to 10 cm in 1 cm steps). The Z offset is not free: it lines the piece's highest point up with
 *     the head's top plus the top offset. X is not offset (hair is centred on the head's plane of symmetry).
 *     Up to MaxFitPoints of the piece's points score each candidate.
 *  3. Score. The share of points outside the skin (the side of the nearest head sample's normal) must reach
 *     MinOutsideFraction; among those the candidate whose 25th percentile distance to the head is smallest wins, so hair
 *     hugs the skull without sinking into it. If none reaches the bar, the one with the largest outside share wins and
 *     bMeetsOutsideBar is false.
 *
 * The scale about the origin and the unit scale are the prototype's, so the transform is
 * P' = P * (UnitScale * Scale) + (0, OffsetY, OffsetZ).
 */
class MANTLEEDITOR_API FMantleHairPieceFit
{
public:
	/** The piece's width along X is scaled to this before the search, cm. */
	static constexpr double ReferenceWidthCm = 18.0;
	/** Candidates need at least this share of points outside the skin to rank by gap. */
	static constexpr double MinOutsideFraction = 0.97;
	/** The most points that score a candidate (an even stride over the piece). */
	static constexpr int32 MaxFitPoints = 2000;

	/** The candidate grid. The defaults are the prototype's; tests pass small ones. */
	struct FSearch
	{
		TArray<double> Scales;
		TArray<double> OffsetsY;
		TArray<double> TopOffsets;

		static FSearch Default();
	};

	struct FResult
	{
		/** ReferenceWidthCm over the piece's X width. */
		double UnitScale = 1.0;
		/** The searched scale, to 4 decimals. */
		double Scale = 1.0;
		double OffsetY = 0.0;
		/** To 2 decimals. */
		double OffsetZ = 0.0;
		double TopOffset = 0.0;

		/** At the chosen candidate: share of the scored points outside the skin. */
		double OutsideFraction = 0.0;
		bool bMeetsOutsideBar = false;
		int32 CandidatesScored = 0;

		double TotalScale() const { return UnitScale * Scale; }
		FVector Apply(const FVector& P) const { return P * TotalScale() + FVector(0.0, OffsetY, OffsetZ); }
		FVector3f Apply(const FVector3f& P) const
		{
			const FVector Moved = Apply(FVector(P.X, P.Y, P.Z));
			return FVector3f(static_cast<float>(Moved.X), static_cast<float>(Moved.Y), static_cast<float>(Moved.Z));
		}
		/** One sentence for the preview's notes. */
		FString Describe() const;
	};

	static bool Fit(const FMantleHairHeadSamples& Head, TConstArrayView<FVector3f> Points, const FSearch& Search, FResult& Out, FString& Error);
	static bool Fit(const FMantleHairHeadSamples& Head, TConstArrayView<FVector3f> Points, FResult& Out, FString& Error);

	/** Moves every point by the result's transform. */
	static void ApplyTo(const FResult& Fit, TArray<FVector3f>& Points);
};
