// Copyright Joshua Eger. All Rights Reserved.
#include "MantleGarmentOutfit.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// "Make Outfit from garment": posing a garment the way its body was, moving it onto the MetaHuman, and taking the
// MetaHuman's skin weights, on synthetic meshes. No assets.

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleGarmentOutfitTestPrivate
{
	/** Two joints: a root at the origin and an arm 10 cm up the Z axis; vertex 0 on the root, vertex 1 on the arm. */
	FMantleGlbMesh GarmentTestMesh()
	{
		FMantleGlbMesh Mesh;
		Mesh.Joints.Add({ FName(TEXT("root")), INDEX_NONE, FTransform::Identity });
		Mesh.Joints.Add({ FName(TEXT("arm")), 0, FTransform(FVector(0, 0, 10)) });
		Mesh.Positions = { FVector3f(1, 0, 0), FVector3f(1, 0, 15) };
		Mesh.Normals = { FVector3f(1, 0, 0), FVector3f(1, 0, 0) };
		Mesh.NumInfluences = 1;
		Mesh.InfluenceJoints = { 0, 1 };
		Mesh.InfluenceWeights = { 1.0f, 1.0f };
		return Mesh;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleGarmentOutfitPoseTest,
	"Mantle.Editor.GarmentOutfit.PoseMesh",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleGarmentOutfitPoseTest::RunTest(const FString& Parameters)
{
	using namespace MantleGarmentOutfitTestPrivate;
	FString Error;

	// The bind pose itself, scaled by 2: every vertex twice as far from the origin.
	FMantleGlbMesh Bind = GarmentTestMesh();
	const TArray<FTransform> BindPose = { FTransform::Identity, FTransform(FVector(0, 0, 10)) };
	TestTrue(TEXT("the bind pose poses"), FMantleGarmentOutfit::PoseMesh(Bind, BindPose, 2.0, Error));
	TestTrue(TEXT("root vertex scaled"), Bind.Positions[0].Equals(FVector3f(2, 0, 0), 1e-4f));
	TestTrue(TEXT("arm vertex scaled"), Bind.Positions[1].Equals(FVector3f(2, 0, 30), 1e-4f));

	// The arm turned 90 degrees about Y at its joint: its vertex (1, 0, 5 above the joint) swings to (5, 0, -1) from it.
	FMantleGlbMesh Turned = GarmentTestMesh();
	const TArray<FTransform> TurnedPose = { FTransform::Identity, FTransform(FQuat(FVector::YAxisVector, UE_HALF_PI), FVector(0, 0, 10)) };
	TestTrue(TEXT("a turned arm poses"), FMantleGarmentOutfit::PoseMesh(Turned, TurnedPose, 1.0, Error));
	TestTrue(TEXT("root vertex stays"), Turned.Positions[0].Equals(FVector3f(1, 0, 0), 1e-4f));
	TestTrue(TEXT("arm vertex swings with its bone"), Turned.Positions[1].Equals(FVector3f(5, 0, 9), 1e-3f));
	TestTrue(TEXT("its normal turns with it"), Turned.Normals[1].Equals(FVector3f(0, 0, -1), 1e-3f));

	// A pose for the wrong number of joints is refused.
	FMantleGlbMesh Wrong = GarmentTestMesh();
	TestFalse(TEXT("a pose that does not match is refused"), FMantleGarmentOutfit::PoseMesh(Wrong, { FTransform::Identity }, 1.0, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleGarmentOutfitAlignTest,
	"Mantle.Editor.GarmentOutfit.AlignTranslation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleGarmentOutfitAlignTest::RunTest(const FString& Parameters)
{
	// A 10 x 10 x 10 grid of points 2 cm apart, and the same grid moved by (1.5, -2, 3): the move is found.
	TArray<FVector3f> To;
	for (int32 X = 0; X < 10; ++X)
	{
		for (int32 Y = 0; Y < 10; ++Y)
		{
			for (int32 Z = 0; Z < 10; ++Z)
			{
				To.Add(FVector3f(X * 2.0f, Y * 2.0f, Z * 2.0f));
			}
		}
	}
	const FVector3f Shift(1.5f, -2.0f, 3.0f);
	TArray<FVector3f> From;
	for (const FVector3f& Point : To)
	{
		From.Add(Point - Shift);
	}
	double Mean = -1.0;
	const FVector3f Found = FMantleGarmentOutfit::AlignTranslation(From, To, Mean);
	TestTrue(FString::Printf(TEXT("the shift is found (%s)"), *Found.ToString()), Found.Equals(Shift, 0.05f));
	TestTrue(TEXT("the points then lie on each other"), Mean < 0.05);

	double Empty = -1.0;
	TestTrue(TEXT("nothing to align moves nothing"), FMantleGarmentOutfit::AlignTranslation({}, To, Empty).IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleGarmentOutfitWeightsTest,
	"Mantle.Editor.GarmentOutfit.TransferWeights",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleGarmentOutfitWeightsTest::RunTest(const FString& Parameters)
{
	// A body: a row of skin vertices at x = 0..40 cm, the first half on "thigh", the rest on "calf".
	FMantleGlbMesh Body;
	Body.Joints.Add({ FName(TEXT("pelvis")), INDEX_NONE, FTransform::Identity });
	Body.Joints.Add({ FName(TEXT("thigh")), 0, FTransform::Identity });
	Body.Joints.Add({ FName(TEXT("calf")), 1, FTransform(FVector(20, 0, 0)) });
	Body.NumInfluences = 1;
	for (int32 X = 0; X <= 40; ++X)
	{
		Body.Positions.Add(FVector3f(X, 0, 0));
		Body.InfluenceJoints.Add(X < 20 ? 1 : 2);
		Body.InfluenceWeights.Add(1.0f);
	}

	// A garment on its own bones: one vertex over the thigh, one over the calf, one 50 cm away.
	FMantleGlbMesh Garment;
	Garment.Joints.Add({ FName(TEXT("own_root")), INDEX_NONE, FTransform::Identity });
	Garment.Positions = { FVector3f(5, 0, 1), FVector3f(35, 0, 1), FVector3f(40, 0, 50) };
	Garment.NumInfluences = 1;
	Garment.InfluenceJoints = { 0, 0, 0 };
	Garment.InfluenceWeights = { 1.0f, 1.0f, 1.0f };

	int32 Far = -1;
	double Mean = -1.0;
	FString Error;
	TestTrue(TEXT("the weights transfer"), FMantleGarmentOutfit::TransferWeights(Garment, Body, Far, Mean, Error));
	TestEqual(TEXT("the garment takes the body's joints"), Garment.Joints.Num(), 3);
	TestEqual(TEXT("one slot is all any vertex uses"), Garment.NumInfluences, 1);
	TestEqual(TEXT("the vertex over the thigh follows the thigh"), static_cast<int32>(Garment.InfluenceJoints[0]), 1);
	TestEqual(TEXT("the vertex over the calf follows the calf"), static_cast<int32>(Garment.InfluenceJoints[1]), 2);
	TestEqual(TEXT("the far vertex still follows its nearest skin"), static_cast<int32>(Garment.InfluenceJoints[2]), 2);
	TestEqual(TEXT("one vertex is far from the skin"), Far, 1);
	TestTrue(TEXT("weights are whole"), FMath::IsNearlyEqual(Garment.InfluenceWeights[0], 1.0f, 1e-4f));

	// A body with no skin gives nothing to take.
	FMantleGlbMesh Bare;
	Bare.Positions = { FVector3f::ZeroVector };
	TestFalse(TEXT("a body without skin weights is refused"), FMantleGarmentOutfit::TransferWeights(Garment, Bare, Far, Mean, Error));
	return true;
}

#endif
