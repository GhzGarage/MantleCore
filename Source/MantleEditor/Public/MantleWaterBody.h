// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class UActorComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** What a Water plugin body publishes as: its mesh and the material it draws with, or why it cannot be carried. */
struct FMantleWaterBodyMesh
{
	/** In the body component's own space, so the body's transform places it. Null when it could not be made. */
	UStaticMesh* Mesh = nullptr;

	/** The body's water material (may be null: the mesh's own slot then applies). */
	UMaterialInterface* Material = nullptr;

	/** "custom", "lake", "river" or "ocean". */
	FString Kind;

	/** The mesh is the body's own static mesh (its water mesh override), not one Mantle generated. */
	bool bOwnMesh = false;

	/** When Mesh is null: why, phrased to follow "left out: ". */
	FString Why;
};

/**
 * Water plugin bodies (WaterBodyCustom, Lake, River, Ocean components) as drawn water (GhzGarage/mantle#818, #628). MantleEditor does
 * not link the Water plugin, so everything is read by class name and property name. A body that has a static mesh of its own (its
 * water mesh override, which is all a Custom body draws) publishes that mesh; a Lake without one publishes a flat mesh filling its
 * spline at the spline's height. The mesh is drawn with the body's water material, which the material converter then publishes as
 * Mantle's water when it reads as water. A River or Ocean without a mesh of its own is rendered by the water mesh system at run time
 * from the landscape and the water zone, which no mesh exists for, so it is left out and said so.
 */
class MANTLEEDITOR_API FMantleWaterBody
{
public:
	/** Whether Component is a Water plugin body component (UWaterBodyComponent or a child). */
	static bool IsWaterBody(const UActorComponent* Component);

	/**
	 * Whether Mesh is the static mesh component a Water plugin body on the same actor draws itself with (a Custom body's MeshComp, or
	 * any body's water mesh override): it publishes on its own, as a mesh drawn with the body's water material.
	 */
	static bool IsBodyMeshComponent(const UStaticMeshComponent* Mesh);

	/** The body's mesh and material, or why not (see FMantleWaterBodyMesh). The generated mesh is cached per body and shape. */
	static FMantleWaterBodyMesh Resolve(const USceneComponent* Body);

	/** A flat mesh filling Polygon (body-space points, ordered around the loop) at Z, facing up; null with OutWhy set when it can't be made. */
	static UStaticMesh* BuildFlatMesh(const TArray<FVector2D>& Polygon, double Z, UMaterialInterface* Material, const FString& Name, FString& OutWhy);

	/** Forgets every cached mesh (tests). */
	static void ClearCache();
};
