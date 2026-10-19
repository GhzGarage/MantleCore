// Copyright Joshua Eger. All Rights Reserved.
//
// Maintainer command: Mantle.Dev.NeckCheck
// Read-only diagnosis of the collar of clothing fitted on the standard body types made in memory. The client fits a collar from the face a
// garment was authored with (FMantleNeckSamples: the neck base is the highest body vertex within 12 cm of the z axis; the samples are the centroids
// of the face's triangles in a region around that base, paired with the wearer face's SAME-INDEX triangles). So this logs, per case:
//  - the neck seam: body and face in the same space (neck base, face column, distance from the body's neck ring to the nearest face vertex);
//  - whether a face's triangle T is the same place as triangle T of a built face (same-index vs nearest-centroid distance over the region).
// Cases: the standard bodies f_med_nrw and m_med_nrw made in memory, and the converted Aera (preview and built), which fitted correctly before.
// Nothing is changed, saved or uploaded. Logs MANTLE_NECK_CHECK lines.
#include "MantleAvatarContract.h"
#include "MantleBodyTopologyData.h"
#include "MantleFitBodies.h"

#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogMantleNeckCheck, Log, All);

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleDevNeckCheckPrivate
{
	// The client's numbers (MantleNeckSamples.h).
	constexpr float ColumnRadiusCm = 12.0f;
	constexpr float ColumnHeightCm = 6.0f;
	constexpr float CorrespondBelowBaseCm = 6.0f;
	constexpr float CorrespondAboveBaseCm = 12.0f;
	constexpr float CorrespondRadiusCm = 16.0f;
	constexpr int32 ReferenceFaceTriangles = 6872;

	struct FNeckCase
	{
		FString Name;
		bool bOk = false;
		FString Error;
		TArray<FVector3f> Body;
		int32 BodyLOD = INDEX_NONE;
		TArray<FVector3f> FacePositions;
		TArray<uint32> FaceIndices;
		int32 FaceLOD = INDEX_NONE;
		float BaseZ = -MAX_flt;
		/** The face's triangle centroids, by triangle index. */
		TArray<FVector3f> Centroids;
	};

	/** The first LOD of a body with the reference vertex count, its vertices; false when none. */
	bool ReadBody(const USkeletalMesh* Mesh, const int32 ReferenceVertices, FNeckCase& Out)
	{
		const int32 NumLODs = UMantleBodyTopologyData::GetNumLODs(Mesh);
		for (int32 LOD = 0; LOD < NumLODs; ++LOD)
		{
			FMantleBodyLODTopology Topology;
			TArray<FVector3f> Positions;
			if (UMantleBodyTopologyData::ReadLOD(Mesh, LOD, Topology, &Positions) && Topology.VertexCount == ReferenceVertices)
			{
				Out.Body = MoveTemp(Positions);
				Out.BodyLOD = LOD;
				return true;
			}
		}
		Out.Error = TEXT("the body has no LOD with the reference vertex count");
		return false;
	}

	/** The face LOD the fit measures against: the first with the reference face topology, else LOD0 (the client's FitLOD). */
	bool ReadFace(const USkeletalMesh* Mesh, FNeckCase& Out)
	{
		const FSkeletalMeshRenderData* RenderData = Mesh ? Mesh->GetResourceForRendering() : nullptr;
		if (!RenderData || RenderData->LODRenderData.IsEmpty())
		{
			Out.Error = TEXT("the face has no render data");
			return false;
		}
		int32 Chosen = 0;
		for (int32 LOD = 0; LOD < RenderData->LODRenderData.Num(); ++LOD)
		{
			if (RenderData->LODRenderData[LOD].GetTotalFaces() == ReferenceFaceTriangles) { Chosen = LOD; break; }
		}
		const FSkeletalMeshLODRenderData& LOD = RenderData->LODRenderData[Chosen];
		const FPositionVertexBuffer& Positions = LOD.StaticVertexBuffers.PositionVertexBuffer;
		if (Positions.GetNumVertices() == 0 || !Positions.GetVertexData() || !LOD.MultiSizeIndexContainer.IsIndexBufferValid())
		{
			Out.Error = TEXT("the face has no CPU positions or indices");
			return false;
		}
		for (uint32 Vertex = 0; Vertex < Positions.GetNumVertices(); ++Vertex) { Out.FacePositions.Add(Positions.VertexPosition(Vertex)); }
		LOD.MultiSizeIndexContainer.GetIndexBuffer(Out.FaceIndices);
		Out.FaceLOD = Chosen;
		for (int32 First = 0; First + 2 < Out.FaceIndices.Num(); First += 3)
		{
			Out.Centroids.Add((Out.FacePositions[Out.FaceIndices[First]] + Out.FacePositions[Out.FaceIndices[First + 1]] + Out.FacePositions[Out.FaceIndices[First + 2]]) / 3.0f);
		}
		return true;
	}

	bool InRegion(const FVector3f& Centroid, const float BaseZ)
	{
		return Centroid.Z >= BaseZ - CorrespondBelowBaseCm && Centroid.Z < BaseZ + CorrespondAboveBaseCm
			&& FMath::Sqrt(Centroid.X * Centroid.X + Centroid.Y * Centroid.Y) <= CorrespondRadiusCm;
	}

	void Describe(FNeckCase& Case)
	{
		float BaseZ = -MAX_flt;
		FBox BodyBox(ForceInit), FaceBox(ForceInit);
		for (const FVector3f& P : Case.Body)
		{
			BodyBox += FVector(P);
			if (P.X * P.X + P.Y * P.Y <= ColumnRadiusCm * ColumnRadiusCm) { BaseZ = FMath::Max(BaseZ, P.Z); }
		}
		for (const FVector3f& P : Case.FacePositions) { FaceBox += FVector(P); }
		Case.BaseZ = BaseZ;
		// The face's neck column just over the base: the axis the client casts or centres on.
		FVector2f Sum = FVector2f::ZeroVector;
		int32 Column = 0;
		for (const FVector3f& P : Case.FacePositions)
		{
			if (P.Z >= BaseZ && P.Z < BaseZ + ColumnHeightCm && P.X * P.X + P.Y * P.Y <= ColumnRadiusCm * ColumnRadiusCm) { Sum += FVector2f(P.X, P.Y); ++Column; }
		}
		// The body's neck ring (its top, within 1 cm of the base) against the nearest face vertex.
		TArray<FVector3f> Ring;
		for (const FVector3f& P : Case.Body)
		{
			if (P.X * P.X + P.Y * P.Y <= ColumnRadiusCm * ColumnRadiusCm && P.Z >= BaseZ - 1.0f) { Ring.Add(P); }
		}
		float RingAverage = -1.0f, RingMax = -1.0f;
		FMantleFitBodies::CompareNearest(Ring, Case.FacePositions, RingAverage, RingMax);
		int32 Region = 0;
		for (const FVector3f& Centroid : Case.Centroids) { Region += InRegion(Centroid, BaseZ) ? 1 : 0; }
		UE_LOG(LogMantleNeckCheck, Display,
			TEXT("MANTLE_NECK_CHECK case=%s bodyLOD=%d bodyVerts=%d faceLOD=%d faceTris=%d baseZ=%.2f bodyZ=[%.2f, %.2f] faceZ=[%.2f, %.2f] faceColumnVerts=%d faceColumnCentre=(%.2f, %.2f) ringVerts=%d ringToFaceAvgCm=%.3f ringToFaceMaxCm=%.3f regionTris=%d"),
			*Case.Name, Case.BodyLOD, Case.Body.Num(), Case.FaceLOD, Case.Centroids.Num(), BaseZ, BodyBox.Min.Z, BodyBox.Max.Z, FaceBox.Min.Z, FaceBox.Max.Z, Column,
			Column ? Sum.X / Column : 0.0f, Column ? Sum.Y / Column : 0.0f, Ring.Num(), RingAverage, RingMax, Region);
	}

	/** Triangle T of A against triangle T of B, and against B's nearest centroid, over A's collar region. */
	void ComparePair(const FNeckCase& A, const FNeckCase& B)
	{
		if (A.Centroids.Num() != B.Centroids.Num() || A.Centroids.IsEmpty())
		{
			UE_LOG(LogMantleNeckCheck, Display, TEXT("MANTLE_NECK_CHECK pair a=%s b=%s not compared: %d vs %d face triangles"), *A.Name, *B.Name, A.Centroids.Num(), B.Centroids.Num());
			return;
		}
		TArray<FVector3f> Probe;
		double SumSame = 0.0;
		float MaxSame = 0.0f;
		for (int32 Triangle = 0; Triangle < A.Centroids.Num(); ++Triangle)
		{
			if (!InRegion(A.Centroids[Triangle], A.BaseZ)) { continue; }
			Probe.Add(A.Centroids[Triangle]);
			const float Distance = FVector3f::Distance(A.Centroids[Triangle], B.Centroids[Triangle]);
			SumSame += Distance;
			MaxSame = FMath::Max(MaxSame, Distance);
		}
		float NearestAverage = -1.0f, NearestMax = -1.0f;
		FMantleFitBodies::CompareNearest(Probe, B.Centroids, NearestAverage, NearestMax);
		UE_LOG(LogMantleNeckCheck, Display,
			TEXT("MANTLE_NECK_CHECK pair a=%s b=%s regionTris=%d sameIndexAvgCm=%.3f sameIndexMaxCm=%.3f nearestCentroidAvgCm=%.3f nearestCentroidMaxCm=%.3f"),
			*A.Name, *B.Name, Probe.Num(), Probe.IsEmpty() ? -1.0 : SumSame / Probe.Num(), MaxSame, NearestAverage, NearestMax);
	}

	/** A standard body type made in memory (nothing saved): the body and face the upload would use. */
	FNeckCase InMemory(const TCHAR* Id, const EMetaHumanBodyType Type, const int32 ReferenceVertices)
	{
		FNeckCase Case;
		Case.Name = FString(Id) + TEXT("(in memory)");
		FMantleFitBody Body;
		if (FMantleFitBodies::Make(Id, Type, Body, Case.Error))
		{
			Case.bOk = ReadBody(Body.BodyMesh.Get(), ReferenceVertices, Case) && ReadFace(Body.FaceMesh.Get(), Case);
		}
		FMantleFitBodies::Release(Body);
		return Case;
	}

	/** The converted Aera: its built body and face (what fitted correctly), or its preview body and face from Creator. */
	FNeckCase Aera(const bool bPreview, const int32 ReferenceVertices)
	{
		FNeckCase Case;
		Case.Name = bPreview ? TEXT("Aera(preview)") : TEXT("Aera(built)");
		const FString CharacterPackage = TEXT("/Game/MetaHumans/Aera/Converted/Aera/Aera");
		const FString Unpacked = TEXT("/Game/MetaHumans/Aera/Converted/Aera/Unpacked/Aera");
		if (!bPreview)
		{
			const USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, *(Unpacked + TEXT("/Body/SKM_Aera_BodyMesh.SKM_Aera_BodyMesh")));
			const USkeletalMesh* Face = LoadObject<USkeletalMesh>(nullptr, *(Unpacked + TEXT("/Face/SKM_Aera_FaceMesh.SKM_Aera_FaceMesh")));
			if (!Body || !Face) { Case.Error = TEXT("the converted Aera body or face is not in the project"); return Case; }
			Case.bOk = ReadBody(Body, ReferenceVertices, Case) && ReadFace(Face, Case);
			return Case;
		}
		UMetaHumanCharacter* Character = LoadObject<UMetaHumanCharacter>(nullptr, *(CharacterPackage + TEXT(".Aera")));
		UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>() : nullptr;
		if (!Character || !Subsystem) { Case.Error = TEXT("the converted Aera character or MetaHuman Creator is unavailable"); return Case; }
		const bool bAdded = !Subsystem->IsObjectAddedForEditing(Character) && Subsystem->TryAddObjectToEdit(Character);
		FMetaHumanCharacterPreviewAssetOptions Options;
		FMetaHumanCharacterPreviewAssets Preview;
		if (Subsystem->IsObjectAddedForEditing(Character) && Subsystem->TryGetCharacterPreviewAssets(Character, Options, Preview) && Preview.BodyMesh && Preview.FaceMesh)
		{
			Case.bOk = ReadBody(Preview.BodyMesh.Get(), ReferenceVertices, Case) && ReadFace(Preview.FaceMesh.Get(), Case);
		}
		else { Case.Error = TEXT("Creator did not provide the Aera preview body and face"); }
		if (bAdded) { Subsystem->RemoveObjectToEdit(Character); }
		return Case;
	}

	void Run(const TArray<FString>& Args)
	{
		const FMantleAvatarContract* Contract = FMantleAvatarContract::Get();
		if (!Contract)
		{
			UE_LOG(LogMantleNeckCheck, Error, TEXT("MANTLE_NECK_CHECK error=the avatar contract is unavailable"));
			return;
		}
		TArray<FNeckCase> Cases;
		Cases.Add(InMemory(TEXT("m_med_nrw"), EMetaHumanBodyType::m_med_nrw, Contract->TopologyVertexCount));
		Cases.Add(InMemory(TEXT("f_med_nrw"), EMetaHumanBodyType::f_med_nrw, Contract->TopologyVertexCount));
		Cases.Add(Aera(false, Contract->TopologyVertexCount));
		Cases.Add(Aera(true, Contract->TopologyVertexCount));
		for (FNeckCase& Case : Cases)
		{
			if (!Case.bOk)
			{
				UE_LOG(LogMantleNeckCheck, Warning, TEXT("MANTLE_NECK_CHECK case=%s skipped: %s"), *Case.Name, *Case.Error);
				continue;
			}
			Describe(Case);
		}
		// Is triangle T the same place on a preview face as on a built one? Aera(preview) against Aera(built) is the same character, so
		// same-index distance should be ~0; if it is large while the nearest-centroid distance is ~0, the preview face numbers its triangles differently.
		const auto Find = [&Cases](const TCHAR* Name) -> const FNeckCase* { return Cases.FindByPredicate([Name](const FNeckCase& Case) { return Case.bOk && Case.Name == Name; }); };
		const FNeckCase* AeraBuilt = Find(TEXT("Aera(built)"));
		const FNeckCase* AeraPreview = Find(TEXT("Aera(preview)"));
		const FNeckCase* Male = Find(TEXT("m_med_nrw(in memory)"));
		const FNeckCase* Female = Find(TEXT("f_med_nrw(in memory)"));
		if (AeraPreview && AeraBuilt) { ComparePair(*AeraPreview, *AeraBuilt); }
		if (Female && AeraBuilt) { ComparePair(*Female, *AeraBuilt); }
		if (Male && AeraBuilt) { ComparePair(*Male, *AeraBuilt); }
		if (Male && Female) { ComparePair(*Male, *Female); }
		UE_LOG(LogMantleNeckCheck, Display, TEXT("MANTLE_NECK_CHECK done"));
	}

	FAutoConsoleCommand GMantleDevNeckCheckCommand(
		TEXT("Mantle.Dev.NeckCheck"),
		TEXT("Mantle.Dev.NeckCheck -- read-only: logs MANTLE_NECK_CHECK lines about the neck seam (body vs face, same space) and whether a preview face's triangle T is the same place as a built face's ")
		TEXT("triangle T, for the standard bodies made in memory and the converted Aera (preview and built). Changes nothing."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
