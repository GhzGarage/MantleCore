// Copyright Joshua Eger. All Rights Reserved.
#include "MantleWaterBody.h"

#include "MantlePlaceAssets.h"
#include "MantleRuntime.h"

#include "Components/SplineComponent.h"
#include "CompGeom/PolygonTriangulation.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/SecureHash.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleWaterBodyPrivate
{
	/** Bumped when what a generated water mesh holds changes, so a cached one from before is not reused. */
	const TCHAR* const BakeVersion = TEXT("waterbody-1");

	/** The slot name a generated mesh's one polygon group and material slot share, so the build maps the section to the slot. */
	const FName SlotName(TEXT("MantleWaterSlot_0"));

	/** A lake's outline is sampled about this far apart (cm), within MinSamples to MaxSamples points. */
	constexpr double SampleSpacing = 150.0;
	constexpr int32 MinSamples = 24;
	constexpr int32 MaxSamples = 512;

	TMap<FString, TStrongObjectPtr<UStaticMesh>>& Cache()
	{
		static TMap<FString, TStrongObjectPtr<UStaticMesh>> Meshes;
		return Meshes;
	}

	bool DerivesFrom(const UClass* Class, const TCHAR* Name)
	{
		for (; Class; Class = Class->GetSuperClass())
		{
			if (Class->GetFName() == FName(Name))
			{
				return true;
			}
		}
		return false;
	}

	/** An object property of the body by name (the Water module's members are not linked, so they are read by reflection). */
	UObject* ReadObject(const UObject* Body, const TCHAR* Property)
	{
		const FObjectProperty* Found = CastField<FObjectProperty>(Body->GetClass()->FindPropertyByName(FName(Property)));
		return Found ? Found->GetObjectPropertyValue_InContainer(Body) : nullptr;
	}

	FString KindOf(const UClass* Class)
	{
		if (DerivesFrom(Class, TEXT("WaterBodyCustomComponent")))
		{
			return TEXT("custom");
		}
		if (DerivesFrom(Class, TEXT("WaterBodyLakeComponent")))
		{
			return TEXT("lake");
		}
		if (DerivesFrom(Class, TEXT("WaterBodyRiverComponent")))
		{
			return TEXT("river");
		}
		if (DerivesFrom(Class, TEXT("WaterBodyOceanComponent")))
		{
			return TEXT("ocean");
		}
		return TEXT("water");
	}
}

bool FMantleWaterBody::IsWaterBody(const UActorComponent* Component)
{
	return Component && MantleWaterBodyPrivate::DerivesFrom(Component->GetClass(), TEXT("WaterBodyComponent"));
}

UStaticMesh* FMantleWaterBody::BuildFlatMesh(const TArray<FVector2D>& Polygon, double Z, UMaterialInterface* Material, const FString& Name, FString& OutWhy)
{
	using namespace MantleWaterBodyPrivate;
	OutWhy.Reset();
	if (Polygon.Num() < 3)
	{
		OutWhy = TEXT("its outline has fewer than three points");
		return nullptr;
	}

	TArray<UE::Math::TVector2<double>> Points;
	Points.Reserve(Polygon.Num());
	for (const FVector2D& Point : Polygon)
	{
		Points.Emplace(Point.X, Point.Y);
	}
	TArray<UE::Geometry::FIndex3i> Triangles;
	PolygonTriangulation::TriangulateSimplePolygon<double>(Points, Triangles, /*bOrientAsHoleFill*/ false);
	if (Triangles.Num() == 0)
	{
		OutWhy = TEXT("its outline could not be filled (it crosses itself)");
		return nullptr;
	}

	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();
	TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> Signs = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
	UVs.SetNumChannels(1);

	const FPolygonGroupID Group = Description.CreatePolygonGroup();
	Attributes.GetPolygonGroupMaterialSlotNames()[Group] = SlotName;

	TArray<FVertexID> Vertices;
	for (const FVector2D& Point : Polygon)
	{
		const FVertexID Vertex = Description.CreateVertex();
		Positions[Vertex] = FVector3f(static_cast<float>(Point.X), static_cast<float>(Point.Y), static_cast<float>(Z));
		Vertices.Add(Vertex);
	}
	for (const UE::Geometry::FIndex3i& Triangle : Triangles)
	{
		int32 Order[3] = { Triangle.A, Triangle.B, Triangle.C };
		// Unreal's front face is the one a counter-clockwise cross product points out of: the surface is seen from above.
		const FVector2D AB = Polygon[Order[1]] - Polygon[Order[0]];
		const FVector2D AC = Polygon[Order[2]] - Polygon[Order[0]];
		if (AB.X * AC.Y - AB.Y * AC.X < 0.0)
		{
			Swap(Order[1], Order[2]);
		}
		TArray<FVertexInstanceID> Corners;
		for (const int32 Index : Order)
		{
			const FVertexInstanceID Corner = Description.CreateVertexInstance(Vertices[Index]);
			Normals[Corner] = FVector3f(0.0f, 0.0f, 1.0f);
			Tangents[Corner] = FVector3f(1.0f, 0.0f, 0.0f);
			Signs[Corner] = 1.0f;
			// A texture repeat per metre, laid out by position so the mesh tiles the way a water material expects.
			UVs.Set(Corner, 0, FVector2f(static_cast<float>(Polygon[Index].X / 100.0), static_cast<float>(Polygon[Index].Y / 100.0)));
			Corners.Add(Corner);
		}
		Description.CreatePolygon(Group, Corners);
	}

	UObject* Outer = GetTransientPackage();
	UStaticMesh* Baked = NewObject<UStaticMesh>(Outer, FMantlePlaceAssets::UniqueBakedMeshName(Outer, Name), RF_Transient);
	Baked->GetStaticMaterials().Add(FStaticMaterial(Material, SlotName, SlotName));
	Baked->bAllowCPUAccess = true;
	Baked->SetNumSourceModels(1);
	Baked->GetSourceModel(0).ReductionSettings = FMeshReductionSettings();
	FMeshBuildSettings& Settings = Baked->GetSourceModel(0).BuildSettings;
	Settings.bRecomputeNormals = false;
	Settings.bRecomputeTangents = false;
	Settings.bGenerateLightmapUVs = false;
	Settings.bBuildReversedIndexBuffer = false;
	Settings.DistanceFieldResolutionScale = 0.0f;
	Settings.MaxLumenMeshCards = 0;

	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bAllowCpuAccess = true;
	TArray<const FMeshDescription*> Pointers;
	Pointers.Add(&Description);
	if (!Baked->BuildFromMeshDescriptions(Pointers, Params))
	{
		OutWhy = TEXT("the engine could not build a mesh from its outline");
		return nullptr;
	}
	return Baked;
}

