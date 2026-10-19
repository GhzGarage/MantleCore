// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * How one layout of MetaHuman Creator's in-memory body (LOD1) is numbered against Mantle's reference body. Both are the same
 * body (same shape, vertex and triangle counts); Epic's "UE Optimized / Medium" build, which made the reference, only numbers
 * the vertices and lays out the UVs differently. Creator's bodies come in more than one layout, so maps are keyed by the preview
 * LOD's UV hash. Each point map is learned once from a converted character by the maintainer command
 * Mantle.Dev.LearnBodyLayoutMap and shipped as a plugin-only resource (Resources/BodyLayout), so the plugin can put a body made
 * in memory into the reference order without Epic's build.
 */
struct MANTLEEDITOR_API FMantleCreatorBodyMap
{
	/** UV hash (UMantleBodyTopologyData::ComputeUVHash, as ReadLOD reports it) of the preview LOD the map was learned on, and of the reference LOD. */
	uint32 SourceUVHash = 0;
	uint32 ReferenceUVHash = 0;
	/** For each reference vertex, the preview LOD's render vertex it is. A bijection over 0..N-1. */
	TArray<int32> PreviewOfReference;
	/** What the maintainer command saw; informational. */
	FString Source;
	FString VerifiedOn;
	double MaxErrorCm = -1.0;
	int32 FlippedTriangles = 0;
	FString Generated;

	/** Resources/BodyLayout/creator-preview-to-reference-v1.json in the plugin. */
	static FString DefaultPath();
	/** The shipped map for a preview LOD with this UV hash and vertex count; null when there is none (file missing, invalid, or another layout). */
	static const FMantleCreatorBodyMap* Find(uint32 UVHash, int32 VertexCount);

	bool Applies(uint32 UVHash, int32 VertexCount) const;

	static bool ParseFile(const FString& Json, TArray<FMantleCreatorBodyMap>& Out, FString& OutError);
	static FString ToJson(const TArray<FMantleCreatorBodyMap>& Maps);
};
