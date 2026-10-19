// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"
#include "MetaHumanCharacter.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

/** One body setting of MetaHuman Creator's parametric body (Height, Chest, Waist, Hip, Fat, Muscularity ...) and the value to hold. */
struct MANTLEEDITOR_API FMantleFitBodyTarget
{
	FName Constraint;
	float Measurement = 0.0f;
};

/** A fit body as a few numbers: an id, the engine preset it was read from (if any), and its body settings. */
struct MANTLEEDITOR_API FMantleFitBodyDefinition
{
	FString Id;
	/** Epic's shipped preset (/MetaHumanCharacter/Optional/Presets/<PresetName>), the source of the numbers in this spike. */
	FString PresetName;
	/** Empty until read from the preset (FMantleFitBodies::ReadPresetTargets) or filled by hand. */
	TArray<FMantleFitBodyTarget> Targets;
};

enum class EMantleFitBodySource : uint8
{
	/** Epic's preset applied whole (InitializeFromPreset). */
	Preset,
	/** Epic's default body with only the numbers in Targets applied (SetBodyConstraints). */
	Numbers,
};

enum class EMantleFitBodyOuter : uint8
{
	/** The character lives in the transient package: nothing is ever in the project. */
	Transient,
	/** Fallback: the character lives in an unsaved /Temp package that is dropped afterwards. */
	TempPackage,
};

/** A fit body made in memory: a Creator character that was never saved, and the meshes the editor evaluates for it. */
struct MANTLEEDITOR_API FMantleFitBody
{
	FString Id;
	bool bTransient = true;
	bool bFixedBodyType = false;
	bool bEditing = false;
	TStrongObjectPtr<UMetaHumanCharacter> Character;
	TStrongObjectPtr<UPackage> TempPackage;
	TStrongObjectPtr<USkeletalMesh> BodyMesh;
	TStrongObjectPtr<USkeletalMesh> FaceMesh;
	TStrongObjectPtr<USkeletalMesh> MergedHeadAndBodyMesh;
	TMap<FString, float> Measurements;
	float HeightCm = 0.0f;
	/** Where HeightCm came from: "measurements" (Creator's own) or "bounds" (the body mesh's height). */
	FString HeightSource;
};

/**
 * Dev spike (Mantle.Dev.FitFromNumbers): makes MetaHuman fit bodies in memory from numbers, with no converted Aera/Bo
 * assets in the project and no Epic sign-in or cloud build. The seed of "clothing fit from numbers"; upload does not use it yet.
 */
struct MANTLEEDITOR_API FMantleFitBodies
{
	/** Aera and Bo, ids "aera" and "bo", numbers not yet read. */
	static TArray<FMantleFitBodyDefinition> BuiltIn();

	/** Reads every body setting of Epic's preset into Out (names and current values), through a throwaway character. */
	static bool ReadPresetTargets(const FString& PresetName, EMantleFitBodyOuter Outer, TArray<FMantleFitBodyTarget>& Out, FString& OutError);

	/**
	 * Makes the character, applies the body (Preset: the whole preset; Numbers: Definition.Targets, read from the preset
	 * first when empty, on Epic's default body), commits it and fetches the editor's body, face and merged head+body meshes.
	 * The character stays registered for editing until Release. Game thread only.
	 */
	static bool Make(const FMantleFitBodyDefinition& Definition, EMantleFitBodySource Source, EMantleFitBodyOuter Outer,
		FMantleFitBody& Out, FString& OutError);

	/** Unregisters the character from editing and drops every reference, so the objects can be collected. */
	static void Release(FMantleFitBody& Body);

	/** LOD's vertex positions (component space, cm) from the render data, or the mesh description when there is no CPU copy. */
	static bool ReadVertices(const USkeletalMesh* Mesh, int32 LODIndex, TArray<FVector3f>& OutPositions);

	/** Triangles of all sections of LOD 0 of a skeletal mesh, or 0. */
	static int32 CountTriangles(const USkeletalMesh* Mesh);

	/** For each vertex of Probe, the distance to the nearest vertex of Reference: average and largest, in cm. Order-independent. */
	static bool CompareNearest(const TArray<FVector3f>& Probe, const TArray<FVector3f>& Reference, float& OutAverageCm, float& OutMaxCm);
};
