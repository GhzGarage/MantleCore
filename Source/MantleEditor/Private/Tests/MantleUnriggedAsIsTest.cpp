// Copyright Joshua Eger. All Rights Reserved.
#include "MantleUnriggedAsIs.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Chunk 7: As-is's auto-rig takes the fitted body's skin weights at each point's closest point, and the head band goes to the
// head. A synthetic cylinder, no assets.

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleUnriggedAsIsTestPrivate
{
	constexpr int32 Pelvis = 0;
	constexpr int32 Spine = 1;
	constexpr int32 Head = 2;

	constexpr double Radius = 10.0;
	constexpr double Height = 180.0;
	constexpr double RingStep = 10.0;
	constexpr int32 Segments = 16;

	/**
	 * A 180 cm open cylinder about Z of radius 10, a ring every 10 cm, skinned to two bones: below 90 cm all Pelvis, above all
	 * Spine, the ring at 90 cm half each. The Head bone weighs nothing on it (the body's skin ends at the neck).
	 */
	FMantleAutoRigSkin Cylinder()
	{
		FMantleAutoRigSkin Skin;
		Skin.NumInfluences = 4;
		const int32 Rings = FMath::RoundToInt32(Height / RingStep) + 1;
		for (int32 Ring = 0; Ring < Rings; ++Ring)
		{
			const double Z = Ring * RingStep;
			for (int32 Segment = 0; Segment < Segments; ++Segment)
			{
				const double Angle = 2.0 * UE_DOUBLE_PI * Segment / Segments;
				const FVector3f Position(static_cast<float>(Radius * FMath::Cos(Angle)), static_cast<float>(Radius * FMath::Sin(Angle)), static_cast<float>(Z));
				TArray<TPair<int32, float>> Influences;
				if (Z < 90.0)
				{
					Influences.Emplace(Pelvis, 1.0f);
				}
				else if (Z > 90.0)
				{
					Influences.Emplace(Spine, 1.0f);
				}
				else
				{
					Influences.Emplace(Pelvis, 0.5f);
					Influences.Emplace(Spine, 0.5f);
				}
				Skin.AddVertex(Position, Influences);
			}
		}
		for (int32 Ring = 0; Ring + 1 < Rings; ++Ring)
		{
			for (int32 Segment = 0; Segment < Segments; ++Segment)
			{
				const int32 A = Ring * Segments + Segment;
				const int32 B = Ring * Segments + (Segment + 1) % Segments;
				const int32 C = A + Segments;
				const int32 D = B + Segments;
				Skin.Indices.Append({ A, B, D, A, D, C });
			}
		}
		return Skin;
	}

	/** Count points on a ring of radius R at height Z. */
	void AddRing(TArray<FVector3f>& Out, const double R, const double Z, const int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			// Between the skin's vertices, so the closest point is inside a triangle, not on a corner.
			const double Angle = 2.0 * UE_DOUBLE_PI * (Index + 0.5) / Count;
			Out.Emplace(static_cast<float>(R * FMath::Cos(Angle)), static_cast<float>(R * FMath::Sin(Angle)), static_cast<float>(Z));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleUnriggedAsIsTransferTest,
	"Mantle.Editor.UnriggedAsIs.TransferWeights",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleUnriggedAsIsTransferTest::RunTest(const FString& Parameters)
{
	using namespace MantleUnriggedAsIsTestPrivate;
	const FMantleAutoRigSkin Skin = Cylinder();
	TestEqual(TEXT("the cylinder has 19 rings of 16 vertices"), Skin.Positions.Num(), 19 * 16);
	TestEqual(TEXT("each vertex keeps its slots"), Skin.Bones.Num(), Skin.Positions.Num() * Skin.NumInfluences);

	// The model: a ring 2 cm outside the skin low down, one high up, one at the split, one inside the skin, and a head band above
	// the cut at 165 cm.
	constexpr int32 PerRing = 8;
	constexpr double HeadCutZ = 165.0;
	TArray<FVector3f> Points;
	AddRing(Points, Radius + 2.0, 35.0, PerRing);   // 0..7: pelvis
	AddRing(Points, Radius + 2.0, 145.0, PerRing);  // 8..15: spine
	AddRing(Points, Radius + 2.0, 90.0, PerRing);   // 16..23: half each
	AddRing(Points, Radius - 4.0, 45.0, PerRing);   // 24..31: inside, pelvis
	AddRing(Points, Radius + 3.0, 172.0, PerRing);  // 32..39: the head band
	Points.Emplace(0.0f, 0.0f, 200.0f);             // 40: the top of the head, above the skin's end
	Points.Emplace(40.0f, 0.0f, 120.0f);            // 41: a cape's edge, 30 cm out

	FMantleAutoRigWeights Weights;
	FString Error;
	if (!TestTrue(TEXT("the weights transfer"), FMantleUnriggedAsIs::TransferWeights(Points, Skin, Head, HeadCutZ, Weights, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("every point has its slots"), Weights.Bones.Num(), Points.Num() * FMantleUnriggedAsIs::MaxInfluences);
	TestEqual(TEXT("the head band and the top of the head go to the head"), Weights.HeadPoints, PerRing + 1);
	TestEqual(TEXT("the rest are weighted from the skin"), Weights.SkinPoints, Points.Num() - PerRing - 1);
	TestEqual(TEXT("only the cape's edge is far from the skin"), Weights.FarPoints, 1);
	TestTrue(TEXT("the cape's edge is 30 cm out"), FMath::IsNearlyEqual(Weights.MaxDistanceCm, 30.0, 0.5));

	for (int32 Point = 0; Point < Points.Num(); ++Point)
	{
		float Sum = 0.0f;
		for (int32 Slot = 0; Slot < Weights.NumInfluences; ++Slot)
		{
			Sum += Weights.Weights[Point * Weights.NumInfluences + Slot];
		}
		TestTrue(FString::Printf(TEXT("point %d's weights sum to 1 (%.4f)"), Point, Sum), FMath::IsNearlyEqual(Sum, 1.0f, 1e-4f));
	}
	for (int32 Point = 0; Point < PerRing; ++Point)
	{
		TestTrue(FString::Printf(TEXT("low point %d is all pelvis"), Point), FMath::IsNearlyEqual(Weights.WeightOf(Point, Pelvis), 1.0f, 1e-4f));
		TestTrue(FString::Printf(TEXT("high point %d is all spine"), Point), FMath::IsNearlyEqual(Weights.WeightOf(PerRing + Point, Spine), 1.0f, 1e-4f));
		const float Split = Weights.WeightOf(2 * PerRing + Point, Pelvis);
		TestTrue(FString::Printf(TEXT("split point %d is half pelvis (%.3f)"), Point, Split), FMath::IsNearlyEqual(Split, 0.5f, 1e-3f));
		TestEqual(FString::Printf(TEXT("inside point %d follows the pelvis"), Point), Weights.Dominant(3 * PerRing + Point), Pelvis);
		TestTrue(FString::Printf(TEXT("head band point %d is all head"), Point), FMath::IsNearlyEqual(Weights.WeightOf(4 * PerRing + Point, Head), 1.0f, 1e-6f));
		TestEqual(FString::Printf(TEXT("head band point %d takes nothing from the spine"), Point), Weights.WeightOf(4 * PerRing + Point, Spine), 0.0f);
	}
	TestTrue(TEXT("the top of the head is all head"), FMath::IsNearlyEqual(Weights.WeightOf(5 * PerRing, Head), 1.0f, 1e-6f));
	TestEqual(TEXT("the cape's edge follows its nearest bone, the spine"), Weights.Dominant(5 * PerRing + 1), Spine);

	// No head bone: nothing is cut, the head band takes the skin's top (the spine).
	FMantleAutoRigWeights NoCut;
	TestTrue(TEXT("the weights transfer without a head bone"), FMantleUnriggedAsIs::TransferWeights(Points, Skin, INDEX_NONE, HeadCutZ, NoCut, Error));
	TestEqual(TEXT("without a head bone nothing goes to the head"), NoCut.HeadPoints, 0);
	TestEqual(TEXT("without a head bone the head band follows the spine"), NoCut.Dominant(4 * PerRing), Spine);

	// No skin: refused with why.
	FMantleAutoRigSkin Empty;
	Empty.NumInfluences = 4;
	FMantleAutoRigWeights None;
	FString EmptyError;
	TestFalse(TEXT("no skin is refused"), FMantleUnriggedAsIs::TransferWeights(Points, Empty, Head, HeadCutZ, None, EmptyError));
	TestFalse(TEXT("and says why"), EmptyError.IsEmpty());

	// AddVertex keeps the heaviest slots.
	FMantleAutoRigSkin Two;
	Two.NumInfluences = 2;
	Two.AddVertex(FVector3f::ZeroVector, { TPair<int32, float>(5, 0.1f), TPair<int32, float>(6, 0.6f), TPair<int32, float>(7, 0.3f) });
	TestEqual(TEXT("the heaviest slot first"), Two.Bones[0], 6);
	TestEqual(TEXT("then the next"), Two.Bones[1], 7);
	return true;
}

#endif
