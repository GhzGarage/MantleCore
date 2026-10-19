// Copyright Joshua Eger. All Rights Reserved.
#include "MantleGarmentSkinTransfer.h"

#include "MantleGarmentClearance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The skin transfer's collar weights: an unrigged garment (one slot, all root) over a one-triangle body that is all
// spine_04, with a one-triangle face whose skin is neck_01 and a face-only neck joint under it. The clearances are
// written by hand, so the face's share of each vertex is known: dB / (dB + dF), faded over the face's last FaceFadeCm.

// Named, not anonymous: MantleRuntime builds as a unity build.
namespace MantleGarmentSkinTransferTestPrivate
{
	enum EBone : uint16 { Root = 0, Spine04 = 1, Neck01 = 2 };

	/** One triangle of three vertices at Z, NumInfluences slots, every vertex weighted Bones / Weights. */
	FMantleSkinnedMeshLOD Triangle(const float Z, const int32 NumInfluences, const TArray<uint16>& Bones, const TArray<uint8>& Weights)
	{
		FMantleSkinnedMeshLOD LOD;
		LOD.Positions = { FVector3f(0.0f, 0.0f, Z), FVector3f(10.0f, 0.0f, Z), FVector3f(0.0f, 10.0f, Z) };
		LOD.Normals.Init(FVector3f(0.0f, 0.0f, 1.0f), 3);
		LOD.UVs.Init(FVector2f::ZeroVector, 3);
		LOD.Indices = { 0, 1, 2 };
		LOD.NumInfluences = NumInfluences;
		LOD.InfluenceBones.SetNumZeroed(3 * NumInfluences);
		LOD.InfluenceWeights.SetNumZeroed(3 * NumInfluences);
		for (int32 Vertex = 0; Vertex < 3; ++Vertex)
		{
			for (int32 Slot = 0; Slot < Bones.Num(); ++Slot)
			{
				LOD.InfluenceBones[Vertex * NumInfluences + Slot] = Bones[Slot];
				LOD.InfluenceWeights[Vertex * NumInfluences + Slot] = Weights[Slot];
			}
		}
		return LOD;
	}

	/** A clearance over a three-vertex triangle: per garment vertex tracked at its own corner (or not, ClearanceCm < 0 here meaning untracked). */
	FMantleGarmentClearance Clearance(const TArray<float>& ClearanceCm)
	{
		FMantleGarmentClearance Out;
		Out.SurfaceIndices = { 0, 1, 2 };
		Out.TriangleSign = { 1 };
		Out.NumSurfaceVertices = 3;
		for (int32 Vertex = 0; Vertex < ClearanceCm.Num(); ++Vertex)
		{
			const bool bTracked = ClearanceCm[Vertex] >= 0.0f;
			Out.Triangle.Add(bTracked ? 0 : INDEX_NONE);
			FVector3f Bary = FVector3f::ZeroVector;
			Bary[Vertex % 3] = 1.0f;
			Out.Barycentrics.Add(Bary);
			Out.ClearanceCm.Add(bTracked ? ClearanceCm[Vertex] : 0.0f);
			Out.NumTracked += bTracked ? 1 : 0;
		}
		return Out;
	}

	/** Bone and weight of each used slot of Vertex. */
	TMap<uint16, int32> WeightsOf(const FMantleSkinnedMeshLOD& LOD, const int32 Vertex)
	{
		TMap<uint16, int32> Out;
		for (int32 Slot = 0; Slot < LOD.NumInfluences; ++Slot)
		{
			const uint8 Weight = LOD.InfluenceWeights[Vertex * LOD.NumInfluences + Slot];
			if (Weight > 0)
			{
				Out.FindOrAdd(LOD.InfluenceBones[Vertex * LOD.NumInfluences + Slot]) += Weight;
			}
		}
		return Out;
	}

	int32 Sum(const TMap<uint16, int32>& Weights)
	{
		int32 Total = 0;
		for (const TPair<uint16, int32>& Pair : Weights)
		{
			Total += Pair.Value;
		}
		return Total;
	}

