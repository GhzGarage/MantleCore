// Copyright Joshua Eger. All Rights Reserved.
#include "MantleAvatarContract.h"
#include "MantleCosmeticExport.h"
#include "MantleCosmeticPackage.h"
#include "MantleMeshReduce.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// A rigid accessory over the class's triangle limit uploads a reduced copy of itself instead of being refused, and
// one under the limit uploads as it is. Small generated meshes: a wavy disc of quads, 20 cm across, pivot at its centre.

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleAccessoryExportTestPrivate
{
	/** A transient one-material static mesh of Rings x Segments quads over a 10 cm radius disc, gently waved. */
	UStaticMesh* MakeDisc(const int32 Rings, const int32 Segments, UMaterialInterface* Material)
	{
		FMeshDescription Description;
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
		UVs.SetNumChannels(1);
		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = FName(TEXT("Surface"));

		TArray<FVertexID> Vertices;
		for (int32 Ring = 0; Ring <= Rings; ++Ring)
		{
			for (int32 Segment = 0; Segment <= Segments; ++Segment)
			{
				const float Radius = 10.0f * Ring / Rings;
				const float Angle = 2.0f * PI * Segment / Segments;
				const FVertexID Vertex = Description.CreateVertex();
				Positions[Vertex] = FVector3f(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.5f * FMath::Sin(Radius) * FMath::Cos(3.0f * Angle));
				Vertices.Add(Vertex);
			}
		}
		auto AddTriangle = [&](int32 A, int32 B, int32 C)
		{
			TArray<FVertexInstanceID> Corners;
			for (const int32 Index : { A, B, C })
			{
				const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertices[Index]);
				const FVector3f Position = Positions[Vertices[Index]];
				Normals[Instance] = FVector3f(0.0f, 0.0f, 1.0f);
				Tangents[Instance] = FVector3f(1.0f, 0.0f, 0.0f);
				BinormalSigns[Instance] = 1.0f;
				Colors[Instance] = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
				UVs.Set(Instance, 0, FVector2f(0.5f + Position.X / 20.0f, 0.5f + Position.Y / 20.0f));
				Corners.Add(Instance);
			}
			Description.CreateTriangle(Group, Corners);
		};
		for (int32 Ring = 0; Ring < Rings; ++Ring)
		{
			for (int32 Segment = 0; Segment < Segments; ++Segment)
			{
				const int32 V00 = Ring * (Segments + 1) + Segment;
				const int32 V01 = V00 + 1;
				const int32 V10 = V00 + Segments + 1;
				const int32 V11 = V10 + 1;
				AddTriangle(V00, V10, V11);
				AddTriangle(V00, V11, V01);
			}
		}

		UObject* Outer = GetTransientPackage();
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), FName(TEXT("MantleAccessoryTestMesh"))), RF_Transient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, FName(TEXT("Surface")), FName(TEXT("Surface"))));
		Mesh->bAllowCPUAccess = true;
		Mesh->SetNumSourceModels(1);
		FMeshBuildSettings& Settings = Mesh->GetSourceModel(0).BuildSettings;
		Settings.bRecomputeNormals = false;
		Settings.bRecomputeTangents = false;
		Settings.bGenerateLightmapUVs = false;
		Settings.bBuildReversedIndexBuffer = false;
		Settings.DistanceFieldResolutionScale = 0.0f;
		Settings.MaxLumenMeshCards = 0;
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bAllowCpuAccess = true;
		Mesh->BuildFromMeshDescriptions({ &Description }, Params);
		return Mesh;
	}

	int32 Triangles(const UStaticMesh& Mesh)
	{
		const FStaticMeshRenderData* Render = Mesh.GetRenderData();
		return Render && Render->LODResources.Num() > 0 ? Render->LODResources[0].GetNumTriangles() : 0;
	}

	bool HasNote(const FMantleCosmeticPackage& Package, const TCHAR* Text)
	{
		return Package.Notes.ContainsByPredicate([Text](const FString& Note) { return Note.Contains(Text); });
	}

	FMantleCosmeticDraft NeckDraft(UStaticMesh* Mesh)
	{
		FMantleCosmeticDraft Draft;
		Draft.Class = EMantleCosmeticClass::Accessory;
		Draft.Slot = TEXT("acc_neck");
		Draft.Name = TEXT("Test chain");
		Draft.bPublic = false;
		Draft.bRenderThumbnail = false;
		Draft.Source = Mesh;
		return Draft;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMantleAccessoryReduceTest, "Mantle.Cosmetics.Accessory.ReduceOverLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMantleAccessoryReduceTest::RunTest(const FString& Parameters)
{
	using namespace MantleAccessoryExportTestPrivate;
	FString ContractError;
	const FMantleAvatarContract* Contract = FMantleAvatarContract::Get(&ContractError);
	if (!TestNotNull(TEXT("the avatar contract loads"), Contract))
	{
		AddError(ContractError);
		return false;
	}
	const int32 Limit = Contract->RigidAccessory.MaxTriangles;
	if (!TestTrue(TEXT("rigid accessories have a triangle limit"), Limit > 0))
	{
		return false;
	}
	FMantleMeshReduce::ClearCache();
	ON_SCOPE_EXIT { FMantleMeshReduce::ClearCache(); };
	TStrongObjectPtr<UMaterialInterface> Material(LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));

	// Over the limit: 60 x 50 quads, 6,000 triangles against 4,000.
	TStrongObjectPtr<UStaticMesh> Dense(MakeDisc(60, 50, Material.Get()));
	const int32 DenseBefore = Triangles(*Dense);
	TestTrue(TEXT("the dense disc is over the limit"), DenseBefore > Limit);
	const FMantleCosmeticPackage Reduced = FMantleCosmeticExport::BuildCore(NeckDraft(Dense.Get()), *Contract);
	for (const FMantleCosmeticIssue& Issue : Reduced.Issues)
	{
		if (Issue.Severity == EMantleCosmeticSeverity::Error)
		{
			AddError(FString::Printf(TEXT("over-limit accessory: %s"), *Issue.ToString()));
		}
	}
	TestFalse(TEXT("an over-limit accessory exports without errors"), Reduced.HasErrors());
	TestTrue(TEXT("its notes say it was reduced"), HasNote(Reduced, TEXT("reduced from")));
	const FMantleCosmeticUploadAsset* ReducedMesh = Reduced.FindAsset(TEXT("mesh"));
	TestTrue(TEXT("it uploads a mesh"), ReducedMesh && ReducedMesh->Bytes.Num() > 0);
	TestEqual(TEXT("the creator's mesh is untouched"), Triangles(*Dense), DenseBefore);

	// Under the limit: 30 x 30 quads, 1,800 triangles, exported as it is.
	TStrongObjectPtr<UStaticMesh> Light(MakeDisc(30, 30, Material.Get()));
	const FMantleCosmeticPackage Kept = FMantleCosmeticExport::BuildCore(NeckDraft(Light.Get()), *Contract);
	TestFalse(TEXT("an under-limit accessory exports without errors"), Kept.HasErrors());
	TestFalse(TEXT("an under-limit accessory is not reduced"), HasNote(Kept, TEXT("reduced from")));
	return true;
}

#endif
