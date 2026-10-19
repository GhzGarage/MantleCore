// Copyright Joshua Eger. All Rights Reserved.
#include "MantleFitBodies.h"

#include "Editor.h"
#include "MeshDescription.h"
#include "MetaHumanCharacterBodyIdentity.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalMeshAttributes.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogMantleFitBodies, Log, All);

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleFitBodiesPrivate
{
	UMetaHumanCharacterEditorSubsystem* GetSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>() : nullptr;
	}

	FString PresetObjectPath(const FString& PresetName)
	{
		return FString::Printf(TEXT("/MetaHumanCharacter/Optional/Presets/%s.%s"), *PresetName, *PresetName);
	}

	/** Every body setting of the character's current body state, with its current value. */
	void ReadTargets(UMetaHumanCharacterEditorSubsystem& Subsystem, const UMetaHumanCharacter& Character, TArray<FMantleFitBodyTarget>& Out)
	{
		Out.Reset();
		const TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> State = Subsystem.GetBodyState(&Character);
		const TArray<FMetaHumanCharacterBodyConstraint> Constraints = State->GetBodyConstraints();
		for (int32 Index = 0; Index < Constraints.Num(); ++Index)
		{
			FMantleFitBodyTarget& Target = Out.AddDefaulted_GetRef();
			Target.Constraint = Constraints[Index].Name;
			Target.Measurement = State->GetMeasurement(Index);
		}
	}
}

TArray<FMantleFitBodyDefinition> FMantleFitBodies::BuiltIn()
{
	TArray<FMantleFitBodyDefinition> Out;
	FMantleFitBodyDefinition& Aera = Out.AddDefaulted_GetRef();
	Aera.Id = TEXT("aera");
	Aera.PresetName = TEXT("Aera");
	FMantleFitBodyDefinition& Bo = Out.AddDefaulted_GetRef();
	Bo.Id = TEXT("bo");
	Bo.PresetName = TEXT("Bo");
	return Out;
}

bool FMantleFitBodies::ReadPresetTargets(const FString& PresetName, EMantleFitBodyOuter Outer, TArray<FMantleFitBodyTarget>& Out, FString& OutError)
{
	FMantleFitBodyDefinition Definition;
	Definition.Id = PresetName.ToLower() + TEXT("_numbers_source");
	Definition.PresetName = PresetName;
	FMantleFitBody Throwaway;
	if (!Make(Definition, EMantleFitBodySource::Preset, Outer, Throwaway, OutError))
	{
		Release(Throwaway);
		return false;
	}
	UMetaHumanCharacterEditorSubsystem* Subsystem = MantleFitBodiesPrivate::GetSubsystem();
	if (!Subsystem || !Throwaway.Character.IsValid())
	{
		OutError = TEXT("MetaHuman Creator's editor subsystem is unavailable");
		Release(Throwaway);
		return false;
	}
	MantleFitBodiesPrivate::ReadTargets(*Subsystem, *Throwaway.Character.Get(), Out);
	Release(Throwaway);
	if (Out.IsEmpty())
	{
		OutError = FString::Printf(TEXT("preset %s has no body settings to read"), *PresetName);
		return false;
	}
	return true;
}

