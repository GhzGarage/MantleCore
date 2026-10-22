// Copyright Joshua Eger. All Rights Reserved.
#include "MantleHairFitPictures.h"

#include "MantleCosmeticThumbnail.h"

#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshDescription.h"
#include "UObject/StrongObjectPtr.h"

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleHairFitPicturesPrivate
{
	/** One static mesh of the head (grey) and the hair (brown): vertex colours on the engine's vertex colour material. */
	UStaticMesh* BuildHeadAndHair(const FMantleHairFitMeshes& Meshes, const TArray<FVector3f>& HairPositions)
	{
		UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
		UStaticMeshDescription* Description = UStaticMesh::CreateStaticMeshDescription(Mesh);
		FMeshDescription& MD = Description->GetMeshDescription();
		FStaticMeshAttributes Attributes(MD);
		Attributes.Register();
		TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		const FPolygonGroupID Group = MD.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("HairFit");

		const auto AddPart = [&](const TArray<FVector3f>& Positions, const TArray<uint32>& Triangles, const FLinearColor& Colour)
		{
			TArray<FVertexID> Vertices;
			for (const FVector3f& P : Positions) Vertices.Add(MD.CreateVertex()), Attributes.GetVertexPositions()[Vertices.Last()] = P;
			for (int32 T = 0; T + 2 < Triangles.Num(); T += 3)
			{
				TArray<FVertexInstanceID> Corners;
				for (int32 C = 0; C < 3; ++C)
				{
					const FVertexInstanceID Corner = MD.CreateVertexInstance(Vertices[Triangles[T + C]]);
					Colors[Corner] = FVector4f(Colour);
					Corners.Add(Corner);
				}
				MD.CreateTriangle(Group, Corners);
			}
		};
		AddPart(Meshes.HeadPositions, Meshes.HeadTriangles, FLinearColor(0.45f, 0.40f, 0.36f));
		AddPart(HairPositions, Meshes.HairTriangles, FLinearColor(0.20f, 0.09f, 0.03f));

		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("HairFit")));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bBuildSimpleCollision = false;
		Mesh->BuildFromMeshDescriptions({&MD}, Params);
		return Mesh;
	}
}

FBox FMantleHairFitPictures::FrameOf(const FMantleHairFitMeshes& Meshes)
{
	FBox Frame(ForceInit);
	for (const FVector3f& P : Meshes.HeadPositions) Frame += FVector(P);
	for (const FVector3f& P : Meshes.AfterPositions) Frame += FVector(P);
	return Frame;
}

bool FMantleHairFitPictures::Render(const FMantleHairFitMeshes& Meshes, bool bAfter, float Yaw, int32 Size, TArray<FColor>& OutPixels, FString& OutError)
{
	if (!Meshes.IsValid()) { OutError = TEXT("there is no fit to draw"); return false; }
	if (!FMantleCosmeticThumbnail::CanCapture(&OutError)) return false;
	TStrongObjectPtr<UStaticMesh> Shown(MantleHairFitPicturesPrivate::BuildHeadAndHair(Meshes, bAfter ? Meshes.AfterPositions : Meshes.BeforePositions));
	FMantleThumbnailSubject Shot = FMantleCosmeticThumbnail::AccessorySubject(Shown.Get(), FQuat::Identity, true);
	Shot.Frame = FrameOf(Meshes);
	Shot.View = FRotator(0.0f, Yaw, 0.0f);
	return FMantleCosmeticThumbnail::Capture(Shot, Size, OutPixels, OutError);
}