FMantleWaterBodyMesh FMantleWaterBody::Resolve(const USceneComponent* Body)
{
	using namespace MantleWaterBodyPrivate;
	FMantleWaterBodyMesh Out;
	if (!IsWaterBody(Body))
	{
		Out.Why = TEXT("it is not a water body");
		return Out;
	}
	Out.Kind = KindOf(Body->GetClass());
	Out.Material = Cast<UMaterialInterface>(ReadObject(Body, TEXT("WaterMaterial")));

	// The body's own mesh: all a Custom body draws, and what any body draws when its water mesh override is set.
	if (UStaticMesh* Own = Cast<UStaticMesh>(ReadObject(Body, TEXT("WaterMeshOverride"))))
	{
		Out.Mesh = Own;
		Out.bOwnMesh = true;
		return Out;
	}

	if (Out.Kind == TEXT("custom"))
	{
		Out.Why = TEXT("it has no water mesh override, so there is no mesh to draw");
		return Out;
	}
	if (Out.Kind != TEXT("lake"))
	{
		Out.Why = FString::Printf(TEXT("a %s is drawn at run time by the water mesh from the landscape and its water zone, so no mesh of it exists to publish"), *Out.Kind);
		return Out;
	}

	// A lake: its spline outline, filled flat at the spline's height.
	const AActor* Owner = Body->GetOwner();
	const USplineComponent* Spline = Owner ? Owner->FindComponentByClass<USplineComponent>() : nullptr;
	if (!Spline || Spline->GetNumberOfSplinePoints() < 3)
	{
		Out.Why = TEXT("its spline has fewer than three points");
		return Out;
	}
	if (!Spline->IsClosedLoop())
	{
		Out.Why = TEXT("its spline is not a closed loop");
		return Out;
	}
	const float Length = Spline->GetSplineLength();
	const int32 Samples = FMath::Clamp(FMath::RoundToInt(Length / SampleSpacing), MinSamples, MaxSamples);
	const FTransform BodyToWorld = Body->GetComponentTransform();
	TArray<FVector2D> Outline;
	double SumZ = 0.0;
	for (int32 Sample = 0; Sample < Samples; ++Sample)
	{
		const FVector Local = BodyToWorld.InverseTransformPosition(
			Spline->GetLocationAtDistanceAlongSpline(Length * Sample / Samples, ESplineCoordinateSpace::World));
		Outline.Emplace(Local.X, Local.Y);
		SumZ += Local.Z;
	}
	const double Z = SumZ / Samples;

	FString ShapeKey = FString::Printf(TEXT("%s|%g|%s"), BakeVersion, Z, Out.Material ? *Out.Material->GetPathName() : TEXT(""));
	for (const FVector2D& Point : Outline)
	{
		ShapeKey += FString::Printf(TEXT("|%.1f,%.1f"), Point.X, Point.Y);
	}
	const FString Key = Body->GetPathName() + TEXT("|") + FMD5::HashAnsiString(*ShapeKey);
	if (const TStrongObjectPtr<UStaticMesh>* Found = Cache().Find(Key))
	{
		if (IsValid(Found->Get()))
		{
			Out.Mesh = Found->Get();
			return Out;
		}
	}
	FString Why;
	UStaticMesh* Built = BuildFlatMesh(Outline, Z, Out.Material, FString::Printf(TEXT("MantleWater_%s"), *Owner->GetName().Left(60)), Why);
	if (!Built)
	{
		Out.Why = Why;
		return Out;
	}
	UE_LOG(LogMantle, Log, TEXT("MANTLE_818 WaterBody: lake '%s' filled into '%s' (%d outline points)"), *Body->GetPathName(), *Built->GetName(), Outline.Num());
	Cache().Add(Key, TStrongObjectPtr<UStaticMesh>(Built));
	Out.Mesh = Built;
	return Out;
}

void FMantleWaterBody::ClearCache()
{
	MantleWaterBodyPrivate::Cache().Empty();
}