bool FMantleFitBodies::Make(const FMantleFitBodyDefinition& Definition, EMantleFitBodySource Source, EMantleFitBodyOuter Outer,
	FMantleFitBody& Out, FString& OutError)
{
	Release(Out);
	Out = FMantleFitBody();
	Out.Id = Definition.Id;
	Out.bTransient = Outer == EMantleFitBodyOuter::Transient;

	UMetaHumanCharacterEditorSubsystem* Subsystem = MantleFitBodiesPrivate::GetSubsystem();
	if (!Subsystem)
	{
		OutError = TEXT("MetaHuman Creator's editor subsystem is unavailable");
		return false;
	}

	UObject* CharacterOuter = GetTransientPackage();
	if (!Out.bTransient)
	{
		// Never saved: the package exists only to give the character an outer other than the transient package.
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/MantleFitBody_%s"), *Definition.Id));
		if (!Package)
		{
			OutError = TEXT("could not create a temporary package for the fit body");
			return false;
		}
		Out.TempPackage.Reset(Package);
		CharacterOuter = Package;
	}
	const FName CharacterName = MakeUniqueObjectName(CharacterOuter, UMetaHumanCharacter::StaticClass(), FName(*FString::Printf(TEXT("MantleFitBody_%s"), *Definition.Id)));
	UMetaHumanCharacter* Character = NewObject<UMetaHumanCharacter>(CharacterOuter, UMetaHumanCharacter::StaticClass(), CharacterName);
	if (!Character)
	{
		OutError = TEXT("could not create the in-memory MetaHuman character");
		return false;
	}
	Out.Character.Reset(Character);
	Subsystem->InitializeMetaHumanCharacter(Character);
	if (!Character->IsCharacterValid())
	{
		OutError = TEXT("MetaHuman Creator did not initialise the in-memory character");
		return false;
	}
	if (!Subsystem->TryAddObjectToEdit(Character))
	{
		OutError = TEXT("MetaHuman Creator could not open the in-memory character for editing");
		return false;
	}
	Out.bEditing = true;

	if (Source == EMantleFitBodySource::Preset)
	{
		UMetaHumanCharacter* Preset = LoadObject<UMetaHumanCharacter>(nullptr, *MantleFitBodiesPrivate::PresetObjectPath(Definition.PresetName));
		if (!Preset)
		{
			OutError = FString::Printf(TEXT("Epic's preset %s was not found"), *MantleFitBodiesPrivate::PresetObjectPath(Definition.PresetName));
			return false;
		}
		Subsystem->InitializeFromPreset(Character, Preset);
	}
	else
	{
		TArray<FMantleFitBodyTarget> Targets = Definition.Targets;
		if (Targets.IsEmpty() && !Definition.PresetName.IsEmpty() && !ReadPresetTargets(Definition.PresetName, Outer, Targets, OutError))
		{
			return false;
		}
		TArray<FMetaHumanCharacterBodyConstraint> Constraints = Subsystem->GetBodyConstraints(Character);
		if (Constraints.IsEmpty())
		{
			OutError = TEXT("the in-memory character's body has no settings to apply numbers to");
			return false;
		}
		int32 Applied = 0;
		for (FMetaHumanCharacterBodyConstraint& Constraint : Constraints)
		{
			const FMantleFitBodyTarget* Target = Targets.FindByPredicate([&Constraint](const FMantleFitBodyTarget& Candidate) { return Candidate.Constraint == Constraint.Name; });
			Constraint.bIsActive = Target != nullptr;
			if (Target)
			{
				Constraint.TargetMeasurement = FMath::Clamp(Target->Measurement, Constraint.MinMeasurement, Constraint.MaxMeasurement);
				++Applied;
			}
		}
		if (Applied == 0)
		{
			OutError = TEXT("none of the fit body's numbers names a setting of Creator's body");
			return false;
		}
		Subsystem->SetBodyConstraints(Character, Constraints);
	}
	Subsystem->CommitBodyState(Character);
	Out.bFixedBodyType = Character->bFixedBodyType;

	FMetaHumanCharacterPreviewAssetOptions Options;
	Options.bGenerateMergedHeadAndBodyMesh = true;
	Options.bGenerateBodyMeasurements = true;
	FMetaHumanCharacterPreviewAssets Preview;
	if (!Subsystem->TryGetCharacterPreviewAssets(Character, Options, Preview) || !Preview.BodyMesh)
	{
		OutError = TEXT("MetaHuman Creator could not provide the in-memory character's body");
		return false;
	}
	Out.BodyMesh.Reset(Preview.BodyMesh.Get());
	Out.FaceMesh.Reset(Preview.FaceMesh.Get());
	Out.MergedHeadAndBodyMesh.Reset(Preview.MergedHeadAndBodyMesh.Get());
	Out.Measurements = Preview.BodyMeasurements;

	for (const TPair<FString, float>& Measurement : Out.Measurements)
	{
		if (Measurement.Key.Equals(TEXT("Height"), ESearchCase::IgnoreCase))
		{
			Out.HeightCm = Measurement.Value;
			Out.HeightSource = TEXT("measurements");
			break;
		}
	}
	if (Out.HeightSource.IsEmpty())
	{
		TArray<FVector3f> Vertices;
		if (ReadVertices(Out.BodyMesh.Get(), 0, Vertices) && !Vertices.IsEmpty())
		{
			float Low = Vertices[0].Z, High = Vertices[0].Z;
			for (const FVector3f& Vertex : Vertices)
			{
				Low = FMath::Min(Low, Vertex.Z);
				High = FMath::Max(High, Vertex.Z);
			}
			Out.HeightCm = High - Low;
			Out.HeightSource = TEXT("bounds");
		}
	}
	UE_LOG(LogMantleFitBodies, Display, TEXT("fit body %s made in memory (%s, %s): fixedBodyType=%d height=%.2f cm from %s"), *Out.Id,
		Out.bTransient ? TEXT("transient package") : TEXT("temp package"), Source == EMantleFitBodySource::Preset ? TEXT("preset") : TEXT("numbers"),
		Out.bFixedBodyType ? 1 : 0, Out.HeightCm, *Out.HeightSource);
	return true;
}

