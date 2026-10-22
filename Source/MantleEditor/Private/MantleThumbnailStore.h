// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MantleThumbnailChoice.h"

/**
 * Where a creator's thumbnail choice (#124) is kept for a source that is not an asset of ours (a mesh, an animation, a groom): a small ini in the
 * project's Saved/Config, one section per source and class, like the accessory placement. A re-upload then keeps the choice. Never alters the source.
 */
struct FMantleThumbnailStore
{
	static FString DefaultConfigFile();

	/** The section name for a source object path and class name ("Garment", "Emote"...). */
	static FString SectionFor(const FString& SourcePath, const FString& ClassName);

	/** Fills OutChoice from ConfigFile and returns true when this source and class have a saved choice; OutChoice is left Automatic otherwise. */
	static bool Load(const FString& SourcePath, const FString& ClassName, FMantleThumbnailChoice& OutChoice, const FString& ConfigFile);

	/** Saves Choice (an Automatic choice removes the entry). */
	static void Save(const FString& SourcePath, const FString& ClassName, const FMantleThumbnailChoice& Choice, const FString& ConfigFile);
};
