// Copyright Joshua Eger. All Rights Reserved.
#include "MantleMetaHumanHeadOnly.h"

#include "MantleAvatarStripFixer.h"
#include "MantleBodySilhouette.h"
#include "MantleMetaHumanPose.h"
#include "MantleRuntime.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/StaticMesh.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBoneWeightFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "Materials/MaterialInterface.h"
#include "UDynamicMesh.h"
#include "UObject/StrongObjectPtr.h"

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleMetaHumanHeadOnlyPrivate
{
	bool HeadOnlyNamesAny(const FString& Name, std::initializer_list<const TCHAR*> Words)
	{
		const FString Lower = Name.ToLower();
		for (const TCHAR* Word : Words)
		{
			if (Lower.Contains(Word))
			{
				return true;
			}
		}
		return false;
	}
}

FString FMantleCleanHead::Describe() const
{
	return FString::Printf(TEXT("face slots %s; %d triangles kept, %d hair or helmet and %d above the brow or behind the face dropped; eyes at %.1f cm (%s), standing %.1f cm, scaled %.4f"),
		FaceSlots.Num() > 0 ? *FString::Join(FaceSlots, TEXT(", ")) : TEXT("none"), KeptTriangles, DroppedHair, DroppedPosition, EyeHeightCm,
		bEyeSlots ? TEXT("from the eye slots") : TEXT("from the head joint"), SourceHeightCm, Scale);
}

bool FMantleMetaHumanHeadOnly::NamesNotFace(const FString& SlotName)
{
	return MantleMetaHumanHeadOnlyPrivate::HeadOnlyNamesAny(SlotName,
		{ TEXT("hair"), TEXT("helmet"), TEXT("pony"), TEXT("tassle"), TEXT("tassel"), TEXT("hat"), TEXT("mask"), TEXT("glass"), TEXT("band") });
}

bool FMantleMetaHumanHeadOnly::NamesSkin(const FString& SlotName)
{
	return MantleMetaHumanHeadOnlyPrivate::HeadOnlyNamesAny(SlotName, { TEXT("skin") });
}

bool FMantleMetaHumanHeadOnly::NamesHairBone(const FString& BoneName)
{
	return MantleMetaHumanHeadOnlyPrivate::HeadOnlyNamesAny(BoneName,
		{ TEXT("hair"), TEXT("pony"), TEXT("braid"), TEXT("tass"), TEXT("helmet"), TEXT("plume"), TEXT("bang") });
}

bool FMantleMetaHumanHeadOnly::NamesEyes(const FString& SlotName)
{
	// A material instance's name may trail " (Instance)" (VRoid's "N00_000_00_EyeIris_00_EYE (Instance)").
	FString Lower = SlotName.ToLower().TrimStartAndEnd();
	Lower.RemoveFromEnd(TEXT("(instance)"));
	Lower.TrimEndInline();
	return Lower.EndsWith(TEXT("eye")) || Lower.EndsWith(TEXT("eyes"));
}

int32 FMantleMetaHumanHeadOnly::FindHeadBone(const TConstArrayView<FName> Names, const TConstArrayView<int32> Parents)
{
	int32 Best = INDEX_NONE;
	int32 BestDepth = MAX_int32;
	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		const FString Name = Names[Index].ToString().ToLower();
		if (Name == TEXT("head"))
		{
			return Index;
		}
		if (!Name.EndsWith(TEXT("head")) || Name.EndsWith(TEXT("forehead")))
		{
			continue;
		}
		int32 Depth = 0;
		for (int32 Parent = Parents.IsValidIndex(Index) ? Parents[Index] : INDEX_NONE; Parent != INDEX_NONE && Depth < Names.Num();
			Parent = Parents.IsValidIndex(Parent) ? Parents[Parent] : INDEX_NONE)
		{
			++Depth;
		}
		if (Depth < BestDepth)
		{
			Best = Index;
			BestDepth = Depth;
		}
	}
	return Best;
}