void FMantleFitBodies::Release(FMantleFitBody& Body)
{
	if (Body.bEditing && Body.Character.IsValid())
	{
		if (UMetaHumanCharacterEditorSubsystem* Subsystem = MantleFitBodiesPrivate::GetSubsystem())
		{
			Subsystem->RemoveObjectToEdit(Body.Character.Get());
		}
	}
	Body.bEditing = false;
	Body.BodyMesh.Reset();
	Body.FaceMesh.Reset();
	Body.MergedHeadAndBodyMesh.Reset();
	Body.Character.Reset();
	Body.TempPackage.Reset();
}

bool FMantleFitBodies::ReadVertices(const USkeletalMesh* Mesh, int32 LODIndex, TArray<FVector3f>& OutPositions)
{
	OutPositions.Reset();
	if (!Mesh)
	{
		return false;
	}
	const FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering();
	if (RenderData && RenderData->LODRenderData.IsValidIndex(LODIndex))
	{
		const FPositionVertexBuffer& Positions = RenderData->LODRenderData[LODIndex].StaticVertexBuffers.PositionVertexBuffer;
		const int32 Count = static_cast<int32>(Positions.GetNumVertices());
		if (Count > 0 && Positions.GetVertexData())
		{
			OutPositions.Reserve(Count);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				OutPositions.Add(Positions.VertexPosition(static_cast<uint32>(Index)));
			}
			return true;
		}
	}
	if (LODIndex >= 0 && LODIndex < Mesh->GetLODNum() && Mesh->HasMeshDescription(LODIndex))
	{
		if (const FMeshDescription* Description = Mesh->GetMeshDescription(LODIndex))
		{
			FSkeletalMeshConstAttributes Attributes(*Description);
			TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
			if (Positions.IsValid())
			{
				OutPositions.Reserve(Description->Vertices().Num());
				for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
				{
					OutPositions.Add(Positions.Get(Vertex));
				}
				return !OutPositions.IsEmpty();
			}
		}
	}
	return false;
}

int32 FMantleFitBodies::CountTriangles(const USkeletalMesh* Mesh)
{
	const FSkeletalMeshRenderData* RenderData = Mesh ? Mesh->GetResourceForRendering() : nullptr;
	if (!RenderData || RenderData->LODRenderData.IsEmpty())
	{
		return 0;
	}
	int32 Triangles = 0;
	for (const FSkelMeshRenderSection& Section : RenderData->LODRenderData[0].RenderSections)
	{
		Triangles += static_cast<int32>(Section.NumTriangles);
	}
	return Triangles;
}

bool FMantleFitBodies::CompareNearest(const TArray<FVector3f>& Probe, const TArray<FVector3f>& Reference, float& OutAverageCm, float& OutMaxCm)
{
	OutAverageCm = 0.0f;
	OutMaxCm = 0.0f;
	if (Probe.IsEmpty() || Reference.IsEmpty())
	{
		return false;
	}
	const auto CellOf = [](const FVector3f& Point)
	{
		return FIntVector(FMath::FloorToInt32(Point.X), FMath::FloorToInt32(Point.Y), FMath::FloorToInt32(Point.Z));
	};
	TMap<FIntVector, TArray<int32>> Grid;
	for (int32 Index = 0; Index < Reference.Num(); ++Index)
	{
		Grid.FindOrAdd(CellOf(Reference[Index])).Add(Index);
	}
	constexpr int32 MaxRing = 12;
	double Sum = 0.0;
	for (const FVector3f& Point : Probe)
	{
		const FIntVector Cell = CellOf(Point);
		float Best = TNumericLimits<float>::Max();
		for (int32 Ring = 0; Ring <= MaxRing && Best > static_cast<float>(Ring - 1); ++Ring)
		{
			for (int32 X = -Ring; X <= Ring; ++X)
			{
				for (int32 Y = -Ring; Y <= Ring; ++Y)
				{
					for (int32 Z = -Ring; Z <= Ring; ++Z)
					{
						if (FMath::Max3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z)) != Ring)
						{
							continue;
						}
						if (const TArray<int32>* Found = Grid.Find(FIntVector(Cell.X + X, Cell.Y + Y, Cell.Z + Z)))
						{
							for (const int32 Index : *Found)
							{
								Best = FMath::Min(Best, FVector3f::Distance(Point, Reference[Index]));
							}
						}
					}
				}
			}
		}
		if (Best == TNumericLimits<float>::Max() || Best > static_cast<float>(MaxRing))
		{
			// Nothing close in the grid: look at every vertex (rare; a body far from the reference).
			for (const FVector3f& Candidate : Reference)
			{
				Best = FMath::Min(Best, FVector3f::Distance(Point, Candidate));
			}
		}
		Sum += Best;
		OutMaxCm = FMath::Max(OutMaxCm, Best);
	}
	OutAverageCm = static_cast<float>(Sum / Probe.Num());
	return true;
}
