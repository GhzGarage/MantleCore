// Copyright Joshua Eger. All Rights Reserved.
#include "MantleCoverage.h"
#include "MantleTerrainExport.h"
#include "MantleWaterBodyExport.h"

#include "Components/BoxComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

// The Water plugin reader (GhzGarage/mantle#997, stage 2). The Water plugin is Experimental and off by default, so every test that spawns
// a body skips itself when the plugin's classes are not loaded; the data, summary and landscape-carve tests need no plugin. The merged
// heightfield itself is read by FMantleTerrainExport::ReadLandscape from the landscape's final textures (see the note in
// FMantleTerrainCarveCoverageTest): rebuilding a real landscape's edit layers needs the GPU, which an automation run may not have.

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleWaterBodyExportTestPrivate
{
	/** An empty editor world for one test, destroyed with it. */
	struct FWaterTestWorld
	{
		UWorld* World = nullptr;

		FWaterTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false, TEXT("MantleWaterBodyExportTestWorld"));
		}

		~FWaterTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(/*bInformEngineOfWorld*/ false);
				if (World->IsRooted())
				{
					World->RemoveFromRoot();
				}
			}
		}
	};

	AActor* SpawnWaterActor(UWorld* World, const TCHAR* ClassPath, const FVector& Location)
	{
		UClass* Class = FindObject<UClass>(nullptr, ClassPath);
		return Class ? World->SpawnActor(Class, &Location) : nullptr;
	}

	const FMantleWaterBody* FindBody(const FMantleWaterBodies& Water, const EMantleWaterBodyType Type)
	{
		return Water.Bodies.FindByPredicate([Type](const FMantleWaterBody& Body) { return Body.Type == Type; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleWaterBodyReadSpawnedBodiesTest,
	"Mantle.Editor.Water.ReadsSpawnedBodies",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleWaterBodyReadSpawnedBodiesTest::RunTest(const FString& Parameters)
{
	using namespace MantleWaterBodyExportTestPrivate;

	if (!FMantleWaterBodyExport::IsWaterPluginLoaded())
	{
		AddInfo(TEXT("The Water plugin is not loaded in this editor, so there are no water bodies to spawn; skipped."));
		return true;
	}
	FWaterTestWorld Fixture;
	if (!TestNotNull(TEXT("a world"), Fixture.World))
	{
		return false;
	}

	AActor* Lake = SpawnWaterActor(Fixture.World, TEXT("/Script/Water.WaterBodyLake"), FVector(0.0, 0.0, 100.0));
	AActor* River = SpawnWaterActor(Fixture.World, TEXT("/Script/Water.WaterBodyRiver"), FVector(10000.0, 0.0, 100.0));
	AActor* Ocean = SpawnWaterActor(Fixture.World, TEXT("/Script/Water.WaterBodyOcean"), FVector(0.0, 50000.0, 0.0));
	AActor* Custom = SpawnWaterActor(Fixture.World, TEXT("/Script/Water.WaterBodyCustom"), FVector(-10000.0, 0.0, 0.0));
	AActor* Zone = SpawnWaterActor(Fixture.World, TEXT("/Script/Water.WaterZone"), FVector(0.0, 0.0, 0.0));
	if (!TestTrue(TEXT("the bodies and the zone spawn"), Lake && River && Ocean && Custom && Zone))
	{
		return false;
	}

	// A custom body names the mesh it is: set on its property, as the details panel does.
	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("MantleWaterTestPool"));
	UActorComponent* CustomBody = Custom->FindComponentByClass(FindObject<UClass>(nullptr, TEXT("/Script/Water.WaterBodyCustomComponent")));
	if (!TestNotNull(TEXT("the custom body's component"), CustomBody))
	{
		return false;
	}
	if (const FObjectPropertyBase* MeshProperty = CastField<FObjectPropertyBase>(CustomBody->GetClass()->FindPropertyByName(TEXT("WaterMeshOverride"))))
	{
		MeshProperty->SetObjectPropertyValue(MeshProperty->ContainerPtrToValuePtr<void>(CustomBody), Mesh);
	}

	const FMantleWaterBodies Water = FMantleWaterBodyExport::Read(Fixture.World);
	TestTrue(TEXT("the plugin is seen as loaded"), Water.bWaterPluginLoaded);
	TestEqual(TEXT("one lake"), Water.Count(EMantleWaterBodyType::Lake), 1);
	TestEqual(TEXT("one river"), Water.Count(EMantleWaterBodyType::River), 1);
	TestEqual(TEXT("one ocean"), Water.Count(EMantleWaterBodyType::Ocean), 1);
	TestEqual(TEXT("one custom body"), Water.Count(EMantleWaterBodyType::Custom), 1);
	TestEqual(TEXT("one water zone"), Water.Zones.Num(), 1);
	TestTrue(TEXT("the check line counts them"), Water.SummaryLine().Contains(TEXT("ocean=1 lake=1 river=1 custom=1 zones=1 plugin=1")));

	if (const FMantleWaterBody* Body = FindBody(Water, EMantleWaterBodyType::Lake))
	{
		TestTrue(TEXT("the lake is where it was put"), Body->Transform.GetLocation().Equals(FVector(0.0, 0.0, 100.0), 1.0));
		TestTrue(TEXT("a lake's spline is closed"), Body->bClosedLoop);
		TestTrue(TEXT("with points"), Body->Points.Num() >= 3);
		TestTrue(TEXT("a lake carves the landscape by default"), Body->bAffectsLandscape);
		TestFalse(TEXT("and has a collision setting"), Body->Collision.Enabled.IsEmpty());
	}
	if (const FMantleWaterBody* Body = FindBody(Water, EMantleWaterBodyType::River))
	{
		TestFalse(TEXT("a river's spline is open"), Body->bClosedLoop);
		TestTrue(TEXT("with at least two points"), Body->Points.Num() >= 2);
		TestTrue(TEXT("and the Water plugin's metadata"), Body->bHasMetadata);
		if (Body->Points.Num() > 0)
		{
			TestTrue(TEXT("each point has a width"), Body->Points[0].Width > 0.f);
			TestTrue(TEXT("and a depth"), Body->Points[0].Depth > 0.f);
		}
	}
	if (const FMantleWaterBody* Body = FindBody(Water, EMantleWaterBodyType::Custom))
	{
		TestTrue(TEXT("a custom body names its mesh"), Body->MeshPath.Contains(TEXT("MantleWaterTestPool")));
		TestFalse(TEXT("and never carves the landscape"), Body->bAffectsLandscape);
	}
	if (const FMantleWaterBody* Body = FindBody(Water, EMantleWaterBodyType::Ocean))
	{
		TestTrue(TEXT("an ocean has an extent"), Body->OceanExtents.X > 0.0 && Body->OceanExtents.Y > 0.0);
	}
	if (Water.Zones.Num() == 1)
	{
		TestTrue(TEXT("the zone has an extent"), Water.Zones[0].Extent.X > 0.0 && Water.Zones[0].Extent.Y > 0.0);
	}

	// The writer-to-come's input is plain JSON.
	const FString Json = FMantleWaterBodyExport::ToJsonString(Water);
	TestTrue(TEXT("the JSON lists the bodies"), Json.Contains(TEXT("\"bodyType\"")) && Json.Contains(TEXT("\"zones\"")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleWaterBodyEmptyWorldTest,
	"Mantle.Editor.Water.EmptyWorldHasNoBodies",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleWaterBodyEmptyWorldTest::RunTest(const FString& Parameters)
{
	using namespace MantleWaterBodyExportTestPrivate;

	// With the plugin loaded or not, a level with no water reads as empty and does not fail.
	FWaterTestWorld Fixture;
	if (!TestNotNull(TEXT("a world"), Fixture.World))
	{
		return false;
	}
	const FMantleWaterBodies Water = FMantleWaterBodyExport::Read(Fixture.World);
	TestEqual(TEXT("no bodies"), Water.Bodies.Num(), 0);
	TestEqual(TEXT("no zones"), Water.Zones.Num(), 0);
	TestEqual(TEXT("the plugin flag follows the classes"), Water.bWaterPluginLoaded, FMantleWaterBodyExport::IsWaterPluginLoaded());
	const FMantleWaterBodies Nothing = FMantleWaterBodyExport::Read(nullptr);
	TestEqual(TEXT("no world, no bodies"), Nothing.Bodies.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleWaterBodyJsonTest,
	"Mantle.Editor.Water.JsonCarriesTheData",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleWaterBodyJsonTest::RunTest(const FString& Parameters)
{
	// Built by hand, so it runs without the Water plugin.
	FMantleWaterBodies Water;
	Water.bWaterPluginLoaded = true;

	FMantleWaterBody& River = Water.Bodies.AddDefaulted_GetRef();
	River.Type = EMantleWaterBodyType::River;
	River.Name = TEXT("Brook");
	River.bHasMetadata = true;
	FMantleWaterSplinePoint& Start = River.Points.AddDefaulted_GetRef();
	Start.Location = FVector(0.0, 0.0, 10.0);
	Start.Width = 300.f;
	Start.Depth = 120.f;
	Start.Velocity = 64.f;
	Start.AudioIntensity = 0.5f;
	River.Points.AddDefaulted();
	River.Waves.bHasWaves = true;
	River.Waves.WavesClass = TEXT("GerstnerWaterWaves");
	FMantleWaterWave& Wave = River.Waves.Waves.AddDefaulted_GetRef();
	Wave.WaveLength = 800.f;
	Wave.Amplitude = 12.f;
	Water.Bodies.AddDefaulted_GetRef().Type = EMantleWaterBodyType::Ocean;
	Water.Zones.AddDefaulted();

	TestEqual(TEXT("the check line"), Water.SummaryLine(), FString(TEXT("MANTLE_CHECK_WATER ocean=1 lake=0 river=1 custom=0 zones=1 plugin=1")));

	const TSharedRef<FJsonObject> Json = FMantleWaterBodyExport::ToJson(Water);
	const TArray<TSharedPtr<FJsonValue>>& Bodies = Json->GetArrayField(TEXT("bodies"));
	if (!TestEqual(TEXT("both bodies are written"), Bodies.Num(), 2))
	{
		return false;
	}
	// Sorted order is the reader's job; here the bodies are in the order they were added.
	const TSharedPtr<FJsonObject> First = Bodies[0]->AsObject();
	TestEqual(TEXT("the type"), First->GetStringField(TEXT("bodyType")), FString(TEXT("river")));
	const TArray<TSharedPtr<FJsonValue>>& Points = First->GetObjectField(TEXT("spline"))->GetArrayField(TEXT("points"));
	TestEqual(TEXT("both spline points"), Points.Num(), 2);
	TestEqual(TEXT("a river point has its width"), Points[0]->AsObject()->GetNumberField(TEXT("width")), 300.0);
	TestEqual(TEXT("its depth"), Points[0]->AsObject()->GetNumberField(TEXT("depth")), 120.0);
	TestEqual(TEXT("its velocity"), Points[0]->AsObject()->GetNumberField(TEXT("velocity")), 64.0);
	TestEqual(TEXT("and one wave"), First->GetObjectField(TEXT("waves"))->GetArrayField(TEXT("waves")).Num(), 1);
	TestFalse(TEXT("an ocean without waves writes none"), Bodies[1]->AsObject()->HasField(TEXT("waves")));
	TestEqual(TEXT("and no spline points of its own"), Bodies[1]->AsObject()->GetObjectField(TEXT("spline"))->GetArrayField(TEXT("points")).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleWaterBodyCarvingCountTest,
	"Mantle.Editor.Water.CountsBodiesThatCarveALandscape",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleWaterBodyCarvingCountTest::RunTest(const FString& Parameters)
{
	using namespace MantleWaterBodyExportTestPrivate;

	if (!FMantleWaterBodyExport::IsWaterPluginLoaded())
	{
		AddInfo(TEXT("The Water plugin is not loaded in this editor, so there are no water bodies to spawn; skipped."));
		return true;
	}
	FWaterTestWorld Fixture;
	if (!TestNotNull(TEXT("a world"), Fixture.World))
	{
		return false;
	}

	// A box stands in for the landscape: the count reads only its bounds on the ground plane.
	auto MakeGround = [&Fixture](const FVector& Centre)
	{
		AActor* Ground = Fixture.World->SpawnActor<AActor>(Centre, FRotator::ZeroRotator);
		UBoxComponent* Box = NewObject<UBoxComponent>(Ground, TEXT("Ground"));
		Box->SetBoxExtent(FVector(5000.0, 5000.0, 100.0));
		Ground->SetRootComponent(Box);
		Box->RegisterComponent();
		return Ground;
	};

	AActor* Lake = SpawnWaterActor(Fixture.World, TEXT("/Script/Water.WaterBodyLake"), FVector(0.0, 0.0, 0.0));
	AActor* Custom = SpawnWaterActor(Fixture.World, TEXT("/Script/Water.WaterBodyCustom"), FVector(0.0, 0.0, 0.0));
	if (!TestTrue(TEXT("the bodies spawn"), Lake && Custom))
	{
		return false;
	}
	AActor* Under = MakeGround(FVector(0.0, 0.0, 0.0));
	AActor* Far = MakeGround(FVector(5000000.0, 5000000.0, 0.0));
	TestEqual(TEXT("the lake carves the landscape it overlaps, and the custom body does not"), FMantleWaterBodyExport::CountBodiesCarving(Under), 1);
	TestEqual(TEXT("a landscape far from the water is not carved"), FMantleWaterBodyExport::CountBodiesCarving(Far), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleTerrainCarveCoverageTest,
	"Mantle.Editor.Water.TerrainCarveCoverage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleTerrainCarveCoverageTest::RunTest(const FString& Parameters)
{
	// FMantleTerrainExport::ReadLandscape reads the landscape's final textures (FLandscapeEditDataInterface with no edit layer guid),
	// which hold every edit layer merged, water brushes included. It rebuilds the layers first when the landscape is out of date and
	// water carves it. A landscape's layer merge renders on the GPU, so this test covers what ChooseLayers says about it from a
	// source built in code, not the render.
	FMantleTerrainSource Source;
	Source.Width = 4;
	Source.Height = 4;
	Source.Heights.Init(32768, 16);
	Source.LayerNames.Add(TEXT("Grass"));
	Source.LayerWeights.Emplace();
	Source.LayerWeights.Last().Init(255, 16);
	Source.bMaterialRead = true;
	FMantleLandscapeLayerLook Look;
	Look.Layer = TEXT("Grass");
	Look.bHasFlatColour = true;
	Look.FlatColour = FLinearColor(0.1f, 0.5f, 0.1f);
	Look.bHasAverage = true;
	Look.Average = Look.FlatColour;
	Source.MaterialRead.Layers.Add(Look);
	Source.MaterialRead.bFoundLayers = true;

	auto Choose = [](const FMantleTerrainSource& Read, FMantleCoverage& Coverage, TArray<FMantleValidationIssue>& Issues)
	{
		TArray<FMantleTerrainLayerChoice> Layers;
		return FMantleTerrainExport::ChooseLayers(nullptr, Read, TEXT("Landscape"), Layers, Issues, &Coverage);
	};

	// No water, no line.
	{
		FMantleCoverage Coverage;
		TArray<FMantleValidationIssue> Issues;
		TestTrue(TEXT("a plain landscape is chosen"), Choose(Source, Coverage, Issues));
		TestEqual(TEXT("with no water carve line"), static_cast<int64>(Coverage.Count(MantleCoverageCategory::WaterCarve, EMantleCoverageOutcome::Kept)), static_cast<int64>(0));
	}

	// Carved, up to date: a kept line, no warning.
	Source.WaterBodiesCarving = 2;
	{
		FMantleCoverage Coverage;
		TArray<FMantleValidationIssue> Issues;
		TestTrue(TEXT("a carved landscape is chosen"), Choose(Source, Coverage, Issues));
		TestEqual(TEXT("with a kept water carve line"), static_cast<int64>(Coverage.Count(MantleCoverageCategory::WaterCarve, EMantleCoverageOutcome::Kept)), static_cast<int64>(1));
		TestEqual(TEXT("and no approximated one"), static_cast<int64>(Coverage.Count(MantleCoverageCategory::WaterCarve, EMantleCoverageOutcome::Approximated)), static_cast<int64>(0));
		TestEqual(TEXT("and no warning"), Issues.Num(), 0);
	}

	// Carved, layers still out of date after the rebuild: approximated, with a warning that says what to do.
	Source.bCarveRefreshed = true;
	Source.bCarveStale = true;
	{
		FMantleCoverage Coverage;
		TArray<FMantleValidationIssue> Issues;
		TestTrue(TEXT("a stale carved landscape still publishes"), Choose(Source, Coverage, Issues));
		TestEqual(TEXT("with an approximated water carve line"), static_cast<int64>(Coverage.Count(MantleCoverageCategory::WaterCarve, EMantleCoverageOutcome::Approximated)), static_cast<int64>(1));
		TestEqual(TEXT("and one warning"), Issues.Num(), 1);
		TestTrue(TEXT("which is a warning, not an error"), Issues.Num() == 1 && Issues[0].Severity == EMantleIssueSeverity::Warning);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