bool FMantleMetaHumanHeadOnly::IsCovered(const FMantleStripPlan& Plan, FString& OutWhy)
{
	FBox3d All(ForceInit);
	FBox3d Skin(ForceInit);
	for (const FMantleStripSlot& Slot : Plan.Slots)
	{
		if (!Slot.Bounds.IsValid)
		{
			continue;
		}
		All += Slot.Bounds;
		if (Slot.bSkin)
		{
			Skin += Slot.Bounds;
		}
	}
	if (!Skin.IsValid)
	{
		OutWhy = TEXT("no material on it reads as bare skin");
		return true;
	}
	// A skin slot with clothing modelled into it is not bare (FMantleFusedClothing), whatever it is called.
	for (const FMantleStripSlot& Slot : Plan.Slots)
	{
		if (Slot.bSkin && Slot.Fused == EMantleFusedVerdict::FusedClothing)
		{
			OutWhy = Slot.FusedWhy;
			return true;
		}
	}
	const double Height = All.IsValid ? All.Max.Z - All.Min.Z : 0.0;
	if (Height <= 0.0)
	{
		OutWhy.Reset();
		return false;
	}
	if (Skin.Max.Z - Skin.Min.Z < CoveredSpanShare * Height)
	{
		OutWhy = FString::Printf(TEXT("the parts that read as bare skin cover only %.0f of its %.0f cm"), Skin.Max.Z - Skin.Min.Z, Height);
		return true;
	}
	if (Skin.Min.Z - All.Min.Z > CoveredFeetShare * Height)
	{
		OutWhy = FString::Printf(TEXT("no bare skin reaches its legs and feet (the skin stops %.0f cm above its lowest point)"), Skin.Min.Z - All.Min.Z);
		return true;
	}
	OutWhy.Reset();
	return false;
}

