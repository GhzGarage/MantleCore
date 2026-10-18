// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MantleSkinnedMesh.h"

struct FMantleGarmentClearance;

/** What TransferSkinWeights did (the skin fields of the fit stats). */
struct MANTLERUNTIME_API FMantleSkinTransferStats
{
	bool bRan = false;

	/** Vertices weighted from the wearer's body at their authored closest triangle ... */
	int32 FromBody = 0;

	/** ... untracked ones that copied their nearest tracked vertex's new weights ... */
	int32 FromNearest = 0;

	/** ... and ones that kept the garment's own weights (no tracked vertex at all). */
	int32 Kept = 0;

	/** Influences kept per vertex (the garment's slot count, at most MaxInfluences). */
	int32 Influences = 0;

	/** Body influences dropped because the garment's bone table has no bone of that name. */
	int32 DroppedInfluences = 0;

	/** Per changed vertex, the largest per-bone |new - old| weight (0..1): mean and max; changed vertices whose dominant bone moved. */
	double MeanChange = 0.0;
	double MaxChange = 0.0;
	int32 DominantChanged = 0;
	double Ms = 0.0;
};

/** What SmoothSkinWeights did. */
struct MANTLERUNTIME_API FMantleSkinSmoothStats
{
	bool bRan = false;
	int32 Iterations = 0;

	/** Welded points, the split copies beyond the first, points faded by their clearance, and of those the ones not smoothed at all. */
	int32 Points = 0;
	int32 SplitCopies = 0;
	int32 FadedPoints = 0;
	int32 ExactPoints = 0;

	double MeanChange = 0.0;
	double MaxChange = 0.0;
	int32 DominantChanged = 0;
	double Ms = 0.0;
};

/**
 * The wearer's skin weights transferred onto a fitted garment, so fabric and skin follow the same bones
 * once the pose leaves the reference pose, as Roblox layered clothing does. Weights are MSK1's 8 bits
 * summing to exactly 255.
 *
 * Holds no UObjects; may run on any thread.
 */
struct MANTLERUNTIME_API FMantleGarmentSkinTransfer
{
	/**
	 * Influences kept per vertex, at most (fewer when the garment has fewer slots). 12, the MetaHuman
	 * body's slot count (the blend of three corners' influences has more bones; cut to 8 it dropped weight
	 * the skin still has).
	 */
	static constexpr int32 MaxInfluences = 12;

	/** Smoothing passes, and the authored clearances the smoothing fades between (none at or below Tight, full at or above Loose). */
	static constexpr int32 DefaultSmoothIters = 3;
	static constexpr int32 MaxSmoothIters = 50;
	static constexpr float DefaultSmoothTightCm = 1.0f;
	static constexpr float DefaultSmoothLooseCm = 3.0f;

	/**
	 * Garment (LOD with GarmentBones as its bone table) takes the wearer's weights at the authored
	 * correspondence (Authored: the garment's clearance over its source body, whose vertex order Wearer's
	 * LOD0 shares):
	 *   - tracked vertex: the (bone, weight) lists of the three wearer vertices of its authored closest
	 *     triangle blended with its barycentrics; the top N kept (N = the garment's slot count, at most
	 *     MaxInfluences), renormalised and quantised to sum exactly 255;
	 *   - untracked vertex: its nearest tracked vertex's new weights (fitted positions); none at all: kept.
	 * Wearer bones map to GarmentBones by name (identity when the tables match); influences on bones the
	 * garment lacks are dropped and counted. False (Garment unchanged), with the reason in OutError, on
	 * mismatched inputs.
	 */
	static bool Transfer(FMantleSkinnedMeshLOD& Garment, TConstArrayView<FMantleSkinnedMeshBone> GarmentBones, const FMantleGarmentClearance& Authored,
		const FMantleSkinnedMesh& Wearer, FMantleSkinTransferStats* OutStats = nullptr, FString* OutError = nullptr);

	/**
	 * The per-vertex smoothing factor from the authored clearance: 0 at or below TightCm (fabric lying
	 * on the skin keeps the skin's exact weights), 1 at or above LooseCm, linear between; untracked vertices 1.
	 */
	static void SmoothFactor(const FMantleGarmentClearance& Authored, float TightCm, float LooseCm, TArray<float>& Out);

	/**
	 * Laplacian smoothing of the skin weights over the welded points' edge adjacency (split copies end
	 * with identical weights; sections are never smoothed into each other). Each pass over the union of
	 * bones: w'(p) = (1 - 0.5 f) w(p) + 0.5 f * the neighbours' mean, the top N kept and renormalised; f is
	 * the point's mean Factor (1 everywhere when Factor is null). Iterations <= 0 changes nothing. False
	 * (Garment unchanged) on bad skin data.
	 */
	static bool Smooth(FMantleSkinnedMeshLOD& Garment, int32 Iterations, const TArray<float>* Factor = nullptr, FMantleSkinSmoothStats* OutStats = nullptr);

	/**
	 * The dominant-bone agreement check: over Garment's tracked vertices, how many have
	 * the dominant bone (by name) of Wearer's LOD0 vertex at the corner of their authored closest triangle
	 * with the largest barycentric. False when the sizes do not match.
	 */
	static bool DominantMatch(const FMantleSkinnedMeshLOD& Garment, TConstArrayView<FMantleSkinnedMeshBone> GarmentBones, const FMantleGarmentClearance& Authored,
		const FMantleSkinnedMesh& Wearer, int32& OutMatches, int32& OutTracked);
};
