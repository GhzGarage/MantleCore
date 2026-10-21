// Copyright Joshua Eger. All Rights Reserved.
#include "MantleCoverage.h"
#include "MantlePlaceAssets.h"
#include "MantleWaterBody.h"

#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshResources.h"

#if WITH_DEV_AUTOMATION_TESTS

// A Water plugin lake without a mesh of its own publishes as its spline outline filled flat (FMantleWaterBody::BuildFlatMesh); a body
// the water mesh system draws at run time (river, ocean) is left out and said so (GhzGarage/mantle#818). The Water plugin is not linked,
// so the body classes themselves are not built here: the fill is tested on an outline, and a plain component is not a water body.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleWaterBodyFillTest,
	"Mantle.Editor.WaterBody.FillsAnOutlineFlat",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleWaterBodyFillTest::RunTest(const FString& Parameters)
{
	// A square: two triangles, one material slot, flat at the water's height.
	FString Why;
	const TArray<FVector2D> Square = { FVector2D(0.0, 0.0), FVector2D(1000.0, 0.0), FVector2D(1000.0, 1000.0), FVector2D(0.0, 1000.0) };
	UStaticMesh* Mesh = FMantleWaterBody::BuildFlatMesh(Square, 120.0, nullptr, TEXT("WaterTestSquare"), Why);
	if (!TestNotNull(*FString::Printf(TEXT("a square fills (%s)"), *Why), Mesh))
	{
		return false;
	}
	const FStaticMeshRenderData* Data = Mesh->GetRenderData();
	if (TestNotNull(TEXT("it has render data"), Data) && TestTrue(TEXT("with a first LOD"), Data->LODResources.IsValidIndex(0)))
	{
		TestEqual(TEXT("a square is two triangles"), Data->LODResources[0].GetNumTriangles(), 2);
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		TestTrue(TEXT("it is flat at the given height"), FMath::IsNearlyEqual(Bounds.Origin.Z, 120.0, 0.5) && Bounds.BoxExtent.Z < 0.5);
		TestTrue(TEXT("and as wide as the outline"), FMath::IsNearlyEqual(Bounds.BoxExtent.X, 500.0, 1.0) && FMath::IsNearlyEqual(Bounds.BoxExtent.Y, 500.0, 1.0));
	}
	TestEqual(TEXT("one material slot is published"), FMantlePlaceAssets::UploadSlots(Mesh).Num(), 1);

	// A concave L: six corners fill with four triangles.
	const TArray<FVector2D> Ell = { FVector2D(0.0, 0.0), FVector2D(2000.0, 0.0), FVector2D(2000.0, 1000.0), FVector2D(1000.0, 1000.0), FVector2D(1000.0, 2000.0), FVector2D(0.0, 2000.0) };
	UStaticMesh* Concave = FMantleWaterBody::BuildFlatMesh(Ell, 0.0, nullptr, TEXT("WaterTestEll"), Why);
	if (TestNotNull(*FString::Printf(TEXT("an L fills (%s)"), *Why), Concave) && Concave->GetRenderData() && Concave->GetRenderData()->LODResources.IsValidIndex(0))
	{
		TestEqual(TEXT("an L of six corners is four triangles"), Concave->GetRenderData()->LODResources[0].GetNumTriangles(), 4);
	}

	// Too few points: no mesh, and a reason.
	const TArray<FVector2D> Line = { FVector2D(0.0, 0.0), FVector2D(10.0, 0.0) };
	TestNull(TEXT("two points are not an outline"), FMantleWaterBody::BuildFlatMesh(Line, 0.0, nullptr, TEXT("WaterTestLine"), Why));
	TestFalse(TEXT("and it says why"), Why.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleWaterBodyRecogniseTest,
	"Mantle.Editor.WaterBody.OnlyWaterBodiesAreWater",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleWaterBodyRecogniseTest::RunTest(const FString& Parameters)
{
	USceneComponent* Plain = NewObject<USceneComponent>(GetTransientPackage());
	TestFalse(TEXT("a scene component is not a water body"), FMantleWaterBody::IsWaterBody(Plain));
	TestFalse(TEXT("nor is nothing"), FMantleWaterBody::IsWaterBody(nullptr));
	const FMantleWaterBodyMesh Resolved = FMantleWaterBody::Resolve(Plain);
	TestNull(TEXT("a plain component resolves to no mesh"), Resolved.Mesh);
	TestFalse(TEXT("and says why"), Resolved.Why.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
