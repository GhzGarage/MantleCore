// Copyright Joshua Eger. All Rights Reserved.
#include "MantleCoverage.h"
#include "MantlePlaceExporter.h"
#include "MantleWaterBody.h"
#include "MantleWaterBodyExport.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// A Water plugin body is left out of the place with one warning until the place format carries it (GhzGarage/mantle#997); an ordinary
// component is not a water body. The Water plugin is not linked, so the body classes are built only when the plugin is loaded.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleWaterBodyRecogniseTest,
	"Mantle.Editor.WaterBody.OnlyWaterBodiesAreWater",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleWaterBodyRecogniseTest::RunTest(const FString& Parameters)
{
	USceneComponent* Plain = NewObject<USceneComponent>(GetTransientPackage());
	TestFalse(TEXT("a scene component is not a water body"), FMantleWaterBody::IsWaterBody(Plain));
	TestFalse(TEXT("nor is nothing"), FMantleWaterBody::IsWaterBody(nullptr));
	TestTrue(TEXT("a plain component has no kind of water"), FMantleWaterBody::Kind(nullptr).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleWaterBodySkippedWithWarningTest,
	"Mantle.Editor.WaterBody.IsSkippedWithAWarning",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleWaterBodySkippedWithWarningTest::RunTest(const FString& Parameters)
{
	if (!FMantleWaterBodyExport::IsWaterPluginLoaded())
	{
		AddInfo(TEXT("The Water plugin is not loaded in this editor, so there is no water body to spawn; skipped."));
		return true;
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false, TEXT("MantleWaterBodySkipTestWorld"));
	if (!TestNotNull(TEXT("a world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(/*bInformEngineOfWorld*/ false);
		if (World->IsRooted())
		{
			World->RemoveFromRoot();
		}
	};

	const FVector Location = FVector::ZeroVector;
	UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/Water.WaterBodyLake"));
	AActor* Lake = Class ? World->SpawnActor(Class, &Location) : nullptr;
	if (!TestNotNull(TEXT("a lake"), Lake))
	{
		return false;
	}

	TArray<FMantlePrefabPart> Parts;
	TArray<FMantleUnpublishedPart> Unsupported;
	FMantlePlaceExporter::CollectPrefabParts(Lake, Parts, &Unsupported);
	TestEqual(TEXT("the lake publishes no parts"), Parts.Num(), 0);
	int32 Warnings = 0;
	for (const FMantleUnpublishedPart& Part : Unsupported)
	{
		if (Part.What.Contains(TEXT("water body")))
		{
			++Warnings;
			TestFalse(TEXT("it is a warning, not an error"), Part.bBlocksPublish);
			TestEqual(TEXT("under the Water category"), Part.Category, FString(MantleCoverageCategory::Water));
			TestTrue(TEXT("and says why"), Part.What.Contains(TEXT("waterBodies")));
		}
	}
	TestEqual(TEXT("one warning for the body"), Warnings, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