bool FMantleMetaHumanHeadOnly::MakeCleanHead(USkeletalMesh& Source, const double TargetHeightCm, const FString& Folder, const FString& Name,
	FMantleCleanHead& Out, TArray<TWeakObjectPtr<UObject>>& Created, FString& OutError)
{
	Out = FMantleCleanHead();
	UDynamicMesh* Mesh = NewObject<UDynamicMesh>(GetTransientPackage());
	const TStrongObjectPtr<UDynamicMesh> MeshHeld(Mesh);
	FGeometryScriptMeshReadLOD Read;
	Read.LODType = EGeometryScriptLODType::SourceModel;
	Read.LODIndex = 0;
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(&Source, Mesh, FGeometryScriptCopyMeshFromAssetOptions(), Read, Outcome);
	if (Outcome != EGeometryScriptOutcomePins::Success || Mesh->GetTriangleCount() == 0)
	{
		OutError = FString::Printf(TEXT("%s's LOD 0 source geometry could not be read"), *Source.GetName());
		return false;
	}

	// The skeleton as the mesh carries it: the head bone and which bones are below it, and the hair or helmet bones.
	TArray<FGeometryScriptBoneInfo> Bones;
	UGeometryScriptLibrary_MeshBoneWeightFunctions::GetAllBonesInfo(Mesh, Bones);
	TArray<FName> BoneNames;
	TArray<int32> Parents;
	for (const FGeometryScriptBoneInfo& Bone : Bones)
	{
		BoneNames.Add(Bone.Name);
		Parents.Add(Bone.ParentIndex);
	}
	const int32 Head = FindHeadBone(BoneNames, Parents);
	if (Head == INDEX_NONE)
	{
		OutError = TEXT("its skeleton has no head bone, so its face can't be found");
		return false;
	}
	TArray<bool> UnderHead;
	TArray<bool> HairBone;
	UnderHead.SetNumZeroed(Bones.Num());
	HairBone.SetNumZeroed(Bones.Num());
	for (int32 Index = 0; Index < Bones.Num(); ++Index)
	{
		HairBone[Index] = NamesHairBone(BoneNames[Index].ToString());
		for (int32 Walk = Index, Steps = 0; Walk != INDEX_NONE && Steps <= Bones.Num(); Walk = Parents.IsValidIndex(Walk) ? Parents[Walk] : INDEX_NONE, ++Steps)
		{
			if (Walk == Head)
			{
				UnderHead[Index] = true;
				break;
			}
		}
	}
	const FVector HeadJoint = Bones[Head].WorldTransform.GetLocation();

	// Each vertex's share of weight under the head and on hair bones.
	struct FShare
	{
		double Head = 0.0;
		double Hair = 0.0;
	};
	TMap<int32, FShare> Shares;
	auto ShareOf = [&](const int32 Vertex) -> const FShare&
	{
		if (const FShare* Found = Shares.Find(Vertex))
		{
			return *Found;
		}
		TArray<FGeometryScriptBoneWeight> Weights;
		bool bValid = false;
		UGeometryScriptLibrary_MeshBoneWeightFunctions::GetVertexBoneWeights(Mesh, Vertex, Weights, bValid);
		FShare Share;
		double Total = 0.0;
		for (const FGeometryScriptBoneWeight& Weight : Weights)
		{
			Total += Weight.Weight;
			if (UnderHead.IsValidIndex(Weight.BoneIndex) && UnderHead[Weight.BoneIndex])
			{
				Share.Head += Weight.Weight;
			}
			if (HairBone.IsValidIndex(Weight.BoneIndex) && HairBone[Weight.BoneIndex])
			{
				Share.Hair += Weight.Weight;
			}
		}
		if (Total > 0.0)
		{
			Share.Head /= Total;
			Share.Hair /= Total;
		}
		return Shares.Add(Vertex, Share);
	};

	// The face slots: not named as hair, a helmet and the like, and drawn opaque (hair cards are masked or translucent,
	// whatever their names say); eye slots always. Within them a triangle is the head's when each corner has HeadTriangleShare
	// of its weight under the head bone, so a skin slot that covers the whole body gives up its face too.
	const TArray<FSkeletalMaterial>& Materials = Source.GetMaterials();
	auto SlotName = [&Materials](const int32 Id)
	{
		return Materials.IsValidIndex(Id) ? Materials[Id].MaterialSlotName.ToString() : FString::Printf(TEXT("Slot_%d"), Id);
	};
	auto Opaque = [&Materials](const int32 Id)
	{
		const UMaterialInterface* Material = Materials.IsValidIndex(Id) ? Materials[Id].MaterialInterface.Get() : nullptr;
		return !Material || Material->GetBlendMode() == BLEND_Opaque;
	};
	TMap<int32, FVector2D> SlotShare;
	FBox3d Whole(ForceInit);
	Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Read)
	{
		const UE::Geometry::FDynamicMeshMaterialAttribute* Ids = Read.HasAttributes() ? Read.Attributes()->GetMaterialID() : nullptr;
		for (const int32 Triangle : Read.TriangleIndicesItr())
		{
			const int32 Id = Ids ? Ids->GetValue(Triangle) : 0;
			const UE::Geometry::FIndex3i Corners = Read.GetTriangle(Triangle);
			FVector2D& Sum = SlotShare.FindOrAdd(Id);
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				Sum.X += ShareOf(Corners[Corner]).Head;
				Sum.Y += 1.0;
				Whole += Read.GetVertex(Corners[Corner]);
			}
		}
	});
	TSet<int32> FaceSlots;
	for (const TPair<int32, FVector2D>& Slot : SlotShare)
	{
		const FString Named = SlotName(Slot.Key);
		const bool bEyes = NamesEyes(Named);
		// Hair cards: a slot drawn masked or translucent that sits on the head. A masked skin slot that covers the whole body
		// (the face is part of it) stays, and so does one named as skin (VRoid's MToon draws its Face_00_SKIN masked).
		const bool bOnHead = Slot.Value.Y > 0.0 && Slot.Value.X / Slot.Value.Y >= HeadSlotShare;
		if (!bEyes && (NamesNotFace(Named) || (!Opaque(Slot.Key) && bOnHead && !NamesSkin(Named))))
		{
			continue;
		}
		FaceSlots.Add(Slot.Key);
	}
	// A slot counts among the face's when any of its triangles is the head's (found below); listed for the notes.
	TSet<int32> HeadSlotsSeen;

	// Eye height: the eye slots' mean height and front, else a share of the way from the head joint to the head's top.
	double EyeSum = 0.0;
	int32 EyeCount = 0;
	double EyeFront = -UE_BIG_NUMBER;
	double HeadTop = -UE_BIG_NUMBER;
	Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Read)
	{
		const UE::Geometry::FDynamicMeshMaterialAttribute* Ids = Read.HasAttributes() ? Read.Attributes()->GetMaterialID() : nullptr;
		for (const int32 Triangle : Read.TriangleIndicesItr())
		{
			const int32 Id = Ids ? Ids->GetValue(Triangle) : 0;
			if (!FaceSlots.Contains(Id))
			{
				continue;
			}
			const UE::Geometry::FIndex3i Corners = Read.GetTriangle(Triangle);
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const FVector3d Position = Read.GetVertex(Corners[Corner]);
				HeadTop = FMath::Max(HeadTop, Position.Z);
				if (NamesEyes(SlotName(Id)))
				{
					EyeSum += Position.Z;
					++EyeCount;
					EyeFront = FMath::Max(EyeFront, Position.Y);
				}
			}
		}
	});
	Out.bEyeSlots = EyeCount > 0;
	const double EyeZ = Out.bEyeSlots ? EyeSum / EyeCount : HeadJoint.Z + EyeShareAboveHeadJoint * (HeadTop - HeadJoint.Z);
	if (!Out.bEyeSlots)
	{
		// The front of the face at eye height: the face's front-most point within 3 cm of it.
		Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Read)
		{
			for (const int32 Vertex : Read.VertexIndicesItr())
			{
				const FVector3d Position = Read.GetVertex(Vertex);
				if (FMath::Abs(Position.Z - EyeZ) <= 3.0 && ShareOf(Vertex).Head >= HeadSlotShare)
				{
					EyeFront = FMath::Max(EyeFront, Position.Y);
				}
			}
		});
	}
	if (EyeFront <= -UE_BIG_NUMBER)
	{
		OutError = TEXT("the front of its face couldn't be found at eye height");
		return false;
	}
	const double Top = EyeZ + FaceAboveEyesCm;
	const double Back = EyeFront - FaceBehindEyesCm;

	// Everything but the face slots goes; inside them, hair and helmet triangles, and what is above the brow or behind the face.
	Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& Edit)
	{
		const UE::Geometry::FDynamicMeshMaterialAttribute* Ids = Edit.HasAttributes() ? Edit.Attributes()->GetMaterialID() : nullptr;
		TArray<int32> Remove;
		for (const int32 Triangle : Edit.TriangleIndicesItr())
		{
			const int32 Id = Ids ? Ids->GetValue(Triangle) : 0;
			if (!FaceSlots.Contains(Id))
			{
				Remove.Add(Triangle);
				continue;
			}
			const UE::Geometry::FIndex3i Corners = Edit.GetTriangle(Triangle);
			bool bHead = true;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				bHead &= ShareOf(Corners[Corner]).Head >= HeadTriangleShare;
			}
			if (!bHead)
			{
				Remove.Add(Triangle);
				continue;
			}
			bool bHair = true;
			bool bOutside = false;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				bHair &= ShareOf(Corners[Corner]).Hair >= HairBoneShare;
				const FVector3d Position = Edit.GetVertex(Corners[Corner]);
				bOutside |= Position.Z > Top || Position.Y < Back;
			}
			if (bHair)
			{
				++Out.DroppedHair;
				Remove.Add(Triangle);
			}
			else if (bOutside)
			{
				++Out.DroppedPosition;
				Remove.Add(Triangle);
			}
			else
			{
				HeadSlotsSeen.Add(Id);
			}
		}
		for (const int32 Triangle : Remove)
		{
			Edit.RemoveTriangle(Triangle);
		}
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown);
	UGeometryScriptLibrary_MeshRepairFunctions::RemoveUnusedVertices(Mesh);
	UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh);
	Out.KeptTriangles = Mesh->GetTriangleCount();
	for (const int32 Id : HeadSlotsSeen)
	{
		Out.FaceSlots.Add(SlotName(Id));
	}
	if (Out.KeptTriangles == 0)
	{
		OutError = TEXT("nothing was left of its face after hair, helmet and the back of the head were taken off");
		return false;
	}

	// Sized for the body it goes on: the standing height its eyes imply becomes the target.
	const double Floor = Whole.IsValid ? Whole.Min.Z : 0.0;
	Out.EyeHeightCm = EyeZ - Floor;
	Out.SourceHeightCm = Out.EyeHeightCm / EyeToHeight;
	if (!FMath::IsFinite(Out.SourceHeightCm) || Out.SourceHeightCm <= 1.0)
	{
		OutError = FString::Printf(TEXT("its eyes sit %.1f cm above its lowest point, which isn't a standing character"), Out.EyeHeightCm);
		return false;
	}
	// A target of zero or less keeps the source's size (no scale, feet left where they are).
	Out.Scale = TargetHeightCm > 0.0 ? TargetHeightCm / Out.SourceHeightCm : 1.0;
	// Feet on the origin first, so the head sits at the height the eyes imply on a body standing at zero.
	if (TargetHeightCm > 0.0)
	{
		UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(Mesh, FVector(0.0, 0.0, -Floor));
		UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(Mesh, FVector(Out.Scale), FVector::ZeroVector, /*bFixOrientationForNegativeScale*/ true);
		Out.HeadJoint = (HeadJoint - FVector(0.0, 0.0, Floor)) * Out.Scale;
	}
	else
	{
		Out.HeadJoint = HeadJoint;
	}

	FGeometryScriptCreateNewStaticMeshAssetOptions MeshOptions;
	MeshOptions.bEnableRecomputeNormals = false;
	MeshOptions.bEnableRecomputeTangents = false;
	MeshOptions.bEnableNanite = false;
	MeshOptions.bEnableCollision = false;
	const FString PackageName = FMantleMetaHumanSteps::UniquePackageName(Folder, FString::Printf(TEXT("SM_%s_FaceOnly"), *Name));
	UStaticMesh* Static = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(Mesh, PackageName, MeshOptions, Outcome);
	if (Static)
	{
		Created.Add(Static);
	}
	if (Outcome != EGeometryScriptOutcomePins::Success || !Static)
	{
		OutError = TEXT("the clean head's static mesh could not be made");
		return false;
	}
	// The source's own materials, so the portrait sees the face as it renders (material IDs keep the source's slot numbers).
	TArray<FStaticMaterial> StaticMaterials = Static->GetStaticMaterials();
	for (int32 Slot = 0; Slot < StaticMaterials.Num(); ++Slot)
	{
		UMaterialInterface* Material = Materials.IsValidIndex(Slot) ? Materials[Slot].MaterialInterface.Get() : nullptr;
		StaticMaterials[Slot].MaterialInterface = Material;
		StaticMaterials[Slot].MaterialSlotName = Material ? FName(*Material->GetName()) : FName(*FString::Printf(TEXT("Slot_%d"), Slot));
	}
	Static->SetStaticMaterials(StaticMaterials);
	Static->MarkPackageDirty();
	Out.Mesh = Static;
	UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT_FACEONLY clean head %s from %s: %s"), *Static->GetPathName(), *Source.GetName(), *Out.Describe());
	return true;
}

