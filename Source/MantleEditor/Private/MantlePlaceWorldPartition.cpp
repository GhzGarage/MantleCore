// Copyright Joshua Eger. All Rights Reserved.
#include "MantlePlaceWorldPartition.h"
#include "MantlePlaceTypes.h"
#include "MantlePlaceValidator.h"
#include "MantleRuntime.h"

#include "Engine/World.h"
#include "EngineDefines.h"
#include "WorldPartition/WorldPartition.h"
#include "HAL/PlatformMemory.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#if WITH_EDITOR
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#endif

int32 FMantlePlaceWorldPartition::CountUnloaded(const TArray<FActorEntry>& Entries, TArray<FString>& OutExamples)
{
	OutExamples.Reset();
	TArray<FString> Labels;
	for (const FActorEntry& Entry : Entries)
	{
		if (Entry.bLoaded || Entry.bRuntimeOnly || Entry.bHLOD)
		{
			continue;
		}
		Labels.Add(Entry.Label);
	}

	Labels.Sort();
	for (int32 Index = 0; Index < Labels.Num() && Index < MaxNamedExamples; ++Index)
	{
		OutExamples.Add(Labels[Index]);
	}
	return Labels.Num();
}

TArray<FMantlePlaceWorldPartition::FActorEntry> FMantlePlaceWorldPartition::GatherEntries(UWorld* World)
{
	TArray<FActorEntry> Entries;
#if WITH_EDITOR
	// A game (PIE) world streams its cells from generated data rather than the editor's actor
	// descriptors; Check and Export only ever read the editor's level.
	if (!IsValid(World) || World->IsGameWorld())
	{
		return Entries;
	}

	UWorldPartition* WorldPartition = World->GetWorldPartition();
	if (!WorldPartition || !WorldPartition->IsInitialized())
	{
		return Entries;
	}

	FWorldPartitionHelpers::ForEachActorDescInstance(WorldPartition,
		[&Entries](const FWorldPartitionActorDescInstance* Desc)
		{
			if (Desc)
			{
				const UClass* NativeClass = Desc->GetActorNativeClass();
				FActorEntry Entry;
				Entry.Label = Desc->GetActorLabelOrName().ToString();
				Entry.bLoaded = Desc->IsLoaded();
				Entry.bRuntimeOnly = Desc->GetActorIsRuntimeOnly();
				Entry.bHLOD = NativeClass && NativeClass->IsChildOf(AWorldPartitionHLOD::StaticClass());
				Entries.Add(MoveTemp(Entry));
			}
			return true;
		});
#endif
	return Entries;
}

FString FMantlePlaceWorldPartition::DescribeProblem(int32 Count, const TArray<FString>& Examples)
{
	FString Named;
	if (Examples.Num() > 0)
	{
		Named = FString::Printf(TEXT(" (%s%s)"),
			*FString::Join(Examples, TEXT(", ")),
			Count > Examples.Num() ? TEXT(", and others") : TEXT(""));
	}
	return FString::Printf(
		TEXT("This is a World Partition level, and %d actor%s in cells the editor has not loaded%s would be left out of the place."),
		Count,
		Count == 1 ? TEXT("") : TEXT("s"),
		*Named);
}

FString FMantlePlaceWorldPartition::DescribeRemedy()
{
	return TEXT("Publish loads every cell itself. To check now, load the whole level: in the World Partition window select every cell and choose Load Region From Selection, or turn off Enable Streaming in World Settings.");
}

void FMantlePlaceWorldPartition::CheckLevel(UWorld* World, TArray<FMantleValidationIssue>& OutIssues)
{
	TArray<FString> Examples;
	const int32 Count = CountUnloaded(GatherEntries(World), Examples);
	if (Count == 0)
	{
		return;
	}

	OutIssues.Add(FMantleValidationIssue{
		EMantleIssueSeverity::Error,
		FString(),
		DescribeProblem(Count, Examples),
		DescribeRemedy() });
}

void FMantlePlaceWorldPartition::CheckExport(UWorld* World, TArray<FMantleParseError>& OutErrors)
{
	TArray<FString> Examples;
	const int32 Count = CountUnloaded(GatherEntries(World), Examples);
	if (Count == 0)
	{
		return;
	}

	UE_LOG(LogMantle, Warning, TEXT("Export refused: %d World Partition actor(s) are not loaded."), Count);
	OutErrors.Add(FMantleParseError{
		TEXT("/worldPartition"),
		DescribeProblem(Count, Examples) + TEXT(" ") + DescribeRemedy() });
}

FMantlePlaceWorldPartition::FLoadResult FMantlePlaceWorldPartition::LoadAll(UWorld* World)
{
	FLoadResult Result;
#if WITH_EDITOR
	if (!IsValid(World) || World->IsGameWorld())
	{
		return Result;
	}
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	if (!WorldPartition || !WorldPartition->IsInitialized())
	{
		return Result;
	}
	TArray<FString> Examples;
	Result.UnloadedBefore = CountUnloaded(GatherEntries(World), Examples);
	if (Result.UnloadedBefore == 0)
	{
		return Result;
	}

	Result.CommitBeforeMB = static_cast<int64>(FPlatformMemory::GetStats().UsedVirtual >> 20);
	UE_LOG(LogMantle, Log, TEXT("World Partition: loading %d actors in unloaded cells for the publish (committed memory %lld MB)"),
		Result.UnloadedBefore, Result.CommitBeforeMB);
	// The same adapter the World Partition window's Load Region makes, over the whole world.
	UWorldPartitionEditorLoaderAdapter* Adapter = WorldPartition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(
		World, FBox(FVector(-HALF_WORLD_MAX), FVector(HALF_WORLD_MAX)), TEXT("Mantle publish"));
	Adapter->GetLoaderAdapter()->Load();
	Result.Adapter = Adapter;

	Result.UnloadedAfter = CountUnloaded(GatherEntries(World), Examples);
	Result.CommitAfterMB = static_cast<int64>(FPlatformMemory::GetStats().UsedVirtual >> 20);
	UE_LOG(LogMantle, Log, TEXT("World Partition: %s"), *DescribeLoad(Result));
#endif
	return Result;
}

void FMantlePlaceWorldPartition::ReleaseLoaded(UWorld* World, FLoadResult& Result)
{
#if WITH_EDITOR
	UWorldPartitionEditorLoaderAdapter* Adapter = Cast<UWorldPartitionEditorLoaderAdapter>(Result.Adapter.Get());
	Result.Adapter.Reset();
	if (!Adapter)
	{
		return;
	}
	if (IsValid(World))
	{
		if (UWorldPartition* WorldPartition = World->GetWorldPartition())
		{
			WorldPartition->ReleaseEditorLoaderAdapter(Adapter);
			UE_LOG(LogMantle, Log, TEXT("World Partition: let go of the cells the publish loaded"));
		}
	}
#endif
}

FString FMantlePlaceWorldPartition::DescribeLoad(const FLoadResult& Result)
{
	FString Text = FString::Printf(TEXT("Loaded %d World Partition actors in cells that were not loaded, so they publish with the rest (committed memory %lld MB, was %lld MB)."),
		Result.Loaded(), Result.CommitAfterMB, Result.CommitBeforeMB);
	if (Result.UnloadedAfter > 0)
	{
		Text += FString::Printf(TEXT(" %d actors are still not loaded (a data layer that is off, or a cell that would not load)."), Result.UnloadedAfter);
	}
	return Text;
}
