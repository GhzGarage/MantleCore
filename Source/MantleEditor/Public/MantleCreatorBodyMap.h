// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * How MetaHuman Creator's in-memory preview body (LOD1) is laid out against Mantle's reference body. Both are the same body
 * (same shape, vertex and triangle counts); Epic's "UE Optimized / Medium" build, which made the reference, only numbers the
 * vertices and lays out the UVs differently. The point map is learned once from a converted character by the maintainer
 * command Mantle.Dev.LearnBodyLayoutMap and shipped as a plugin-only resource (Resources/BodyLayout), so the plugin can put a
 * standard body made in memory into the reference order without Epic's build.
 */
struct MANTLEEDITOR_API FMantleCreatorBodyMap
{
	/** UV hash (UMantleBodyTopologyData::ComputeUVHash) of the preview LOD the map was learned on, and of the reference LOD. */
	uint32 SourceUVHash = 0;
	uint32 ReferenceUVHash = 0;
	/** For each reference vertex, the preview LOD's render vertex it is. A bijection over 0..N-1. */
	TArray<int32> PreviewOfReference;
	/** What the maintainer command measured; informational. */
	double MaxErrorCmAera = -1.0;
	double MaxErrorCmBo = -1.0;
	FString Generated;

	/** Resources/BodyLayout/creator-preview-to-reference-v1.json in the plugin. */
	static FString DefaultPath();
	/** The shipped map, read once; null when the file is missing or invalid. */
	static const FMantleCreatorBodyMap* Get();

	/** True when a preview LOD with this UV hash and vertex count is the layout the map was learned on. */
	bool Applies(uint32 UVHash, int32 VertexCount) const;

	static bool Parse(const FString& Json, FMantleCreatorBodyMap& Out, FString& OutError);
	FString ToJson() const;
};