	TArray<FMantleSkinnedMeshBone> BodyBones()
	{
		return { { TEXT("root"), INDEX_NONE }, { TEXT("spine_04"), 0 }, { TEXT("neck_01"), 1 } };
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleGarmentSkinTransferCollarTest,
	"Mantle.Runtime.GarmentSkinTransfer.CollarTakesFaceWeights",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleGarmentSkinTransferCollarTest::RunTest(const FString& Parameters)
{
	using namespace MantleGarmentSkinTransferTestPrivate;

	FMantleSkinnedMesh Wearer;
	Wearer.Bones = BodyBones();
	Wearer.LODs.Add(Triangle(0.0f, 4, { Spine04 }, { 255 }));

	// The face's table differs from the body's, and its neck joint (face bone 3) is not on the garment: it rides on neck_01.
	const TArray<FMantleSkinnedMeshBone> FaceBones = { { TEXT("root"), INDEX_NONE }, { TEXT("spine_04"), 0 }, { TEXT("neck_01"), 1 }, { TEXT("FACIAL_C_NeckBackB"), 2 } };
	const FMantleSkinnedMeshLOD FaceLOD = Triangle(0.0f, 8, { 2, 3 }, { 128, 127 });

	FMantleSkinnedMeshLOD Garment = Triangle(1.0f, 1, { Root }, { 255 });
	const FMantleGarmentClearance Authored = Clearance({ 2.0f, 2.0f, 2.0f });
	// Vertex 0 lies 0.5 cm off the face (share 2 / 2.5 = 0.8), vertex 1 is past its reach, vertex 2 at 5.5 cm (faded to 1/4 of 2 / 7.5).
	const FMantleGarmentClearance FaceClearance = Clearance({ 0.5f, -1.0f, 5.5f });

	FMantleSkinTransferFace Face;
	Face.Surface = &FaceLOD;
	Face.Bones = FaceBones;
	Face.Clearance = &FaceClearance;
	FMantleSkinTransferStats Stats;
	FString Error;
	const TArray<FMantleSkinnedMeshBone> GarmentBones = BodyBones();
	if (!TestTrue(TEXT("the transfer runs"), FMantleGarmentSkinTransfer::Transfer(Garment, GarmentBones, Authored, Wearer, Face, &Stats, &Error)))
	{
		AddError(Error);
		return false;
	}

	TestEqual(TEXT("the one-slot garment grows to the two bones its blends use"), Garment.NumInfluences, 2);
	TestEqual(TEXT("the stats say so"), Stats.Influences, 2);
	TestEqual(TEXT("slots before"), Stats.SlotsBefore, 1);
	TestEqual(TEXT("every vertex from the skin"), Stats.FromBody, 3);
	TestEqual(TEXT("two took the face's weights"), Stats.FromFace, 2);
	TestEqual(TEXT("the face's neck joint mapped to neck_01, nothing dropped"), Stats.DroppedInfluences, 0);

	const TMap<uint16, int32> Collar = WeightsOf(Garment, 0);
	TestEqual(TEXT("the collar vertex sums to 255"), Sum(Collar), 255);
	TestEqual(TEXT("the collar vertex is 80% neck_01"), Collar.FindRef(Neck01), 204);
	TestEqual(TEXT("and 20% spine_04"), Collar.FindRef(Spine04), 51);
	TestFalse(TEXT("and no root left"), Collar.Contains(Root));

	const TMap<uint16, int32> Shoulder = WeightsOf(Garment, 1);
	TestEqual(TEXT("past the face: the body's weights only"), Shoulder.FindRef(Spine04), 255);
	TestEqual(TEXT("and only one bone"), Shoulder.Num(), 1);

	const TMap<uint16, int32> Edge = WeightsOf(Garment, 2);
	TestEqual(TEXT("at the face's edge the vertex sums to 255"), Sum(Edge), 255);
	TestTrue(TEXT("and mostly the body's"), Edge.FindRef(Spine04) > 230 && Edge.FindRef(Neck01) > 0);

	// Without the face the same garment stays on the chest, the old behaviour.
	FMantleSkinnedMeshLOD BodyOnly = Triangle(1.0f, 1, { Root }, { 255 });
	TestTrue(TEXT("the body-only transfer runs"), FMantleGarmentSkinTransfer::Transfer(BodyOnly, GarmentBones, Authored, Wearer));
	TestEqual(TEXT("body only: the collar vertex is all spine_04"), WeightsOf(BodyOnly, 0).FindRef(Spine04), 255);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleGarmentSkinTransferSlotsTest,
	"Mantle.Runtime.GarmentSkinTransfer.UnriggedGarmentKeepsTheBlend",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleGarmentSkinTransferSlotsTest::RunTest(const FString& Parameters)
{
	using namespace MantleGarmentSkinTransferTestPrivate;

	// A body blended over three bones; the unrigged garment (one slot) used to keep only the heaviest.
	FMantleSkinnedMesh Wearer;
	Wearer.Bones = BodyBones();
	Wearer.LODs.Add(Triangle(0.0f, 4, { Spine04, Neck01, Root }, { 153, 77, 25 }));
	FMantleSkinnedMeshLOD Garment = Triangle(1.0f, 1, { Root }, { 255 });
	const FMantleGarmentClearance Authored = Clearance({ 1.0f, 1.0f, 1.0f });
	FMantleSkinTransferStats Stats;
	if (!TestTrue(TEXT("the transfer runs"), FMantleGarmentSkinTransfer::Transfer(Garment, BodyBones(), Authored, Wearer, FMantleSkinTransferFace(), &Stats)))
	{
		return false;
	}
	TestTrue(TEXT("more than one influence"), Garment.NumInfluences > 1);
	TestEqual(TEXT("as many slots as the blend has bones"), Garment.NumInfluences, 3);
	const TMap<uint16, int32> Weights = WeightsOf(Garment, 0);
	TestEqual(TEXT("spine_04 kept"), Weights.FindRef(Spine04), 153);
	TestEqual(TEXT("neck_01 kept"), Weights.FindRef(Neck01), 77);
	TestEqual(TEXT("root kept"), Weights.FindRef(Root), 25);

	// Smoothing works on the wider slots and keeps every vertex at 255.
	TestTrue(TEXT("the smoothing runs on the grown slots"), FMantleGarmentSkinTransfer::Smooth(Garment, 3));
	for (int32 Vertex = 0; Vertex < 3; ++Vertex)
	{
		TestEqual(FString::Printf(TEXT("vertex %d sums to 255 after the smoothing"), Vertex), Sum(WeightsOf(Garment, Vertex)), 255);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
