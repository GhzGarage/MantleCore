// Copyright Joshua Eger. All Rights Reserved.
#include "MantleCheck.h"
#include "MantleClientFeatures.h"
#include "MantleDrawDistance.h"
#include "MantlePlaceExporter.h"
#include "MantlePlaceLimits.h"
#include "MantleTargetFeatures.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

// GhzGarage/mantle#630 stage 2: the draw, fade and shadow distances a mesh or instance set publishes, from the source component's
// cull distance or a size-based default, only for a client that reads drawDistances. All on plain numbers and built places.

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleDrawDistanceTestPrivate
{
	const TCHAR* const Mesh = TEXT("ast_u0123456789abcdef0123456789abcdef");

	FMantleDrawDistanceSource Source(const double Radius)
	{
		FMantleDrawDistanceSource Result;
		Result.SphereRadius = Radius;
		return Result;
	}

	FMantleStaticMeshProps Apply(const FMantleDrawDistanceSource& Source, const FMantleTargetFeatures& Target = FMantleTargetFeatures::All())
	{
		FMantleStaticMeshProps Props;
		Props.Mesh = Mesh;
		FMantleDrawDistance::Apply(Source, Target, Props);
		return Props;
	}

	/** The properties object of entity Index in the place file the exporter writes for Place. */
	TSharedPtr<FJsonObject> PropertiesOf(const FMantlePlace& Place, const int32 Index)
	{
		TSharedPtr<FJsonObject> Root;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FMantlePlaceExporter::ToJson(Place)), Root);
		if (!Root.IsValid())
		{
			return nullptr;
		}
		const TArray<TSharedPtr<FJsonValue>>* Entities = nullptr;
		if (!Root->TryGetArrayField(TEXT("entities"), Entities) || !Entities->IsValidIndex(Index))
		{
			return nullptr;
		}
		const TSharedPtr<FJsonObject>* Properties = nullptr;
		(*Entities)[Index]->AsObject()->TryGetObjectField(TEXT("properties"), Properties);
		return Properties ? *Properties : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleDrawDistanceSourceTest,
	"Mantle.Editor.DrawDistance.SourceCullDistanceIsCarried",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleDrawDistanceSourceTest::RunTest(const FString& Parameters)
{
	using namespace MantleDrawDistanceTestPrivate;

	// A small mesh whose component sets Desired Max Draw Distance: the band's default does not replace it.
	FMantleDrawDistanceSource Desired = Source(20.0);
	Desired.LDMaxDrawDistance = 9000.0;
	Desired.CachedMaxDrawDistance = 4000.0;
	FMantleStaticMeshProps Props = Apply(Desired);
	TestEqual(TEXT("the desired distance"), Props.DrawDistance.Get(0.0), 9000.0);

	// Only the current one (a cull distance volume): that one.
	Desired.LDMaxDrawDistance = 0.0;
	TestEqual(TEXT("the current distance"), Apply(Desired).DrawDistance.Get(0.0), 4000.0);

	// A large mesh with a cull distance is culled there, though its size alone would never cull it.
	FMantleDrawDistanceSource Big = Source(5000.0);
	Big.LDMaxDrawDistance = 150000.0;
	TestEqual(TEXT("a big mesh's own distance"), Apply(Big).DrawDistance.Get(0.0), 150000.0);

	// Never Distance Cull wins over both the source and the default.
	FMantleDrawDistanceSource Never = Source(20.0);
	Never.LDMaxDrawDistance = 9000.0;
	Never.bNeverDistanceCull = true;
	TestFalse(TEXT("never cull publishes no draw distance"), Apply(Never).DrawDistance.IsSet());

	// Past the schema's largest, and below its smallest.
	FMantleDrawDistanceSource Far = Source(20.0);
	Far.LDMaxDrawDistance = FMantlePlaceLimits::MaxDrawDistance * 2.0;
	TestFalse(TEXT("a distance past the limit is no cull"), Apply(Far).DrawDistance.IsSet());
	Far.LDMaxDrawDistance = 5.0;
	TestEqual(TEXT("a tiny one is raised to the minimum"), Apply(Far).DrawDistance.Get(0.0), FMantlePlaceLimits::MinDrawDistance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleDrawDistanceDefaultTest,
	"Mantle.Editor.DrawDistance.SizeBasedDefaultByBand",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleDrawDistanceDefaultTest::RunTest(const FString& Parameters)
{
	using namespace MantleDrawDistanceTestPrivate;

	// Each band: a radius inside it gets its draw and shadow distance, and a bigger mesh a longer one.
	double Previous = 0.0;
	for (const FMantleDrawDistance::FBand& Band : FMantleDrawDistance::Bands())
	{
		const FMantleStaticMeshProps Props = Apply(Source(Band.MaxRadius * 0.5));
		TestEqual(*FString::Printf(TEXT("draw distance below radius %g"), Band.MaxRadius), Props.DrawDistance.Get(0.0), Band.Draw);
		TestEqual(*FString::Printf(TEXT("shadow distance below radius %g"), Band.MaxRadius), Props.ShadowDistance.Get(0.0), Band.Shadow);
		TestTrue(TEXT("a shadow stops before the mesh does"), Band.Shadow < Band.Draw);
		TestTrue(TEXT("a bigger band culls farther"), Band.Draw > Previous);
		TestFalse(TEXT("no fade without a source start"), Props.FadeStart.IsSet());
		Previous = Band.Draw;
	}

	// The edge of a band belongs to the next one.
	const double FirstEdge = FMantleDrawDistance::Bands()[0].MaxRadius;
	TestEqual(TEXT("the edge goes to the bigger band"), Apply(Source(FirstEdge)).DrawDistance.Get(0.0), FMantleDrawDistance::Bands()[1].Draw);

	// A mesh that does not cast shadows gets no shadow distance.
	FMantleDrawDistanceSource NoShadow = Source(20.0);
	NoShadow.bCastShadow = false;
	const FMantleStaticMeshProps Dark = Apply(NoShadow);
	TestTrue(TEXT("still culled"), Dark.DrawDistance.IsSet());
	TestFalse(TEXT("but no shadow distance"), Dark.ShadowDistance.IsSet());

	// A source cull nearer than the shadow default leaves the shadow distance out: the mesh is gone first.
	FMantleDrawDistanceSource Near = Source(20.0);
	Near.LDMaxDrawDistance = 1000.0;
	TestFalse(TEXT("no shadow distance past the draw distance"), Apply(Near).ShadowDistance.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleDrawDistanceHugeTest,
	"Mantle.Editor.DrawDistance.HugeMeshGetsNone",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleDrawDistanceHugeTest::RunTest(const FString& Parameters)
{
	using namespace MantleDrawDistanceTestPrivate;
	const double Radius = FMantleDrawDistance::NeverCullRadius();
	for (const double Size : { Radius, Radius * 2.0, 100000.0 })
	{
		const FMantleStaticMeshProps Props = Apply(Source(Size));
		TestFalse(*FString::Printf(TEXT("radius %g: no draw distance"), Size), Props.DrawDistance.IsSet());
		TestFalse(*FString::Printf(TEXT("radius %g: no shadow distance"), Size), Props.ShadowDistance.IsSet());
	}
	TestTrue(TEXT("just below it, still culled"), Apply(Source(Radius - 1.0)).DrawDistance.IsSet());

	// An unmeasured mesh (no radius) is the smallest band, never an unculled one by accident of 0.
	TestTrue(TEXT("a mesh with no bounds is treated as small"), Apply(Source(0.0)).DrawDistance.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleDrawDistanceFoliageTest,
	"Mantle.Editor.DrawDistance.InstanceEndCullDistanceAndFade",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleDrawDistanceFoliageTest::RunTest(const FString& Parameters)
{
	using namespace MantleDrawDistanceTestPrivate;

	// Painted foliage: the foliage type's start and end cull distance, on the instanced component.
	FMantleDrawDistanceSource Foliage = Source(150.0);
	Foliage.bInstanced = true;
	Foliage.InstanceStartCullDistance = 8000;
	Foliage.InstanceEndCullDistance = 12000;
	FMantleStaticMeshProps Props = Apply(Foliage);
	TestEqual(TEXT("the end distance is the draw distance"), Props.DrawDistance.Get(0.0), 12000.0);
	TestEqual(TEXT("the start distance is the fade start"), Props.FadeStart.Get(0.0), 8000.0);
	TestTrue(TEXT("a tree casts no shadow far out"), Props.ShadowDistance.IsSet() && Props.ShadowDistance.GetValue() < 12000.0);

	// Only an end: no fade. A start at or past the end: no fade.
	Foliage.InstanceStartCullDistance = 0;
	TestFalse(TEXT("no start, no fade"), Apply(Foliage).FadeStart.IsSet());
	Foliage.InstanceStartCullDistance = 12000;
	TestFalse(TEXT("a start at the end is no fade"), Apply(Foliage).FadeStart.IsSet());

	// No cull set on an instanced component: the size default, and no fade.
	Foliage.InstanceStartCullDistance = 0;
	Foliage.InstanceEndCullDistance = 0;
	Props = Apply(Foliage);
	TestEqual(TEXT("the band's draw distance"), Props.DrawDistance.Get(0.0), FMantleDrawDistance::Bands()[2].Draw);
	TestFalse(TEXT("and no fade"), Props.FadeStart.IsSet());

	// Instance cull distances mean nothing on a plain mesh component.
	FMantleDrawDistanceSource Plain = Source(150.0);
	Plain.InstanceEndCullDistance = 12000;
	TestEqual(TEXT("a plain mesh ignores them"), Apply(Plain).DrawDistance.Get(0.0), FMantleDrawDistance::Bands()[2].Draw);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleDrawDistanceGatedTest,
	"Mantle.Editor.DrawDistance.NothingWrittenWithoutTheFeature",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleDrawDistanceGatedTest::RunTest(const FString& Parameters)
{
	using namespace MantleDrawDistanceTestPrivate;
	FMantleDrawDistanceSource Foliage = Source(20.0);
	Foliage.bInstanced = true;
	Foliage.InstanceStartCullDistance = 1000;
	Foliage.InstanceEndCullDistance = 5000;
	Foliage.LDMaxDrawDistance = 3000.0;

	// A client that reads no features, and one that reads every feature but this one.
	TArray<FString> AllButThis = MantleClientFeatures::All();
	AllButThis.Remove(MantleClientFeatures::DrawDistances);
	for (const FMantleTargetFeatures& Target : { FMantleTargetFeatures::None(), FMantleTargetFeatures::FromNames(AllButThis) })
	{
		FMantleStaticMeshProps Props;
		Props.DrawDistance = 1.0e3;
		Props.FadeStart = 1.0e2;
		Props.ShadowDistance = 1.0e2;
		TestFalse(TEXT("nothing set"), FMantleDrawDistance::Apply(Foliage, Target, Props));
		TestFalse(TEXT("no draw distance"), Props.DrawDistance.IsSet());
		TestFalse(TEXT("no fade start"), Props.FadeStart.IsSet());
		TestFalse(TEXT("no shadow distance"), Props.ShadowDistance.IsSet());
	}

	// The default target (a bare export, Publish) reads it, and so does a target that names it.
	TestTrue(TEXT("the default target reads it"), Apply(Foliage).DrawDistance.IsSet());
	TestTrue(TEXT("a named target reads it"),
		Apply(Foliage, FMantleTargetFeatures::FromNames({ MantleClientFeatures::DrawDistances })).DrawDistance.IsSet());

	// And the written place file has none of the keys.
	FMantlePlace Place;
	Place.SchemaVersion = MANTLE_PLACE_SCHEMA_VERSION;
	FMantleEntity Entity;
	Entity.Id = TEXT("ent_gated");
	Entity.Type = EMantleEntityType::StaticMesh;
	Entity.Props.Set<FMantleStaticMeshProps>(Apply(Foliage, FMantleTargetFeatures::None()));
	Place.Entities.Add(MoveTemp(Entity));
	const TSharedPtr<FJsonObject> Properties = PropertiesOf(Place, 0);
	if (TestNotNull(TEXT("the entity's properties"), Properties.Get()))
	{
		TestFalse(TEXT("no drawDistance"), Properties->HasField(TEXT("drawDistance")));
		TestFalse(TEXT("no fadeStart"), Properties->HasField(TEXT("fadeStart")));
		TestFalse(TEXT("no shadowDistance"), Properties->HasField(TEXT("shadowDistance")));
	}
	TestEqual(TEXT("and the check counts none"), FMantleDrawDistance::CountEntities(Place), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleDrawDistanceJsonTest,
	"Mantle.Editor.DrawDistance.KeysAreWrittenAndCounted",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleDrawDistanceJsonTest::RunTest(const FString& Parameters)
{
	using namespace MantleDrawDistanceTestPrivate;
	FMantlePlace Place;
	Place.SchemaVersion = MANTLE_PLACE_SCHEMA_VERSION;

	// A mesh with a draw and a shadow distance, an instance set with a fade, and a mesh with none (a big one).
	FMantleStaticMeshProps Small = Apply(Source(20.0));
	FMantleEntity SmallEntity;
	SmallEntity.Id = TEXT("ent_small");
	SmallEntity.Type = EMantleEntityType::StaticMesh;
	SmallEntity.Props.Set<FMantleStaticMeshProps>(Small);
	Place.Entities.Add(MoveTemp(SmallEntity));

	FMantleDrawDistanceSource Foliage = Source(20.0);
	Foliage.bInstanced = true;
	Foliage.InstanceStartCullDistance = 3000;
	Foliage.InstanceEndCullDistance = 5000;
	FMantleInstancedMeshProps Set;
	Set.Mesh = Apply(Foliage);
	Set.Instances.Add(FTransform::Identity);
	FMantleEntity SetEntity;
	SetEntity.Id = TEXT("ent_set");
	SetEntity.Type = EMantleEntityType::InstancedMesh;
	SetEntity.Props.Set<FMantleInstancedMeshProps>(Set);
	Place.Entities.Add(MoveTemp(SetEntity));

	FMantleEntity BigEntity;
	BigEntity.Id = TEXT("ent_big");
	BigEntity.Type = EMantleEntityType::StaticMesh;
	BigEntity.Props.Set<FMantleStaticMeshProps>(Apply(Source(5000.0)));
	Place.Entities.Add(MoveTemp(BigEntity));

	const TSharedPtr<FJsonObject> SmallJson = PropertiesOf(Place, 0);
	if (TestNotNull(TEXT("the mesh's properties"), SmallJson.Get()))
	{
		TestEqual(TEXT("drawDistance"), SmallJson->GetNumberField(TEXT("drawDistance")), FMantleDrawDistance::Bands()[0].Draw);
		TestEqual(TEXT("shadowDistance"), SmallJson->GetNumberField(TEXT("shadowDistance")), FMantleDrawDistance::Bands()[0].Shadow);
		TestFalse(TEXT("a mesh has no fadeStart"), SmallJson->HasField(TEXT("fadeStart")));
	}
	const TSharedPtr<FJsonObject> SetJson = PropertiesOf(Place, 1);
	if (TestNotNull(TEXT("the instance set's properties"), SetJson.Get()))
	{
		TestEqual(TEXT("drawDistance"), SetJson->GetNumberField(TEXT("drawDistance")), 5000.0);
		TestEqual(TEXT("fadeStart"), SetJson->GetNumberField(TEXT("fadeStart")), 3000.0);
		TestTrue(TEXT("shadowDistance"), SetJson->HasField(TEXT("shadowDistance")));
	}
	const TSharedPtr<FJsonObject> BigJson = PropertiesOf(Place, 2);
	if (TestNotNull(TEXT("the big mesh's properties"), BigJson.Get()))
	{
		TestFalse(TEXT("no drawDistance"), BigJson->HasField(TEXT("drawDistance")));
		TestFalse(TEXT("no shadowDistance"), BigJson->HasField(TEXT("shadowDistance")));
	}

	TestEqual(TEXT("two entities got a draw distance"), FMantleDrawDistance::CountEntities(Place), 2);

	// Different distances are different instance sets.
	FMantleStaticMeshProps Other = Small;
	Other.DrawDistance = 7000.0;
	TestNotEqual(TEXT("another distance is another group"), FMantlePlaceExporter::InstancedGroupKey(Small), FMantlePlaceExporter::InstancedGroupKey(Other));
	FMantleStaticMeshProps NoDistance;
	NoDistance.Mesh = Mesh;
	FMantleStaticMeshProps Same = NoDistance;
	TestEqual(TEXT("and no distance keeps the key as it was"), FMantlePlaceExporter::InstancedGroupKey(NoDistance), FMantlePlaceExporter::InstancedGroupKey(Same));

	// The count reaches Mantle.Check's last line.
	FMantleCheckReport Report;
	Report.DrawDistances = FMantleDrawDistance::CountEntities(Place);
	TestTrue(TEXT("Mantle.Check reports it"), FMantleCheck::DoneLine(Report).EndsWith(TEXT(" drawDistances=2")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
