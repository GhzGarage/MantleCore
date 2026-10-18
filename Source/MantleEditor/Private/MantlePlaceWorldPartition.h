// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

struct FMantleParseError;
struct FMantleValidationIssue;
class UWorld;

/**
 * World Partition levels (#450). Check and Export walk the level's loaded actors, and in a World
 * Partition level an actor in a cell the editor has not loaded is not in the world at all, so it
 * would be missing from the place without a word. Mantle refuses such a level instead, naming World
 * Partition and how to load the rest, rather than loading every cell itself: a large world may not
 * fit in memory at once, and Check must not change what the creator has loaded.
 */
class FMantlePlaceWorldPartition
{
public:
	/** One actor descriptor, reduced to what decides whether its absence loses content. */
	struct FActorEntry
	{
		FString Label;
		bool bLoaded = false;
		/** Runtime-only actors (HLODs among them) never exist in the editor and are never exported. */
		bool bRuntimeOnly = false;
		/** HLOD proxies are generated from the level's own actors, not placed content. */
		bool bHLOD = false;
	};

	/** How many examples a message names before it stops listing. */
	static constexpr int32 MaxNamedExamples = 5;

	/**
	 * The actors among Entries whose absence would lose content: the count, and up to
	 * MaxNamedExamples labels in a stable (sorted) order.
	 */
	static int32 CountUnloaded(const TArray<FActorEntry>& Entries, TArray<FString>& OutExamples);

	/** The actor descriptors of World's World Partition; empty for a level without one, or a game world. */
	static TArray<FActorEntry> GatherEntries(UWorld* World);

	/** The creator-facing problem sentence for Count unloaded actors. */
	static FString DescribeProblem(int32 Count, const TArray<FString>& Examples);

	/** The remedy that goes with DescribeProblem. */
	static FString DescribeRemedy();

	/** Check's half: an error on the place when World has unloaded actors. */
	static void CheckLevel(UWorld* World, TArray<FMantleValidationIssue>& OutIssues);

	/** Export's half: the same refusal as an export error, so no path publishes a partial place. */
	static void CheckExport(UWorld* World, TArray<FMantleParseError>& OutErrors);
};
