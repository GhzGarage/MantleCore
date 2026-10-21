// Copyright Joshua Eger. All Rights Reserved.
//
// Maintainer command: Mantle.Dev.RenderHairFit <HairCardsMesh> <Body> <Face> <Directory> <Label> [slot] [size]
// Checks a bought hair cards mesh as the plugin does (FMantleEditorActions::MakeCosmeticDraft, then FMantleCosmeticExport::Build,
// which runs the automatic head fit) and renders the result as the catalog thumbnail does (FMantleCosmeticThumbnail, the engine's
// thumbnail scene): the head it was fitted to with the hair as authored ("before") and as fitted ("after"), from the front, a
// three-quarter view, the side and the back, into <Directory>/<Label>_{before,after}_{front,angled,side,back}.png. The head and the
// hair are the geometry the export used (Package.HairHeadFitMeshes), drawn in one mesh with vertex colours (head grey, hair brown).
// Nothing is saved or uploaded. Logs MANTLE_RENDER_HAIRFIT lines: the notes, the fit's words and each picture.
#include "MantleAvatarContract.h"
#include "MantleCosmeticExport.h"
#include "MantleCosmeticThumbnail.h"
#include "MantleEditorActions.h"
#include "MantlePng.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshDescription.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogMantleRenderHairFit, Log, All);

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleDevRenderHairFitPrivate
{
	template <typename T>
	T* LoadByPath(FString Path)
	{
		if (!FPackageName::GetShortName(Path).Contains(TEXT("."))) Path += TEXT(".") + FPackageName::GetShortName(Path);
		return LoadObject<T>(nullptr, *Path);
	}

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

	void Run(const TArray<FString>& Args)
	{
		if (Args.Num() < 5)
		{
			UE_LOG(LogMantleRenderHairFit, Error, TEXT("MANTLE_RENDER_HAIRFIT usage: Mantle.Dev.RenderHairFit <HairCardsMesh> <Body> <Face> <Directory> <Label> [slot] [size]"));
			return;
		}
		const FMantleAvatarContract* Contract = FMantleAvatarContract::Get();
		UStaticMesh* Hair = LoadByPath<UStaticMesh>(Args[0]);
		USkeletalMesh* Body = LoadByPath<USkeletalMesh>(Args[1]);
		USkeletalMesh* Face = LoadByPath<USkeletalMesh>(Args[2]);
		FString WhyNot;
		if (!Contract || !Hair || !Body || !Face || !FMantleCosmeticThumbnail::CanCapture(&WhyNot))
		{
			UE_LOG(LogMantleRenderHairFit, Error, TEXT("MANTLE_RENDER_HAIRFIT failed: need the contract, a cards static mesh, a body, a face and a renderer (%s)"), *WhyNot);
			return;
		}
		const FString Slot = Args.Num() > 5 ? Args[5] : (Contract->HairSlots.Num() > 0 ? Contract->HairSlots[0] : FString(TEXT("hair")));
		const int32 Size = Args.Num() > 6 ? FMath::Clamp(FCString::Atoi(*Args[6]), 128, 2048) : 768;

		FMantleCosmeticDraft Draft = FMantleEditorActions::MakeCosmeticDraft(EMantleCosmeticClass::Hair, Hair, Slot, Args[4], *Contract, EMantleFitSex::Auto);
		Draft.AuthoringBody = Body;
		Draft.AuthoringFace = Face;
		Draft.bRenderThumbnail = false;
		const FMantleCosmeticPackage Package = FMantleCosmeticExport::Build(Draft, *Contract);
		for (const FString& Note : Package.Notes) UE_LOG(LogMantleRenderHairFit, Display, TEXT("MANTLE_RENDER_HAIRFIT note: %s"), *Note);
		for (const FMantleCosmeticIssue& Issue : Package.Issues) UE_LOG(LogMantleRenderHairFit, Display, TEXT("MANTLE_RENDER_HAIRFIT issue: %s"), *Issue.ToString());
		UE_LOG(LogMantleRenderHairFit, Display, TEXT("MANTLE_RENDER_HAIRFIT fitted=%d lowConfidence=%d bakedHead=%d"),
			Package.bHairHeadFitted, Package.HairHeadFit.bLowConfidence, Package.HairHeadFit.bBakedHead);
		for (const FString& Message : Package.HairHeadFit.Messages) UE_LOG(LogMantleRenderHairFit, Display, TEXT("MANTLE_RENDER_HAIRFIT message: %s"), *Message);
		const FMantleHairFitMeshes& Meshes = Package.HairHeadFitMeshes;
		if (!Package.bHairHeadFitted || !Meshes.IsValid())
		{
			UE_LOG(LogMantleRenderHairFit, Error, TEXT("MANTLE_RENDER_HAIRFIT no fit to draw (errors=%d)"), Package.HasErrors());
			return;
		}

		// One frame for every picture: the head and the fitted hair.
		FBox Frame(ForceInit);
		for (const FVector3f& P : Meshes.HeadPositions) Frame += FVector(P);
		for (const FVector3f& P : Meshes.AfterPositions) Frame += FVector(P);
		UE_LOG(LogMantleRenderHairFit, Display, TEXT("MANTLE_RENDER_HAIRFIT frame=%s"), *Frame.ToString());

		struct FView { const TCHAR* Name; float Yaw; };
		const FView Views[] = { {TEXT("front"), 180.f}, {TEXT("angled"), 135.f}, {TEXT("side"), 90.f}, {TEXT("back"), 0.f} };
		for (const bool bAfter : { false, true })
		{
			TStrongObjectPtr<UStaticMesh> Shown(BuildHeadAndHair(Meshes, bAfter ? Meshes.AfterPositions : Meshes.BeforePositions));
			for (const FView& View : Views)
			{
				FMantleThumbnailSubject Shot = FMantleCosmeticThumbnail::AccessorySubject(Shown.Get(), FQuat::Identity, true);
				Shot.Frame = Frame;
				Shot.View = FRotator(0.0f, View.Yaw, 0.0f);
				TArray<FColor> Pixels;
				FString Error;
				const FString File = FPaths::Combine(Args[3], FString::Printf(TEXT("%s_%s_%s.png"), *Args[4], bAfter ? TEXT("after") : TEXT("before"), View.Name));
				const bool bWritten = FMantleCosmeticThumbnail::Capture(Shot, Size, Pixels, Error)
					&& FFileHelper::SaveArrayToFile(FMantlePng::EncodeRGBA8(Size, Size, Pixels), *File);
				UE_LOG(LogMantleRenderHairFit, Display, TEXT("MANTLE_RENDER_HAIRFIT %s %s %s"), View.Name, bWritten ? *File : TEXT("FAILED"), bWritten ? TEXT("") : *Error);
			}
		}
		UE_LOG(LogMantleRenderHairFit, Display, TEXT("MANTLE_RENDER_HAIRFIT done"));
	}

	FAutoConsoleCommand GMantleDevRenderHairFitCommand(
		TEXT("Mantle.Dev.RenderHairFit"),
		TEXT("Mantle.Dev.RenderHairFit <HairCardsMesh> <Body> <Face> <Directory> <Label> [slot] [size] -- checks bought hair cards as the plugin does (automatic head fit) and ")
		TEXT("renders the head with the hair as authored and as fitted, as the catalog thumbnail does. Saves and uploads nothing."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
