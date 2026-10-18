// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MantleCosmeticExport.h"

/** Creator corrections, keyed by source object path and accessory slot; never alter the source mesh. */
struct FMantleAccessorySettings
{
	static FString DefaultConfigFile();
	/** Returns whether this exact source/slot has saved settings, including an explicitly disabled correction. */
	static bool Load(FMantleCosmeticDraft& Draft, const FString& ConfigFile);
	static void Save(const FMantleCosmeticDraft& Draft, const FString& ConfigFile);
	/** Align standard Unreal component-space mesh axes to this avatar's neutral anchor, preserving location and scale. */
	static bool AlignToAvatarAxes(FMantleCosmeticDraft& Draft, const USkeletalMesh* Body, FString& OutError);
};
