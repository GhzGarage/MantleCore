// Copyright Joshua Eger. All Rights Reserved.
//
// Maintainer command: Mantle.Dev.LearnBodyLayoutMap [out=<path>] [build=0] [verify=0]
// Learns, once, how MetaHuman Creator's in-memory body (LOD1) is numbered against Mantle's reference body (the output of Epic's
// UE Optimized / Medium build), one map per distinct preview layout, from the converted Aera and Bo (each map checked on the other
// character when it has the same layout), and writes them to the plugin-only
// resource Resources/BodyLayout/creator-preview-to-reference-v1.json (FMantleCreatorBodyMap). Nothing is uploaded or changed.
#include "MantleAvatarContract.h"
#include "MantleBodyTopologyData.h"
#include "MantleCosmeticExport.h"
#include "MantleCreatorBodyMap.h"
#include "MantleFitBodies.h"
#include "MantleMetaHumanBuild.h"

#include "Algo/Find.h"
#include "Algo/Sort.h"
#include "AssetRegistry/AssetRegistryModule.h"
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
#include "UObject/Package.h"
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

	bool LoadPairAt(const TCHAR* Name, const FString& CharacterPackage, const FString& CharacterObject, const FString& BodyPackage, const FString& BodyObject,
		const FMantleAvatarContract& Contract, FPair& Out, FString& OutError)
	{
		Out.Name = Name;
		if (!FPackageName::DoesPackageExist(CharacterPackage) || !FPackageName::DoesPackageExist(BodyPackage))
		{
			OutError = FString::Printf(TEXT("%s or %s is not in the project"), *CharacterPackage, *BodyPackage);
			return false;
		}
		UMetaHumanCharacter* Character = LoadObject<UMetaHumanCharacter>(nullptr, *FString::Printf(TEXT("%s.%s"), *CharacterPackage, *CharacterObject));
		const USkeletalMesh* Built = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *BodyPackage, *BodyObject));
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

	bool LoadConverted(const TCHAR* Name, const FMantleAvatarContract& Contract, FPair& Out, FString& OutError)
	{
		return LoadPairAt(Name, FString::Printf(TEXT("/Game/MetaHumans/%s/Converted/%s/%s"), Name, Name, Name), Name,
			FString::Printf(TEXT("/Game/MetaHumans/%s/Converted/%s/Unpacked/%s/Body/SKM_%s_BodyMesh"), Name, Name, Name, Name), FString::Printf(TEXT("SKM_%s_BodyMesh"), Name),
			Contract, Out, OutError);
	}

	/**
	 * A standard body type as a built character, for learning its layout: /Game/MetaHumans/FitRef_<type>. When its built body is not in the
	 * project yet, the character is made (Epic's initial character set to the fixed body type, in the project) and built through the plugin's
	 * converter build (FMantleMetaHumanBuild::Build, Optimized / Medium, the route Aera and Bo took). That asks Epic for the sign-in and uses
	 * Epic's cloud for the skin and rig; not signed in is a clean stop with Epic's message.
	 */
	bool LoadOrBuildStandard(const TCHAR* Id, const EMetaHumanBodyType Type, const FMantleAvatarContract& Contract, const bool bAllowBuild, FPair& Out, FString& OutError)
	{
		const FString Name = FString(TEXT("FitRef_")) + Id;
		const FString Folder = FString::Printf(TEXT("/Game/MetaHumans/%s"), *Name);
		const FString CharacterPackage = FString::Printf(TEXT("%s/%s"), *Folder, *Name);
		const FString BodyPackage = FString::Printf(TEXT("%s/Unpacked/%s/Body/SKM_%s_BodyMesh"), *Folder, *Name, *Name);
		if (!FPackageName::DoesPackageExist(BodyPackage))
		{
			UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>() : nullptr;
			if (!bAllowBuild || !Subsystem)
			{
				OutError = FString::Printf(TEXT("%s is not built in the project (and building is off or MetaHuman Creator is unavailable)"), *Name);
				return false;
			}
			UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP building %s with the converter build (Epic sign-in needed, about 5 minutes)"), *Name);
			UPackage* Package = FPackageName::DoesPackageExist(CharacterPackage) ? LoadPackage(nullptr, *CharacterPackage, LOAD_None) : CreatePackage(*CharacterPackage);
			UMetaHumanCharacter* Character = Package ? FindObject<UMetaHumanCharacter>(Package, *Name) : nullptr;
			if (Package && !Character)
			{
				Character = NewObject<UMetaHumanCharacter>(Package, FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
				if (Character)
				{
					FAssetRegistryModule::AssetCreated(Character);
					Subsystem->InitializeMetaHumanCharacter(Character);
				}
			}
			if (!Character || !Character->IsCharacterValid())
			{
				OutError = FString::Printf(TEXT("the character %s could not be made"), *CharacterPackage);
				return false;
			}
			if (!Subsystem->IsObjectAddedForEditing(Character) && !Subsystem->TryAddObjectToEdit(Character))
			{
				OutError = FString::Printf(TEXT("MetaHuman Creator could not open %s"), *CharacterPackage);
				return false;
			}
			Subsystem->SetMetaHumanBodyType(Character, Type, UMetaHumanCharacterEditorSubsystem::EBodyMeshUpdateMode::Full);
			Subsystem->CommitBodyState(Character);
			Character->MarkPackageDirty();
			FMantleMetaHumanBuildResult Result;
			FString BuildError;
			if (!FMantleMetaHumanBuild::Build(*Character, Folder, Result, BuildError))
			{
				OutError = FString::Printf(TEXT("the converter build of %s stopped: %s"), *Name, *BuildError);
				return false;
			}
			UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP built %s at %s"), *Name, *Result.BuildRoot);
		}
		return LoadPairAt(*Name, CharacterPackage, Name, BodyPackage, FString::Printf(TEXT("SKM_%s_BodyMesh"), *Name), Contract, Out, OutError);
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

	struct FTriangleCheck
	{
		int32 Broken = 0;
		int32 Flipped = 0;
		int32 Unexplained = 0;
		int32 InconsistentGroups = 0;
	};

	/**
	 * Every built triangle should land on a preview triangle once seam copies are welded. A few may not: Epic's optimized build can
	 * flip the diagonal of a quad. Those are accepted only as pairs (two built triangles sharing an edge whose four vertices are two
	 * preview triangles on the other diagonal), each logged; anything else is unexplained.
	 */
	FTriangleCheck CheckTriangles(const FSide& Preview, const FSide& Built, const TArray<int32>& Map)
	{
		FTriangleCheck Result;
		const TArray<int32> PreviewGroup = WeldGroups(Preview.Positions);
		const TArray<int32> BuiltGroup = WeldGroups(Built.Positions);
		TMap<int32, int32> PreviewOfBuiltGroup;
		for (int32 Reference = 0; Reference < Map.Num(); ++Reference)
		{
			const int32 Wanted = PreviewGroup[Map[Reference]];
			const int32 Target = PreviewOfBuiltGroup.FindOrAdd(BuiltGroup[Reference], Wanted);
			if (Target != Wanted) { ++Result.InconsistentGroups; }
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
		struct FBroken { int32 Triangle; int32 Groups[3]; bool bUsed; };
		TArray<FBroken> Broken;
		for (int32 First = 0; First + 2 < Built.Indices.Num(); First += 3)
		{
			const int32* A = PreviewOfBuiltGroup.Find(BuiltGroup[Built.Indices[First]]);
			const int32* B = PreviewOfBuiltGroup.Find(BuiltGroup[Built.Indices[First + 1]]);
			const int32* C = PreviewOfBuiltGroup.Find(BuiltGroup[Built.Indices[First + 2]]);
			if (A && B && C && PreviewTriangles.Contains(Key(*A, *B, *C))) { continue; }
			++Result.Broken;
			if (A && B && C) { Broken.Add({ First / 3, { *A, *B, *C }, false }); }
			else { ++Result.Unexplained; }
		}
		for (int32 I = 0; I < Broken.Num(); ++I)
		{
			if (Broken[I].bUsed) { continue; }
			for (int32 J = I + 1; J < Broken.Num() && !Broken[I].bUsed; ++J)
			{
				if (Broken[J].bUsed) { continue; }
				TArray<int32> Shared, OnlyI, OnlyJ;
				for (const int32 Group : Broken[I].Groups) { (Algo::Find(Broken[J].Groups, Group) ? Shared : OnlyI).AddUnique(Group); }
				for (const int32 Group : Broken[J].Groups) { if (!Shared.Contains(Group)) { OnlyJ.AddUnique(Group); } }
				if (Shared.Num() != 2 || OnlyI.Num() != 1 || OnlyJ.Num() != 1 || OnlyI[0] == OnlyJ[0]) { continue; }
				if (PreviewTriangles.Contains(Key(OnlyI[0], OnlyJ[0], Shared[0])) && PreviewTriangles.Contains(Key(OnlyI[0], OnlyJ[0], Shared[1])))
				{
					Broken[I].bUsed = Broken[J].bUsed = true;
					Result.Flipped += 2;
					UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP flipped diagonal: reference triangles %d and %d are the preview's other diagonal of the same quad"), Broken[I].Triangle, Broken[J].Triangle);
				}
			}
		}
		for (const FBroken& Entry : Broken)
		{
			if (!Entry.bUsed)
			{
				++Result.Unexplained;
				UE_LOG(LogMantleLayoutMap, Warning, TEXT("MANTLE_LAYOUT_MAP unexplained triangle: reference triangle %d is not a preview triangle and not half of a flipped quad"), Entry.Triangle);
			}
		}
		return Result;
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

	constexpr float AcceptCm = 0.1f;
	constexpr int32 AcceptBrokenTriangles = 16;

	bool Accepts(const FTriangleCheck& Check, const float MaxCm)
	{
		return MaxCm <= AcceptCm && Check.InconsistentGroups == 0 && Check.Unexplained == 0 && Check.Broken <= AcceptBrokenTriangles;
	}

	/** One converted character, read and released: its name, preview and built sides, and which standard body it is nearest to. */
	struct FCharacter
	{
		FPair Pair;
		FString Sex;
	};

	/** Preview LOD1 vertices of a standard body made in memory (nothing saved), or empty. */
	TArray<FVector3f> StandardBodyVertices(const TCHAR* Id, const EMetaHumanBodyType Type, uint32& OutUVHash)
	{
		TArray<FVector3f> Out;
		OutUVHash = 0;
		FMantleFitBody Body;
		FString Error;
		if (FMantleFitBodies::Make(Id, Type, Body, Error))
		{
			FSide Side;
			if (ReadSide(Body.BodyMesh.Get(), 1, Side, Error))
			{
				Out = MoveTemp(Side.Positions);
				OutUVHash = Side.Topology.UVHash;
			}
			else { UE_LOG(LogMantleLayoutMap, Warning, TEXT("MANTLE_LAYOUT_MAP standard body %s LOD1 unreadable: %s"), Id, *Error); }
		}
		else { UE_LOG(LogMantleLayoutMap, Warning, TEXT("MANTLE_LAYOUT_MAP standard body %s not made: %s"), Id, *Error); }
		FMantleFitBodies::Release(Body);
		return Out;
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

		// Converted characters (Aera, Bo) and standard body types built once into the project (FitRef_<type>, see LoadOrBuildStandard). Every one
		// is read even when an earlier one fails; nothing is held in the editor afterwards.
		bool bAllowBuild = true, bVerifyBuild = true;
		for (const FString& Arg : Args)
		{
			if (Arg.Equals(TEXT("build=0"), ESearchCase::IgnoreCase)) { bAllowBuild = false; }
			if (Arg.Equals(TEXT("verify=0"), ESearchCase::IgnoreCase)) { bVerifyBuild = false; }
		}
		TArray<TUniquePtr<FCharacter>> Characters;
		const auto Keep = [&Characters](TUniquePtr<FCharacter> Character)
		{
			UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP %s (%s): preview LOD1 uvhash %08x, %d verts"), *Character->Pair.Name, *Character->Sex,
				Character->Pair.Preview.Topology.UVHash, Character->Pair.Preview.Topology.VertexCount);
			Characters.Add(MoveTemp(Character));
		};
		for (const TCHAR* Name : { TEXT("Aera"), TEXT("Bo") })
		{
			TUniquePtr<FCharacter> Character = MakeUnique<FCharacter>();
			Character->Sex = TEXT("converted preset");
			FString Error;
			const bool bLoaded = LoadConverted(Name, *Contract, Character->Pair, Error);
			ReleasePair(Character->Pair);
			if (bLoaded) { Keep(MoveTemp(Character)); }
			else { UE_LOG(LogMantleLayoutMap, Warning, TEXT("MANTLE_LAYOUT_MAP %s skipped: %s"), Name, *Error); }
		}
		// The standard body types the upload makes: the layout of every one is the same (one build is enough to learn it); a second, differently
		// shaped body checks that the map carries across shapes. A build is Epic's converter build (about 5 minutes, needs Epic sign-in).
		TArray<TPair<const TCHAR*, EMetaHumanBodyType>> Standards;
		Standards.Emplace(TEXT("f_med_nrw"), EMetaHumanBodyType::f_med_nrw);
		if (bVerifyBuild) { Standards.Emplace(TEXT("f_med_ovw"), EMetaHumanBodyType::f_med_ovw); }
		for (const TPair<const TCHAR*, EMetaHumanBodyType>& Standard : Standards)
		{
			TUniquePtr<FCharacter> Character = MakeUnique<FCharacter>();
			Character->Sex = TEXT("standard female body");
			FString Error;
			const bool bLoaded = LoadOrBuildStandard(Standard.Key, Standard.Value, *Contract, bAllowBuild, Character->Pair, Error);
			ReleasePair(Character->Pair);
			if (bLoaded) { Keep(MoveTemp(Character)); }
			else { UE_LOG(LogMantleLayoutMap, Warning, TEXT("MANTLE_LAYOUT_MAP standard body %s skipped: %s"), Standard.Key, *Error); }
		}


		// One map per distinct preview layout: learned from the first character that has it, checked on the others that do.
		TArray<FMantleCreatorBodyMap> Maps;
		TSet<uint32> Done;
		for (const TUniquePtr<FCharacter>& Source : Characters)
		{
			const uint32 Hash = Source->Pair.Preview.Topology.UVHash;
			if (Done.Contains(Hash)) { continue; }
			const FPair& Pair = Source->Pair;
			if (Pair.Preview.Topology.VertexCount != Contract->TopologyVertexCount || Pair.Built.Topology.UVHash != Contract->TopologyUVHash)
			{
				UE_LOG(LogMantleLayoutMap, Warning, TEXT("MANTLE_LAYOUT_MAP %s: preview LOD%d has %d vertices (reference %d) or its built body is not the reference UV layout; skipped"),
					*Pair.Name, Pair.PreviewLOD, Pair.Preview.Topology.VertexCount, Contract->TopologyVertexCount);
				continue;
			}
			FMantleCreatorBodyMap Map;
			Map.SourceUVHash = Hash;
			Map.ReferenceUVHash = Pair.Built.Topology.UVHash;
			Map.Source = Pair.Name + TEXT(" (") + Source->Sex + TEXT(")");
			FVector3f Shift;
			float MaxCm = 0.0f;
			FString Error;
			if (!LearnMap(Pair.Preview, Pair.Built, Shift, Map.PreviewOfReference, MaxCm, Error))
			{
				UE_LOG(LogMantleLayoutMap, Warning, TEXT("MANTLE_LAYOUT_MAP %s layout %08x: %s"), *Pair.Name, Hash, *Error);
				continue;
			}
			const FTriangleCheck Check = CheckTriangles(Pair.Preview, Pair.Built, Map.PreviewOfReference);
			UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP %s learned for layout %08x: shift (%.3f, %.3f, %.3f) cm, max position error %.4f cm, %d of %d triangles differ (%d in flipped-diagonal pairs, %d unexplained), %d inconsistent weld groups"),
				*Pair.Name, Hash, Shift.X, Shift.Y, Shift.Z, MaxCm, Check.Broken, Pair.Built.Indices.Num() / 3, Check.Flipped, Check.Unexplained, Check.InconsistentGroups);
			if (!Accepts(Check, MaxCm))
			{
				UE_LOG(LogMantleLayoutMap, Warning, TEXT("MANTLE_LAYOUT_MAP %s layout %08x not accepted (needs positions within %.1f mm, at most %d differing triangles all in flipped pairs)"), *Pair.Name, Hash, AcceptCm * 10.0f, AcceptBrokenTriangles);
				continue;
			}
			Map.MaxErrorCm = MaxCm;
			Map.FlippedTriangles = Check.Flipped;
			bool bVerifiedAll = true;
			for (const TUniquePtr<FCharacter>& Other : Characters)
			{
				if (Other.Get() == Source.Get() || Other->Pair.Preview.Topology.UVHash != Hash || Other->Pair.Preview.Topology.VertexCount != Map.PreviewOfReference.Num()) { continue; }
				FVector3f OtherShift;
				const float OtherMax = MaxErrorWithMap(Other->Pair.Preview, Other->Pair.Built, Map.PreviewOfReference, OtherShift);
				const FTriangleCheck OtherCheck = CheckTriangles(Other->Pair.Preview, Other->Pair.Built, Map.PreviewOfReference);
				UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP %s's map checked on %s: shift (%.3f, %.3f, %.3f) cm, max position error %.4f cm, %d triangles differ (%d flipped, %d unexplained)"),
					*Pair.Name, *Other->Pair.Name, OtherShift.X, OtherShift.Y, OtherShift.Z, OtherMax, OtherCheck.Broken, OtherCheck.Flipped, OtherCheck.Unexplained);
				Map.VerifiedOn += (Map.VerifiedOn.IsEmpty() ? TEXT("") : TEXT(", ")) + Other->Pair.Name + TEXT(" (") + Other->Sex + TEXT(")");
				Map.MaxErrorCm = FMath::Max<double>(Map.MaxErrorCm, OtherMax);
				bVerifiedAll &= Accepts(OtherCheck, OtherMax);
			}
			if (!bVerifiedAll)
			{
				UE_LOG(LogMantleLayoutMap, Warning, TEXT("MANTLE_LAYOUT_MAP layout %08x: the map does not carry over to every character with that layout; not written"), Hash);
				continue;
			}
			Map.Generated = FDateTime::UtcNow().ToIso8601();
			Maps.Add(MoveTemp(Map));
			Done.Add(Hash);
		}

		// What the standard bodies need: a map for each preview layout they have.
		if (Maps.IsEmpty())
		{
			Finish(TEXT("no layout could be learned; no map written"));
			return;
		}
		if (!FFileHelper::SaveStringToFile(FMantleCreatorBodyMap::ToJson(Maps), *OutPath))
		{
			Finish(*FString::Printf(TEXT("could not write %s"), *OutPath));
			return;
		}
		for (const FMantleCreatorBodyMap& Map : Maps)
		{
			UE_LOG(LogMantleLayoutMap, Display, TEXT("MANTLE_LAYOUT_MAP written=%s layout=%08x from=%s verifiedOn=%s maxErrorCm=%.4f flippedTriangles=%d"), *OutPath, Map.SourceUVHash,
				*Map.Source, *Map.VerifiedOn, Map.MaxErrorCm, Map.FlippedTriangles);
		}
		Finish(nullptr);
	}

	FAutoConsoleCommand GMantleDevLearnBodyLayoutMapCommand(
		TEXT("Mantle.Dev.LearnBodyLayoutMap"),
		TEXT("Mantle.Dev.LearnBodyLayoutMap [out=<path>] -- maintainer command: learns how Creator's in-memory body LOD1 is numbered against Mantle's reference body, one map per ")
		TEXT("preview layout, from the converted Aera and Bo (/Game/MetaHumans/<Name>/Converted) and from the standard body types f_med_nrw / f_med_ovw built once into /Game/MetaHumans/FitRef_<type> (Epic sign-in, ~5 min each; build=0 never builds, verify=0 builds only f_med_nrw), and writes Resources/BodyLayout/creator-preview-to-reference-v1.json. ")
		TEXT("Logs MANTLE_LAYOUT_MAP lines."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
