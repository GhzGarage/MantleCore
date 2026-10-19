// Copyright Joshua Eger. All Rights Reserved.
//
// Maintainer command: Mantle.Dev.LearnBodyLayoutMap [out=<path>]
// Learns, once, how MetaHuman Creator's in-memory body (LOD1) is numbered against Mantle's reference body (the output of Epic's
// UE Optimized / Medium build), from a converted Aera, checks the same map on a converted Bo, and writes it to the plugin-only
// resource Resources/BodyLayout/creator-preview-to-reference-v1.json (FMantleCreatorBodyMap). Nothing is uploaded or changed.
#include "MantleAvatarContract.h"
#include "MantleBodyTopologyData.h"
#include "MantleCosmeticExport.h"
#include "MantleCreatorBodyMap.h"

#include "Algo/Sort.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogMantleLayoutMap, Log, All);

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleDevLearnBodyLayoutMapPrivate
{
	/** One LOD as the reference check sees it: render vertices, their UVs and the triangle indices. */
	struct FSide
	{
		TArray<FVector3f> Positions;
		TArray<FVector2f> UVs;
		TArray<uint32> Indices;
		FMantleBodyLODTopology Topology;
	};

	bool ReadSide(const USkeletalMesh* Mesh, const int32 LOD, FSide& Out, FString& OutError)
	{
		if (!UMantleBodyTopologyData::ReadLOD(Mesh, LOD, Out.Topology, &Out.Positions, &Out.UVs, &OutError))
		{
			return false;
		}
		const FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering();
		if (!RenderData || !RenderData->LODRenderData.IsValidIndex(LOD))
		{
			OutError = TEXT("no render data");
			return false;
		}
		RenderData->LODRenderData[LOD].MultiSizeIndexContainer.GetIndexBuffer(Out.Indices);
		return true;
	}

	/** A converted character: Creator's in-memory preview body (LOD1) and the built reference-topology body. */
	struct FPair
	{
		FString Name;
		TStrongObjectPtr<UMetaHumanCharacter> Character;
		TStrongObjectPtr<USkeletalMesh> PreviewBody;
		bool bAdded = false;
		FSide Preview;
		FSide Built;
		int32 PreviewLOD = 1;
		int32 BuiltLOD = INDEX_NONE;
	};

	void ReleasePair(FPair& Pair)
	{
		if (Pair.bAdded && Pair.Character.IsValid() && GEditor)
		{
			if (UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>())
			{
				Subsystem->RemoveObjectToEdit(Pair.Character.Get());
			}
		}
		Pair.bAdded = false;
		Pair.PreviewBody.Reset();
		Pair.Character.Reset();
	}

	bool LoadPair(const TCHAR* Name, const FMantleAvatarContract& Contract, FPair& Out, FString& OutError)
	{
		Out.Name = Name;
		const FString CharacterPackage = FString::Printf(TEXT("/Game/MetaHumans/%s/Converted/%s/%s"), Name, Name, Name);
		const FString BodyPackage = FString::Printf(TEXT("/Game/MetaHumans/%s/Converted/%s/Unpacked/%s/Body/SKM_%s_BodyMesh"), Name, Name, Name, Name);
		if (!FPackageName::DoesPackageExist(CharacterPackage) || !FPackageName::DoesPackageExist(BodyPackage))
		{
			OutError = FString::Printf(TEXT("%s or %s is not in the project"), *CharacterPackage, *BodyPackage);
			return false;
		}
		UMetaHumanCharacter* Character = LoadObject<UMetaHumanCharacter>(nullptr, *FString::Printf(TEXT("%s.%s"), *CharacterPackage, Name));
		const USkeletalMesh* Built = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s.SKM_%s_BodyMesh"), *BodyPackage, Name));
		UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>() : nullptr;
		if (!Character || !Built || !Subsystem)
		{
			OutError = FString::Printf(TEXT("%s: the character, its built body or MetaHuman Creator's subsystem could not be loaded"), Name);
			return false;
		}
		Out.Character.Reset(Character);
		if (!Subsystem->IsObjectAddedForEditing(Character))
		{
			if (!Subsystem->TryAddObjectToEdit(Character))
			{
				OutError = FString::Printf(TEXT("%s: MetaHuman Creator could not open the character"), Name);
				return false;
			}
			Out.bAdded = true;
		}
		FMetaHumanCharacterPreviewAssetOptions Options;
		FMetaHumanCharacterPreviewAssets Preview;
		if (!Subsystem->TryGetCharacterPreviewAssets(Character, Options, Preview) || !Preview.BodyMesh)
		{
			OutError = FString::Printf(TEXT("%s: MetaHuman Creator did not provide the preview body"), Name);
			return false;
		}
		Out.PreviewBody.Reset(Preview.BodyMesh.Get());
		FString Error;
		if (!ReadSide(Out.PreviewBody.Get(), Out.PreviewLOD, Out.Preview, Error))
		{
			OutError = FString::Printf(TEXT("%s: the preview body's LOD%d could not be read: %s"), Name, Out.PreviewLOD, *Error);
			return false;
		}
		Out.BuiltLOD = FMantleCosmeticExport::FindReferenceLOD(Built, Contract);
		if (Out.BuiltLOD == INDEX_NONE || !ReadSide(Built, Out.BuiltLOD, Out.Built, Error))
		{
			OutError = FString::Printf(TEXT("%s: the built body has no readable LOD with the reference topology (%s)"), Name, *Error);
			return false;
		}
		UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP %s preview LOD%d: %d verts, %d tris, %d sections, uvhash %08x; built LOD%d: %d verts, %d tris, uvhash %08x"),
			Name, Out.PreviewLOD, Out.Preview.Topology.VertexCount, Out.Preview.Indices.Num() / 3, Out.Preview.Topology.SectionCount, Out.Preview.Topology.UVHash,
			Out.BuiltLOD, Out.Built.Topology.VertexCount, Out.Built.Indices.Num() / 3, Out.Built.Topology.UVHash);
		return true;
	}

	FVector3f Mean(const TArray<FVector3f>& Points)
	{
		FVector3d Sum = FVector3d::ZeroVector;
		for (const FVector3f& Point : Points) { Sum += FVector3d(Point); }
		return Points.IsEmpty() ? FVector3f::ZeroVector : FVector3f(Sum / Points.Num());
	}

	/** Reference vertex i -> preview vertex, one-to-one, by position after removing the whole-body shift (the build may re-seat the body). */
	bool LearnMap(const FSide& Preview, const FSide& Built, FVector3f& OutShift, TArray<int32>& OutMap, float& OutMaxCm, FString& OutError)
	{
		const int32 Count = Built.Positions.Num();
		if (Preview.Positions.Num() != Count)
		{
			OutError = FString::Printf(TEXT("the preview body has %d vertices and the built body %d"), Preview.Positions.Num(), Count);
			return false;
		}
		OutShift = Mean(Built.Positions) - Mean(Preview.Positions);
		constexpr float Cell = 0.25f;
		constexpr int32 MaxRing = 8;
		const auto CellOf = [](const FVector3f& Point)
		{
			return FIntVector(FMath::FloorToInt32(Point.X / Cell), FMath::FloorToInt32(Point.Y / Cell), FMath::FloorToInt32(Point.Z / Cell));
		};
		TMap<FIntVector, TArray<int32>> Grid;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Grid.FindOrAdd(CellOf(Preview.Positions[Index])).Add(Index);
		}
		TBitArray<> Used(false, Count);
		OutMap.Init(INDEX_NONE, Count);
		OutMaxCm = 0.0f;
		for (int32 Reference = 0; Reference < Count; ++Reference)
		{
			const FVector3f Target = Built.Positions[Reference] - OutShift;
			const FIntVector Center = CellOf(Target);
			int32 Best = INDEX_NONE;
			float BestDistance = TNumericLimits<float>::Max();
			for (int32 Ring = 0; Ring <= MaxRing && !(Best != INDEX_NONE && BestDistance < (Ring - 1) * Cell); ++Ring)
			{
				for (int32 X = -Ring; X <= Ring; ++X)
				{
					for (int32 Y = -Ring; Y <= Ring; ++Y)
					{
						for (int32 Z = -Ring; Z <= Ring; ++Z)
						{
							if (FMath::Max3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z)) != Ring) { continue; }
							const TArray<int32>* Found = Grid.Find(FIntVector(Center.X + X, Center.Y + Y, Center.Z + Z));
							if (!Found) { continue; }
							for (const int32 Candidate : *Found)
							{
								const float Distance = FVector3f::Distance(Target, Preview.Positions[Candidate]);
								if (!Used[Candidate] && Distance < BestDistance) { Best = Candidate; BestDistance = Distance; }
							}
						}
					}
				}
			}
			if (Best == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("reference vertex %d has no unused preview vertex within %.1f cm"), Reference, MaxRing * Cell);
				return false;
			}
			Used[Best] = true;
			OutMap[Reference] = Best;
			OutMaxCm = FMath::Max(OutMaxCm, BestDistance);
		}
		return true;
	}

	/** Positions that coincide (UV seam copies) share a group, so the check is of the welded surface. */
	TArray<int32> WeldGroups(const TArray<FVector3f>& Points)
	{
		TMap<FIntVector, int32> Groups;
		TArray<int32> Out;
		for (const FVector3f& Point : Points)
		{
			const FIntVector Key(FMath::RoundToInt32(Point.X * 100.0f), FMath::RoundToInt32(Point.Y * 100.0f), FMath::RoundToInt32(Point.Z * 100.0f));
			const int32 Next = Groups.Num();
			Out.Add(Groups.FindOrAdd(Key, Next));
		}
		return Out;
	}

	/** Every built triangle must land on a preview triangle once seam copies are welded. Returns the number that do not. */
	int32 CountBrokenTriangles(const FSide& Preview, const FSide& Built, const TArray<int32>& Map, int32& OutInconsistentGroups)
	{
		const TArray<int32> PreviewGroup = WeldGroups(Preview.Positions);
		const TArray<int32> BuiltGroup = WeldGroups(Built.Positions);
		TMap<int32, int32> PreviewOfBuiltGroup;
		OutInconsistentGroups = 0;
		for (int32 Reference = 0; Reference < Map.Num(); ++Reference)
		{
			const int32 Wanted = PreviewGroup[Map[Reference]];
			const int32 Target = PreviewOfBuiltGroup.FindOrAdd(BuiltGroup[Reference], Wanted);
			if (Target != Wanted) { ++OutInconsistentGroups; }
		}
		const auto Key = [](const int32 A, const int32 B, const int32 C)
		{
			int32 Sorted[3] = { A, B, C };
			Algo::Sort(Sorted);
			return FIntVector(Sorted[0], Sorted[1], Sorted[2]);
		};
		TSet<FIntVector> PreviewTriangles;
		for (int32 First = 0; First + 2 < Preview.Indices.Num(); First += 3)
		{
			PreviewTriangles.Add(Key(PreviewGroup[Preview.Indices[First]], PreviewGroup[Preview.Indices[First + 1]], PreviewGroup[Preview.Indices[First + 2]]));
		}
		int32 Broken = 0;
		for (int32 First = 0; First + 2 < Built.Indices.Num(); First += 3)
		{
			const int32* A = PreviewOfBuiltGroup.Find(BuiltGroup[Built.Indices[First]]);
			const int32* B = PreviewOfBuiltGroup.Find(BuiltGroup[Built.Indices[First + 1]]);
			const int32* C = PreviewOfBuiltGroup.Find(BuiltGroup[Built.Indices[First + 2]]);
			if (!A || !B || !C || !PreviewTriangles.Contains(Key(*A, *B, *C))) { ++Broken; }
		}
		return Broken;
	}

	/** The largest distance between a built vertex and the preview vertex the map names for it, after the best whole-body shift. */
	float MaxErrorWithMap(const FSide& Preview, const FSide& Built, const TArray<int32>& Map, FVector3f& OutShift)
	{
		TArray<FVector3f> Mapped;
		for (const int32 Index : Map) { Mapped.Add(Preview.Positions[Index]); }
		OutShift = Mean(Built.Positions) - Mean(Mapped);
		float Largest = 0.0f;
		for (int32 Reference = 0; Reference < Map.Num(); ++Reference)
		{
			Largest = FMath::Max(Largest, FVector3f::Distance(Built.Positions[Reference], Mapped[Reference] + OutShift));
		}
		return Largest;
	}

	void Run(const TArray<FString>& Args)
	{
		FString OutPath = FMantleCreatorBodyMap::DefaultPath();
		for (const FString& Arg : Args)
		{
			if (Arg.StartsWith(TEXT("out="), ESearchCase::IgnoreCase)) { OutPath = Arg.Mid(4); }
		}
		const auto Finish = [](const TCHAR* Error)
		{
			if (Error) { UE_LOG(LogMantleLayoutMap, Error, TEXT("MANTLE_LAYOUT_MAP error=%s"), Error); }
			UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP done"));
		};
		const FMantleAvatarContract* Contract = FMantleAvatarContract::Get();
		if (!Contract || OutPath.IsEmpty()) { Finish(TEXT("the avatar contract or the output path is unavailable")); return; }

		FPair Aera, Bo;
		FString Error;
		if (!LoadPair(TEXT("Aera"), *Contract, Aera, Error)) { ReleasePair(Aera); Finish(*Error); return; }
		if (Aera.Preview.Topology.VertexCount != Contract->TopologyVertexCount || Aera.Built.Topology.UVHash != Contract->TopologyUVHash)
		{
			const FString Why = FString::Printf(TEXT("Aera's preview LOD%d has %d vertices (reference %d) or its built body is not the reference UV layout"), Aera.PreviewLOD,
				Aera.Preview.Topology.VertexCount, Contract->TopologyVertexCount);
			ReleasePair(Aera);
			Finish(*Why);
			return;
		}

		FMantleCreatorBodyMap Map;
		Map.SourceUVHash = Aera.Preview.Topology.UVHash;
		Map.ReferenceUVHash = Aera.Built.Topology.UVHash;
		FVector3f Shift;
		float MaxCm = 0.0f;
		if (!LearnMap(Aera.Preview, Aera.Built, Shift, Map.PreviewOfReference, MaxCm, Error)) { ReleasePair(Aera); Finish(*Error); return; }
		int32 Inconsistent = 0;
		const int32 Broken = CountBrokenTriangles(Aera.Preview, Aera.Built, Map.PreviewOfReference, Inconsistent);
		UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP Aera learned: shift (%.3f, %.3f, %.3f) cm, max position error %.4f cm, %d of %d triangles not on a preview triangle, %d inconsistent weld groups"),
			Shift.X, Shift.Y, Shift.Z, MaxCm, Broken, Aera.Built.Indices.Num() / 3, Inconsistent);
		Map.MaxErrorCmAera = MaxCm;
		ReleasePair(Aera);
		if (MaxCm > 0.1f || Broken != 0 || Inconsistent != 0)
		{
			Finish(TEXT("Aera's preview and built bodies do not agree to 1 mm / in connectivity; no map written"));
			return;
		}

		FString BoError;
		if (LoadPair(TEXT("Bo"), *Contract, Bo, BoError))
		{
			if (Bo.Preview.Topology.UVHash != Map.SourceUVHash || Bo.Preview.Topology.VertexCount != Map.PreviewOfReference.Num())
			{
				const FString Why = FString::Printf(TEXT("Bo's preview LOD has uvhash %08x, not Aera's %08x; the layout is not shared, no map written"), Bo.Preview.Topology.UVHash, Map.SourceUVHash);
				ReleasePair(Bo);
				Finish(*Why);
				return;
			}
			FVector3f BoShift;
			const float BoMax = MaxErrorWithMap(Bo.Preview, Bo.Built, Map.PreviewOfReference, BoShift);
			int32 BoInconsistent = 0;
			const int32 BoBroken = CountBrokenTriangles(Bo.Preview, Bo.Built, Map.PreviewOfReference, BoInconsistent);
			UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP Bo checked with Aera's map: shift (%.3f, %.3f, %.3f) cm, max position error %.4f cm, %d triangles not on a preview triangle"),
				BoShift.X, BoShift.Y, BoShift.Z, BoMax, BoBroken);
			Map.MaxErrorCmBo = BoMax;
			ReleasePair(Bo);
			if (BoMax > 0.1f || BoBroken != 0)
			{
				Finish(TEXT("Aera's map does not carry over to Bo within 1 mm; no map written (the map is not topology-level)"));
				return;
			}
		}
		else
		{
			ReleasePair(Bo);
			UE_LOG(LogMantleLayoutMap, Warning, TEXT("MANTLE_LAYOUT_MAP Bo not checked: %s"), *BoError);
		}

		Map.Generated = FDateTime::UtcNow().ToIso8601();
		if (!FFileHelper::SaveStringToFile(Map.ToJson(), *OutPath))
		{
			Finish(*FString::Printf(TEXT("could not write %s"), *OutPath));
			return;
		}
		UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP written=%s vertices=%d sourceUVHash=%08x referenceUVHash=%08x maxErrorCm aera=%.4f bo=%.4f"), *OutPath,
			Map.PreviewOfReference.Num(), Map.SourceUVHash, Map.ReferenceUVHash, Map.MaxErrorCmAera, Map.MaxErrorCmBo);
		Finish(nullptr);
	}

	FAutoConsoleCommand GMantleDevLearnBodyLayoutMapCommand(
		TEXT("Mantle.Dev.LearnBodyLayoutMap"),
		TEXT("Mantle.Dev.LearnBodyLayoutMap [out=<path>] -- maintainer command: learns how Creator's in-memory body LOD1 is numbered against Mantle's reference body ")
		TEXT("from the converted Aera (/Game/MetaHumans/Aera/Converted), checks it on Bo, and writes Resources/BodyLayout/creator-preview-to-reference-v1.json. Logs MANTLE_LAYOUT_MAP lines."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