bool FMantleMetaHumanHeadOnly::MeasureBuild(USkeletalMesh& Source, const double StandingHeightCm, FMantleSilhouette& Out, FString& OutError)
{
	Out = FMantleSilhouette();
	UDynamicMesh* Mesh = NewObject<UDynamicMesh>(GetTransientPackage());
	const TStrongObjectPtr<UDynamicMesh> MeshHeld(Mesh);
	FGeometryScriptMeshReadLOD Read;
	Read.LODType = EGeometryScriptLODType::SourceModel;
	Read.LODIndex = 0;
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(&Source, Mesh, FGeometryScriptCopyMeshFromAssetOptions(), Read, Outcome);
	if (Outcome != EGeometryScriptOutcomePins::Success || Mesh->GetTriangleCount() == 0)
	{
		OutError = FString::Printf(TEXT("%s's LOD 0 source geometry could not be read"), *Source.GetName());
		return false;
	}
	TArray<FVector3f> Vertices;
	double FeetZ = TNumericLimits<double>::Max();
	Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Geometry)
	{
		for (const int32 Vertex : Geometry.VertexIndicesItr())
		{
			const FVector3f Position(Geometry.GetVertex(Vertex));
			Vertices.Add(Position);
			FeetZ = FMath::Min(FeetZ, static_cast<double>(Position.Z));
		}
	});
	FMantleSilhouette Cm;
	if (!FMantleBodySilhouette::Measure(Vertices, FeetZ, StandingHeightCm, Cm))
	{
		OutError = TEXT("no torso could be found at its chest, waist and hips");
		return false;
	}
	Out = FMantleBodySilhouette::Normalised(Cm, StandingHeightCm);
	return true;
}
