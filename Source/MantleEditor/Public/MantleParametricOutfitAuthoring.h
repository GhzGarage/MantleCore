// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UChaosOutfitAsset;
class USkeletalMesh;

/** One authored representation of the same garment, paired with the body/head it actually fits. */
struct MANTLEEDITOR_API FMantleOutfitAuthoredSource
{
	FString Id;
	TWeakObjectPtr<USkeletalMesh> Garment;
	/** Epic's merged authoring body/head with its source MeshDescription, not a runtime body proxy. */
	TWeakObjectPtr<USkeletalMesh> CombinedBodyHead;
};

/** Render-only native Outfit authoring. Original meshes and their materials are read without modification. */
struct MANTLEEDITOR_API FMantleParametricOutfitAuthoring
{
	/**
	 * Wrap one or more authored skeletal garment sizes in native Cloth Assets and one sized Outfit.
	 * Generates Epic's source RBF data; it does not manufacture additional artist-authored sizes.
	 * The returned asset is input to the native resizing pipeline, not a runtime payload.
	 * KeepAlive must outlive the returned asset's use. A package Outer permits the caller to save it.
	 */
	static UChaosOutfitAsset* Create(TConstArrayView<FMantleOutfitAuthoredSource> Sources, UObject* Outer,
		FName Name, int32 InterpolationPoints, TArray<TStrongObjectPtr<UObject>>& KeepAlive, FString& Error);
};
