// Copyright Joshua Eger. All Rights Reserved.
#include "MantleGarmentOutfit.h"

#include "MantleAvatarContract.h"
#include "MantleAvatarConverter.h"
#include "MantleAvatarReposeFixer.h"
#include "MantleAvatarStripFixer.h"
#include "MantleCosmeticExport.h"
#include "MantleMetaHumanPose.h"
#include "MantleParametricOutfitAuthoring.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ChaosOutfitAsset/OutfitAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "PreviewScene.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SkinnedAssetCompiler.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogMantleGarmentOutfit, Log, All);

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleGarmentOutfitPrivate
{
	/** Points hashed into cubic cells, for nearest-point searches that grow ring by ring. */
	struct FGarmentPointGrid
	{
		TConstArrayView<FVector3f> Points;
		double Cell = 1.5;
		TMap<FIntVector, TArray<int32>> Cells;

		FGarmentPointGrid(TConstArrayView<FVector3f> InPoints, const double InCell)
			: Points(InPoints), Cell(InCell)
		{
			for (int32 Index = 0; Index < Points.Num(); ++Index)
			{
				Cells.FindOrAdd(CellOf(Points[Index])).Add(Index);
			}
		}

		FIntVector CellOf(const FVector3f& Point) const
		{
			return FIntVector(FMath::FloorToInt32(Point.X / Cell), FMath::FloorToInt32(Point.Y / Cell), FMath::FloorToInt32(Point.Z / Cell));
		}

		/** The K nearest points within MaxCm, nearest first (fewer when there are not K within it). */
		void Nearest(const FVector3f& Point, const int32 K, const double MaxCm, TArray<TPair<double, int32>>& Out) const
		{
			Out.Reset();
			const FIntVector Center = CellOf(Point);
			const int32 MaxRing = FMath::CeilToInt32(MaxCm / Cell) + 1;
			for (int32 Ring = 0; Ring <= MaxRing; ++Ring)
			{
				for (int32 X = -Ring; X <= Ring; ++X)
				{
					for (int32 Y = -Ring; Y <= Ring; ++Y)
					{
						// Only the ring's shell: every Z on its sides, the top and bottom faces elsewhere.
						const bool bSide = FMath::Max(FMath::Abs(X), FMath::Abs(Y)) == Ring;
						const int32 ZStep = bSide || Ring == 0 ? 1 : 2 * Ring;
						for (int32 Z = -Ring; Z <= Ring; Z += ZStep)
						{
							const TArray<int32>* Found = Cells.Find(Center + FIntVector(X, Y, Z));
							if (!Found)
							{
								continue;
							}
							for (const int32 Index : *Found)
							{
								const double Distance = FVector3d::Dist(FVector3d(Point), FVector3d(Points[Index]));
								if (Distance <= MaxCm)
								{
									Out.Emplace(Distance, Index);
								}
							}
						}
					}
				}
				// Every point within Ring cells of the centre cell is now in: they are the nearest when the Kth is that close.
				if (Out.Num() >= K)
				{
					Out.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
					if (Out[K - 1].Key <= Ring * Cell)
					{
						Out.SetNum(K);
						return;
					}
				}
			}
			Out.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
			if (Out.Num() > K)
			{
				Out.SetNum(K);
			}
		}
	};

	/** Component-space transforms of Joints' reference pose (parents first). */
	TArray<FTransform> GarmentBindPose(const TArray<FMantleGlbJoint>& Joints)
	{
		TArray<FTransform> Out;
		Out.SetNum(Joints.Num());
		for (int32 Joint = 0; Joint < Joints.Num(); ++Joint)
		{
			const int32 Parent = Joints[Joint].Parent;
			Out[Joint] = Out.IsValidIndex(Parent) && Parent < Joint ? Joints[Joint].Local * Out[Parent] : Joints[Joint].Local;
		}
		return Out;
	}

	/**
	 * Each of Joints posed: the clip's bone of the same name (ByName), else the joint's own reference offset from its
	 * posed parent. OutMatched counts the joints the clip had.
	 */
	TArray<FTransform> GarmentPosedJoints(const TArray<FMantleGlbJoint>& Joints, const TMap<FName, FTransform>& ByName, int32& OutMatched)
	{
		OutMatched = 0;
		TArray<FTransform> Out;
		Out.SetNum(Joints.Num());
		for (int32 Joint = 0; Joint < Joints.Num(); ++Joint)
		{
			if (const FTransform* Found = ByName.Find(Joints[Joint].Name))
			{
				Out[Joint] = *Found;
				++OutMatched;
				continue;
			}
			const int32 Parent = Joints[Joint].Parent;
			Out[Joint] = Out.IsValidIndex(Parent) && Parent < Joint ? Joints[Joint].Local * Out[Parent] : Joints[Joint].Local;
		}
		return Out;
	}

	/** Every bone of Mesh posed by Clip (frame 0), component space, by name, evaluated on a component in a private scene. */
	bool GarmentClipBones(USkeletalMesh& Mesh, UAnimSequence& Clip, TMap<FName, FTransform>& Out, FString& OutError)
	{
		TArray<USkinnedAsset*> Compiling = { &Mesh };
		FSkinnedAssetCompilingManager::Get().FinishCompilation(Compiling);
		FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreatePhysicsScene(false).ShouldSimulatePhysics(false));
		USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		Component->SetSkeletalMeshAsset(&Mesh);
		Scene.AddComponent(Component, FTransform::Identity);
		Component->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Component->OverrideAnimationData(&Clip, /*bIsLooping*/ false, /*bIsPlaying*/ false, 0.0f, 1.0f);
		const TArray<FTransform>& Space = Component->GetComponentSpaceTransforms();
		const FReferenceSkeleton& Reference = Mesh.GetRefSkeleton();
		for (int32 Bone = 0; Bone < Reference.GetRawBoneNum() && Bone < Space.Num(); ++Bone)
		{
			Out.Add(Reference.GetBoneName(Bone), Space[Bone]);
		}
		Scene.RemoveComponent(Component);
		if (Out.IsEmpty())
		{
			OutError = FString::Printf(TEXT("the A-pose clip gave no bones on %s"), *Mesh.GetName());
			return false;
		}
		return true;
	}

	bool GarmentSave(UObject* Asset)
	{
		UPackage* Package = Asset ? Asset->GetPackage() : nullptr;
		if (!Package)
		{
			return false;
		}
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Package, Asset, *Filename, Args);
	}

	/** A new USkeleton asset Folder/<Name> built from Mesh's bones, set on Mesh. */
	USkeleton* GarmentGiveSkeleton(USkeletalMesh& Mesh, const FString& Folder, const FString& Name, FString& OutError)
	{
		const FString PackageName = FMantleMetaHumanSteps::UniquePackageName(Folder, Name);
		UPackage* Package = CreatePackage(*PackageName);
		USkeleton* Skeleton = Package ? NewObject<USkeleton>(Package, FName(*FPackageName::GetShortName(PackageName)), RF_Public | RF_Standalone | RF_Transactional) : nullptr;
		if (!Skeleton || !Skeleton->MergeAllBonesToBoneTree(&Mesh, false))
		{
			OutError = FString::Printf(TEXT("a skeleton could not be made for %s"), *Mesh.GetName());
			return nullptr;
		}
		Mesh.SetSkeleton(Skeleton);
		Mesh.MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Skeleton);
		if (!GarmentSave(Skeleton))
		{
			OutError = FString::Printf(TEXT("%s could not be saved"), *PackageName);
			return nullptr;
		}
		return Skeleton;
	}

	USkeletalMesh* GarmentLoadMesh(const FString& InPath)
	{
		FString Path = InPath.TrimStartAndEnd();
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		if (!Path.Contains(TEXT(".")))
		{
			Path += TEXT(".") + FPackageName::GetShortName(Path);
		}
		return LoadObject<USkeletalMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	FString GarmentWriteJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	TArray<FString> GarmentKeys(const FString& List)
	{
		TArray<FString> Out;
		List.ParseIntoArray(Out, TEXT(","), true);
		for (FString& Key : Out)
		{
			Key.TrimStartAndEndInline();
		}
		Out.RemoveAll([](const FString& Key) { return Key.IsEmpty(); });
		return Out;
	}
}

USkeletalMesh* FMantleGarmentOutfit::SaveCopy(USkeletalMesh& Source, const FString& Folder, const FString& Name, FString& OutError)
{
	using namespace MantleGarmentOutfitPrivate;
	TArray<FName> Names;
	TArray<int32> Parents;
	TArray<FTransform> Local;
	FMantleAvatarRepose::ReadMesh(Source, Names, Parents, Local);
	const FString PackageName = FMantleMetaHumanSteps::UniquePackageName(Folder, Name);
	USkeletalMesh* Copy = FMantleAvatarRepose::WriteWithPose(&Source, Local, Folder, FPackageName::GetShortName(PackageName),
		[](int32, FMeshDescription&, FString&) { return true; }, OutError);
	if (!Copy)
	{
		return nullptr;
	}
	auto IsTransient = [](const UObject* Object) { return Object && (Object->HasAnyFlags(RF_Transient) || Object->GetPackage() == GetTransientPackage()); };
	if (!Copy->GetSkeleton() || IsTransient(Copy->GetSkeleton()))
	{
		if (!GarmentGiveSkeleton(*Copy, Folder, FPackageName::GetShortName(PackageName) + TEXT("_Skeleton"), OutError))
		{
			return nullptr;
		}
	}
	// Creator's preview materials are made in memory: a saved asset can't point at them.
	for (FSkeletalMaterial& Slot : Copy->GetMaterials())
	{
		UMaterialInterface* Material = Slot.MaterialInterface;
		while (IsTransient(Material))
		{
			const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
			Material = Instance ? Instance->Parent.Get() : nullptr;
		}
		Slot.MaterialInterface = Material;
	}
	if (IsTransient(Copy->GetPhysicsAsset()))
	{
		Copy->SetPhysicsAsset(nullptr);
	}
	if (!GarmentSave(Copy))
	{
		OutError = FString::Printf(TEXT("%s could not be saved"), *Copy->GetPathName());
		return nullptr;
	}
	return Copy;
}

bool FMantleGarmentOutfit::PoseMesh(FMantleGlbMesh& Mesh, TConstArrayView<FTransform> Posed, const double Scale, FString& OutError)
{
	using namespace MantleGarmentOutfitPrivate;
	if (Posed.Num() != Mesh.Joints.Num() || Mesh.NumInfluences <= 0 || !FMath::IsFinite(Scale) || Scale <= 0.0)
	{
		OutError = TEXT("the pose does not match the mesh's skeleton");
		return false;
	}
	const TArray<FTransform> Bind = GarmentBindPose(Mesh.Joints);
	TArray<FMatrix> Skin;
	Skin.SetNum(Bind.Num());
	for (int32 Joint = 0; Joint < Bind.Num(); ++Joint)
	{
		Skin[Joint] = Bind[Joint].ToMatrixWithScale().Inverse() * Posed[Joint].ToMatrixWithScale();
	}
	const int32 Slots = Mesh.NumInfluences;
	for (int32 Vertex = 0; Vertex < Mesh.Positions.Num(); ++Vertex)
	{
		FVector3d Position(0.0);
		FVector3d Normal(0.0);
		FVector3d Tangent(0.0);
		double Sum = 0.0;
		const FVector3d BindPosition(Mesh.Positions[Vertex]);
		const FVector3d BindNormal = Mesh.Normals.IsValidIndex(Vertex) ? FVector3d(Mesh.Normals[Vertex]) : FVector3d::ZeroVector;
		const FVector3d BindTangent = Mesh.Tangents.IsValidIndex(Vertex) ? FVector3d(Mesh.Tangents[Vertex].X, Mesh.Tangents[Vertex].Y, Mesh.Tangents[Vertex].Z) : FVector3d::ZeroVector;
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			const int32 Index = Vertex * Slots + Slot;
			if (!Mesh.InfluenceWeights.IsValidIndex(Index) || !Mesh.InfluenceJoints.IsValidIndex(Index))
			{
				OutError = TEXT("the mesh's skin weights are incomplete");
				return false;
			}
			const double Weight = Mesh.InfluenceWeights[Index];
			if (Weight <= 0.0)
			{
				continue;
			}
			const int32 Joint = Mesh.InfluenceJoints[Index];
			if (!Skin.IsValidIndex(Joint))
			{
				OutError = TEXT("a skin weight names a bone the mesh does not have");
				return false;
			}
			Position += Weight * Skin[Joint].TransformPosition(BindPosition);
			Normal += Weight * Skin[Joint].TransformVector(BindNormal);
			Tangent += Weight * Skin[Joint].TransformVector(BindTangent);
			Sum += Weight;
		}
		if (Sum <= UE_SMALL_NUMBER)
		{
			Position = BindPosition;
			Normal = BindNormal;
			Tangent = BindTangent;
		}
		else
		{
			Position /= Sum;
		}
		Mesh.Positions[Vertex] = FVector3f(Position * Scale);
		if (Mesh.Normals.IsValidIndex(Vertex))
		{
			Mesh.Normals[Vertex] = FVector3f(Normal.GetSafeNormal());
		}
		if (Mesh.Tangents.IsValidIndex(Vertex))
		{
			const FVector3f Turned(Tangent.GetSafeNormal());
			Mesh.Tangents[Vertex] = FVector4f(Turned.X, Turned.Y, Turned.Z, Mesh.Tangents[Vertex].W);
		}
	}
	return true;
}

FVector3f FMantleGarmentOutfit::AlignTranslation(TConstArrayView<FVector3f> From, TConstArrayView<FVector3f> To, double& OutMeanCm)
{
	using namespace MantleGarmentOutfitPrivate;
	OutMeanCm = 0.0;
	if (From.IsEmpty() || To.IsEmpty())
	{
		return FVector3f::ZeroVector;
	}
	constexpr double PairCm = 20.0;
	const FGarmentPointGrid Grid(To, 2.0);
	const int32 Stride = FMath::Max(1, From.Num() / 8000);
	FVector3d Offset(0.0);
	TArray<TPair<double, int32>> Found;
	for (int32 Iteration = 0; Iteration < 12; ++Iteration)
	{
		FVector3d Sum(0.0);
		int32 Pairs = 0;
		for (int32 Index = 0; Index < From.Num(); Index += Stride)
		{
			const FVector3f Point = From[Index] + FVector3f(Offset);
			Grid.Nearest(Point, 1, PairCm, Found);
			if (Found.Num() > 0)
			{
				Sum += FVector3d(To[Found[0].Value]) - FVector3d(Point);
				++Pairs;
			}
		}
		if (Pairs == 0)
		{
			break;
		}
		const FVector3d Step = Sum / Pairs;
		Offset += Step;
		if (Step.Size() < 0.005)
		{
			break;
		}
	}
	double Total = 0.0;
	int32 Count = 0;
	for (int32 Index = 0; Index < From.Num(); Index += Stride)
	{
		Grid.Nearest(From[Index] + FVector3f(Offset), 1, PairCm, Found);
		if (Found.Num() > 0)
		{
			Total += Found[0].Key;
			++Count;
		}
	}
	OutMeanCm = Count > 0 ? Total / Count : PairCm;
	return FVector3f(Offset);
}

bool FMantleGarmentOutfit::TransferWeights(FMantleGlbMesh& Garment, const FMantleGlbMesh& Body, int32& OutFar, double& OutMeanCm, FString& OutError)
{
	using namespace MantleGarmentOutfitPrivate;
	OutFar = 0;
	OutMeanCm = 0.0;
	if (!Body.IsSkinned() || Body.NumInfluences <= 0 || Body.Positions.IsEmpty())
	{
		OutError = TEXT("the MetaHuman body has no skin weights to take");
		return false;
	}
	constexpr int32 MaxSlots = 8;
	const FGarmentPointGrid Grid(Body.Positions, 1.5);
	TArray<uint16> Joints;
	TArray<float> Weights;
	Joints.Reserve(Garment.Positions.Num() * MaxSlots);
	Weights.Reserve(Garment.Positions.Num() * MaxSlots);
	TArray<TPair<double, int32>> Found;
	TArray<TPair<int32, double>> Blend;
	double Total = 0.0;
	int32 UsedSlots = 1;
	for (int32 Vertex = 0; Vertex < Garment.Positions.Num(); ++Vertex)
	{
		// The search widens until it finds skin: a garment far from the body still follows its nearest bones.
		const FVector3f Point = Garment.Positions[Vertex];
		Grid.Nearest(Point, TransferNeighbours, NearCm * 3.0, Found);
		if (Found.IsEmpty())
		{
			Grid.Nearest(Point, TransferNeighbours, 200.0, Found);
		}
		if (Found.IsEmpty())
		{
			OutError = TEXT("a garment vertex is more than 2 m from the MetaHuman body");
			return false;
		}
		Total += Found[0].Key;
		OutFar += Found[0].Key > NearCm ? 1 : 0;
		Blend.Reset();
		for (const TPair<double, int32>& Near : Found)
		{
			const double Share = 1.0 / (Near.Key + 0.5);
			for (int32 Slot = 0; Slot < Body.NumInfluences; ++Slot)
			{
				const int32 Index = Near.Value * Body.NumInfluences + Slot;
				const float Weight = Body.InfluenceWeights.IsValidIndex(Index) ? Body.InfluenceWeights[Index] : 0.0f;
				if (Weight <= 0.0f)
				{
					continue;
				}
				const int32 Joint = Body.InfluenceJoints[Index];
				TPair<int32, double>* Existing = Blend.FindByPredicate([Joint](const TPair<int32, double>& Entry) { return Entry.Key == Joint; });
				if (Existing)
				{
					Existing->Value += Share * Weight;
				}
				else
				{
					Blend.Emplace(Joint, Share * Weight);
				}
			}
		}
		Blend.Sort([](const TPair<int32, double>& A, const TPair<int32, double>& B) { return A.Value > B.Value; });
		if (Blend.Num() > MaxSlots)
		{
			Blend.SetNum(MaxSlots);
		}
		double Sum = 0.0;
		for (const TPair<int32, double>& Entry : Blend)
		{
			Sum += Entry.Value;
		}
		if (Sum <= 0.0)
		{
			OutError = TEXT("the MetaHuman skin near a garment vertex has no weights");
			return false;
		}
		UsedSlots = FMath::Max(UsedSlots, Blend.Num());
		for (int32 Slot = 0; Slot < MaxSlots; ++Slot)
		{
			Joints.Add(Blend.IsValidIndex(Slot) ? static_cast<uint16>(Blend[Slot].Key) : 0);
			Weights.Add(Blend.IsValidIndex(Slot) ? static_cast<float>(Blend[Slot].Value / Sum) : 0.0f);
		}
	}
	// Cut the slots to the most any vertex uses.
	Garment.InfluenceJoints.Reset();
	Garment.InfluenceWeights.Reset();
	for (int32 Vertex = 0; Vertex < Garment.Positions.Num(); ++Vertex)
	{
		for (int32 Slot = 0; Slot < UsedSlots; ++Slot)
		{
			Garment.InfluenceJoints.Add(Joints[Vertex * MaxSlots + Slot]);
			Garment.InfluenceWeights.Add(Weights[Vertex * MaxSlots + Slot]);
		}
	}
	Garment.NumInfluences = UsedSlots;
	Garment.Joints = Body.Joints;
	OutMeanCm = Garment.Positions.IsEmpty() ? 0.0 : Total / Garment.Positions.Num();
	return true;
}

bool FMantleGarmentOutfit::Make(USkeletalMesh& InBody, TConstArrayView<USkeletalMesh*> Garments, const FString& TurnOff, const FString& Confirm,
	FMantleGarmentOutfitResult& OutResult, FString& OutConvertJson, FString& OutRefusalCode, FString& OutRefusal)
{
	using namespace MantleGarmentOutfitPrivate;
	OutResult = FMantleGarmentOutfitResult();
	FString ContractError;
	const FMantleAvatarContract* Contract = FMantleAvatarContract::Get(&ContractError);
	if (!Contract)
	{
		OutRefusalCode = TEXT("no_contract");
		OutRefusal = ContractError.IsEmpty() ? FString(TEXT("Mantle's avatar rules (limits.json) could not be read.")) : ContractError;
		return false;
	}
	const FString BodyTag = FMantleMetaHumanSteps::AssetTag(InBody.GetName());
	const FString Folder = FMantleAvatarConverter::OutputFolderFor(FPackageName::GetLongPackagePath(InBody.GetPackage()->GetName())) / (BodyTag + TEXT("_Outfit"));
	OutResult.OutputFolder = Folder;
	UE_LOG(LogMantleGarmentOutfit, Display, TEXT("MANTLE_GARMENT_OUTFIT body=%s garments=%d folder=%s"), *InBody.GetPathName(), Garments.Num(), *Folder);

	// 1. A body with bones but no USkeleton gets one, on a copy: Epic's IK rig and retargeter need a skeleton.
	USkeletalMesh* Body = &InBody;
	if (!Body->GetSkeleton())
	{
		FString Error;
		USkeletalMesh* Copy = FMantleGarmentOutfit::SaveCopy(InBody, Folder, BodyTag + TEXT("_Skinned"), Error);
		if (!Copy)
		{
			OutRefusalCode = TEXT("no_skeleton");
			OutRefusal = FString::Printf(TEXT("%s has no skeleton asset, and one could not be made for it: %s."), *InBody.GetName(), *Error);
			return false;
		}
		if (!Copy->GetSkeleton() || Copy->GetSkeleton()->HasAnyFlags(RF_Transient))
		{
			if (!GarmentGiveSkeleton(*Copy, Folder, BodyTag + TEXT("_Skeleton"), Error) || !GarmentSave(Copy))
			{
				OutRefusalCode = TEXT("no_skeleton");
				OutRefusal = FString::Printf(TEXT("%s has no skeleton asset, and one could not be made for it: %s."), *InBody.GetName(), *Error);
				return false;
			}
		}
		OutResult.Notes.Add(FString::Printf(TEXT("%s has no skeleton asset, so a copy with its own skeleton was made: %s."), *InBody.GetName(), *Copy->GetPathName()));
		Body = Copy;
	}

	// Convert's MetaHuman steps on the body, and nothing after them.
	FMantleConvertOptions Options;
	Options.Slot = TEXT("avatar");
	Options.OutputFolder = Folder;
	Options.bSave = true;
	Options.bCheck = false;
	Options.OnlyFixers = { MantleAvatarFixerIds::Strip, MantleAvatarFixerIds::MHPose, MantleAvatarFixerIds::MHConform };
	FMantleConvertContext Context;
	FString Error;
	if (!FMantleAvatarConverter::MakeContext(Body, Options, *Contract, Context, Error))
	{
		OutRefusalCode = TEXT("not_convertible");
		OutRefusal = Error;
		return false;
	}
	if (Context.Parts.Num() != 1 || Context.Parts[0].bUnrigged)
	{
		OutRefusalCode = TEXT("not_convertible");
		OutRefusal = Context.Parts.Num() == 1 ? FString::Printf(TEXT("%s has no skeleton to pose, so its clothing can't be posed the way it is."), *Body->GetName())
			: FString::Printf(TEXT("%s is not one body: pass the character's bare body mesh."), *Body->GetName());
		return false;
	}
	TArray<FMantleConvertFix> Fixes = FMantleAvatarConverter::Analyze(Context);
	const TArray<FString> Keys = FMantleAvatarConverter::FixKeys(Fixes);
	TArray<TSharedPtr<FJsonValue>> UnknownTurnOff;
	TArray<TSharedPtr<FJsonValue>> UnknownConfirm;
	for (int32 Index = 0; Index < Fixes.Num(); ++Index)
	{
		// The garment was made for this body: its face never goes on another one.
		const FName Kind = Fixes[Index].Kind;
		if (Kind == FMantleAvatarStripFixer::KindFaceOnly || Kind == FMantleAvatarStripFixer::KindFeminineBody || Kind == FMantleAvatarStripFixer::KindMasculineBody)
		{
			Fixes[Index].bEnabled = false;
		}
	}
	for (const FString& Wanted : GarmentKeys(TurnOff))
	{
		const int32 Found = Keys.IndexOfByKey(Wanted);
		if (Found == INDEX_NONE) { UnknownTurnOff.Add(MakeShared<FJsonValueString>(Wanted)); }
		else { Fixes[Found].bEnabled = false; }
	}
	// Any line may be confirmed here, the bare-skin line too when no material read as skin (the creator kept the slots).
	for (const FString& Wanted : GarmentKeys(Confirm))
	{
		const int32 Found = Keys.IndexOfByKey(Wanted);
		if (Found == INDEX_NONE) { UnknownConfirm.Add(MakeShared<FJsonValueString>(Wanted)); }
		else { Fixes[Found].bEnabled = true; }
	}
	const FMantleConvertResult Converted = FMantleAvatarConverter::Convert(Context, Fixes, Options, *Contract);
	{
		TSharedRef<FJsonObject> Json = FMantleAvatarConverter::ToJson(Context, Fixes, &Converted, *Contract);
		Json->SetArrayField(TEXT("unknownTurnOff"), UnknownTurnOff);
		Json->SetArrayField(TEXT("unknownConfirm"), UnknownConfirm);
		OutConvertJson = GarmentWriteJson(Json);
	}
	const FMantleConvertMetaHumanWork* Work = Context.MetaHuman.Find(0);
	if (Converted.Failed.Num() > 0 || !Work || !Work->Character.IsValid() || !Work->APoseClip.IsValid() || !Work->SkinOnly.IsValid() || !Work->PosedMesh.IsValid())
	{
		TArray<FString> Reasons;
		for (const FMantleConvertOutcome& Failed : Converted.Failed)
		{
			Reasons.Add(FString::Printf(TEXT("%s: %s"), *Failed.Key, *Failed.Reason));
		}
		OutRefusalCode = TEXT("convert_failed");
		OutRefusal = Reasons.Num() > 0 ? FString::Printf(TEXT("The body could not be made a MetaHuman: %s."), *FString::Join(Reasons, TEXT("; ")))
			: FString(TEXT("The body could not be made a MetaHuman: Strip, the A-pose or the conform did not run (see the fix list in \"convert\")."));
		return false;
	}
	OutResult.Character = Work->Character->GetPathName();
	OutResult.InputScale = Work->InputScale;

	// 2. The MetaHuman's merged body and head, as Epic's resize reads a character: the Outfit's authored body.
	UMetaHumanCharacter* Character = Cast<UMetaHumanCharacter>(Work->Character.Get());
	UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>() : nullptr;
	if (!Character || !Subsystem)
	{
		OutRefusalCode = TEXT("no_metahuman");
		OutRefusal = TEXT("MetaHuman Creator's editor subsystem is unavailable.");
		return false;
	}
	const bool bAlreadyEditing = Subsystem->IsObjectAddedForEditing(Character);
	if (!bAlreadyEditing && !Subsystem->TryAddObjectToEdit(Character))
	{
		OutRefusalCode = TEXT("no_metahuman");
		OutRefusal = TEXT("MetaHuman Creator could not open the conformed character.");
		return false;
	}
	ON_SCOPE_EXIT
	{
		if (!bAlreadyEditing)
		{
			Subsystem->RemoveObjectToEdit(Character);
		}
	};
	FMetaHumanCharacterPreviewAssetOptions PreviewOptions;
	PreviewOptions.bGenerateMergedHeadAndBodyMesh = true;
	PreviewOptions.bGenerateBodyMeasurements = true;
	FMetaHumanCharacterPreviewAssets Preview;
	if (!Subsystem->TryGetCharacterPreviewAssets(Character, PreviewOptions, Preview) || !Preview.MergedHeadAndBodyMesh)
	{
		OutRefusalCode = TEXT("no_metahuman");
		OutRefusal = TEXT("MetaHuman Creator could not provide the conformed character's merged body and head.");
		return false;
	}
	USkeletalMesh* Authoring = FMantleGarmentOutfit::SaveCopy(*Preview.MergedHeadAndBodyMesh, Folder, TEXT("SKM_") + BodyTag + TEXT("_OutfitBody"), Error);
	if (!Authoring)
	{
		OutRefusalCode = TEXT("no_metahuman");
		OutRefusal = FString::Printf(TEXT("The MetaHuman's merged body and head could not be saved: %s."), *Error);
		return false;
	}
	OutResult.AuthoringBody = Authoring->GetPathName();
	FMantleGlbMesh MetaHumanSkin;
	TArray<UMaterialInterface*> SkinMaterials;
	int32 Triangles = 0;
	if (!FMantleCosmeticExport::ReadSkeletalMesh(Authoring, 0, MetaHumanSkin, SkinMaterials, Triangles, Error) || !MetaHumanSkin.IsSkinned())
	{
		OutRefusalCode = TEXT("no_metahuman");
		OutRefusal = FString::Printf(TEXT("The MetaHuman's merged body and head could not be read: %s."), *Error);
		return false;
	}

	// The body posed by its own A-pose clip and scaled by k, as the conform's input was: the check that this pose is the
	// A-pose step's, and the offset that puts it on the MetaHuman's skin.
	TMap<FName, FTransform> ClipBones;
	if (!GarmentClipBones(*Work->SkinOnly.Get(), *Work->APoseClip.Get(), ClipBones, Error))
	{
		OutRefusalCode = TEXT("pose_failed");
		OutRefusal = Error;
		return false;
	}
	FMantleGlbMesh PosedBody;
	TArray<UMaterialInterface*> BodyMaterials;
	int32 Matched = 0;
	if (!FMantleCosmeticExport::ReadSkeletalMesh(Work->SkinOnly.Get(), 0, PosedBody, BodyMaterials, Triangles, Error)
		|| !PoseMesh(PosedBody, GarmentPosedJoints(PosedBody.Joints, ClipBones, Matched), Work->InputScale, Error))
	{
		OutRefusalCode = TEXT("pose_failed");
		OutRefusal = FString::Printf(TEXT("The body could not be posed: %s."), *Error);
		return false;
	}
	FBox3f PosedBox(ForceInit);
	for (const FVector3f& Position : PosedBody.Positions)
	{
		PosedBox += Position;
	}
	const FBox ConformBox = Work->PosedMesh->GetBoundingBox();
	const double PoseGap = FMath::Max((FVector(PosedBox.Min) - ConformBox.Min).GetAbsMax(), (FVector(PosedBox.Max) - ConformBox.Max).GetAbsMax());
	UE_LOG(LogMantleGarmentOutfit, Display, TEXT("MANTLE_GARMENT_OUTFIT posed body bounds %s vs conform input %s: %.3f cm"), *PosedBox.ToString(), *ConformBox.ToString(), PoseGap);
	if (PoseGap > PoseCheckCm)
	{
		OutRefusalCode = TEXT("pose_failed");
		OutRefusal = FString::Printf(TEXT("Posing the body again gave a shape %.1f cm from the one the MetaHuman was made from, so its clothing would not be posed the same way."), PoseGap);
		return false;
	}
	OutResult.Offset = AlignTranslation(PosedBody.Positions, MetaHumanSkin.Positions, OutResult.BodyGapCm);
	UE_LOG(LogMantleGarmentOutfit, Display, TEXT("MANTLE_GARMENT_OUTFIT k=%.5f offset=%s body gap %.3f cm"), OutResult.InputScale, *OutResult.Offset.ToString(), OutResult.BodyGapCm);
	OutResult.Notes.Add(FString::Printf(TEXT("The MetaHuman's skin is on average %.2f cm from the original body's (after the A-pose and the scale %.4f)."), OutResult.BodyGapCm, OutResult.InputScale));

	// 3 and 4, per garment.
	for (USkeletalMesh* Garment : Garments)
	{
		FMantleGarmentOutfitPiece& Piece = OutResult.Pieces.AddDefaulted_GetRef();
		Piece.Garment = Garment ? Garment->GetPathName() : FString();
		if (!Garment)
		{
			Piece.Error = TEXT("not a skeletal mesh");
			continue;
		}
		TSharedRef<FMantleGlbMesh> Geometry = MakeShared<FMantleGlbMesh>();
		TArray<UMaterialInterface*> Materials;
		if (!FMantleCosmeticExport::ReadSkeletalMesh(Garment, 0, *Geometry, Materials, Triangles, Error))
		{
			Piece.Error = Error;
			continue;
		}
		const TArray<FTransform> Posed = GarmentPosedJoints(Geometry->Joints, ClipBones, Matched);
		if (Matched == 0)
		{
			Piece.Error = FString::Printf(TEXT("%s shares no bones with %s, so it can't be posed the way the body was"), *Garment->GetName(), *Body->GetName());
			continue;
		}
		if (!PoseMesh(*Geometry, Posed, Work->InputScale, Error))
		{
			Piece.Error = Error;
			continue;
		}
		for (FVector3f& Position : Geometry->Positions)
		{
			Position += OutResult.Offset;
		}
		if (!TransferWeights(*Geometry, MetaHumanSkin, Piece.FarVertices, Piece.MeanSkinCm, Error))
		{
			Piece.Error = Error;
			continue;
		}
		Piece.Vertices = Geometry->Positions.Num();
		FMantleOutfitAuthoredSource Source;
		Source.Id = TEXT("authored");
		Source.CombinedBodyHead = Authoring;
		Source.Geometry = Geometry;
		Source.Materials = Materials;
		const FString PackageName = FMantleMetaHumanSteps::UniquePackageName(Folder, TEXT("OA_") + FMantleMetaHumanSteps::AssetTag(Garment->GetName()));
		UPackage* Package = CreatePackage(*PackageName);
		TArray<TStrongObjectPtr<UObject>> KeepAlive;
		const FMantleOutfitAuthoredSource Sources[] = { Source };
		UChaosOutfitAsset* Outfit = Package ? FMantleParametricOutfitAuthoring::Create(Sources, Package, FName(*FPackageName::GetShortName(PackageName)), 1500, KeepAlive, Error) : nullptr;
		if (!Outfit)
		{
			Piece.Error = Error.IsEmpty() ? FString(TEXT("the Outfit could not be made")) : Error;
			continue;
		}
		FAssetRegistryModule::AssetCreated(Outfit);
		if (!GarmentSave(Outfit))
		{
			Piece.Error = FString::Printf(TEXT("the Outfit was made but %s could not be saved"), *PackageName);
			continue;
		}
		Piece.Outfit = Outfit->GetPathName();
		UE_LOG(LogMantleGarmentOutfit, Display, TEXT("MANTLE_GARMENT_OUTFIT made %s from %s: %d vertices, %d far, skin %.2f cm away on average"),
			*Piece.Outfit, *Piece.Garment, Piece.Vertices, Piece.FarVertices, Piece.MeanSkinCm);
	}
	return true;
}

FString FMantleGarmentOutfit::RunForAgent(const FString& BodyPath, const FString& GarmentPaths, const FString& TurnOff, const FString& Confirm)
{
	using namespace MantleGarmentOutfitPrivate;
	auto Refuse = [](const FString& Code, const FString& Message, const FString& ConvertJson = FString())
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetBoolField(TEXT("ok"), false);
		Object->SetStringField(TEXT("refused"), Code);
		Object->SetStringField(TEXT("message"), Message);
		TSharedPtr<FJsonObject> Convert;
		if (!ConvertJson.IsEmpty() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ConvertJson), Convert) && Convert.IsValid())
		{
			Object->SetObjectField(TEXT("convert"), Convert);
		}
		return GarmentWriteJson(Object);
	};
	USkeletalMesh* Body = GarmentLoadMesh(BodyPath);
	if (!Body)
	{
		return Refuse(TEXT("not_found"), FString::Printf(TEXT("%s is not a skeletal mesh."), *BodyPath));
	}
	TArray<USkeletalMesh*> Garments;
	for (const FString& Path : GarmentKeys(GarmentPaths))
	{
		USkeletalMesh* Garment = GarmentLoadMesh(Path);
		if (!Garment)
		{
			return Refuse(TEXT("not_found"), FString::Printf(TEXT("%s is not a skeletal mesh."), *Path));
		}
		Garments.Add(Garment);
	}
	if (Garments.IsEmpty())
	{
		return Refuse(TEXT("no_garment"), TEXT("Pass at least one garment mesh."));
	}
	FMantleGarmentOutfitResult Result;
	FString ConvertJson;
	FString Code;
	FString Message;
	if (!Make(*Body, Garments, TurnOff, Confirm, Result, ConvertJson, Code, Message))
	{
		return Refuse(Code, Message, ConvertJson);
	}
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("ok"), Result.Pieces.ContainsByPredicate([](const FMantleGarmentOutfitPiece& Piece) { return !Piece.Outfit.IsEmpty(); }));
	Object->SetStringField(TEXT("body"), Body->GetPathName());
	Object->SetStringField(TEXT("outputFolder"), Result.OutputFolder);
	Object->SetStringField(TEXT("character"), Result.Character);
	Object->SetStringField(TEXT("authoringBody"), Result.AuthoringBody);
	Object->SetNumberField(TEXT("inputScale"), Result.InputScale);
	TArray<TSharedPtr<FJsonValue>> Offset;
	Offset.Add(MakeShared<FJsonValueNumber>(Result.Offset.X));
	Offset.Add(MakeShared<FJsonValueNumber>(Result.Offset.Y));
	Offset.Add(MakeShared<FJsonValueNumber>(Result.Offset.Z));
	Object->SetArrayField(TEXT("offsetCm"), Offset);
	Object->SetNumberField(TEXT("bodyGapCm"), Result.BodyGapCm);
	TArray<TSharedPtr<FJsonValue>> Outfits;
	for (const FMantleGarmentOutfitPiece& Piece : Result.Pieces)
	{
		TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("garment"), Piece.Garment);
		Row->SetStringField(TEXT("outfit"), Piece.Outfit);
		Row->SetStringField(TEXT("error"), Piece.Error);
		Row->SetNumberField(TEXT("vertices"), Piece.Vertices);
		Row->SetNumberField(TEXT("farVertices"), Piece.FarVertices);
		Row->SetNumberField(TEXT("meanSkinCm"), Piece.MeanSkinCm);
		Outfits.Add(MakeShared<FJsonValueObject>(Row));
	}
	Object->SetArrayField(TEXT("outfits"), Outfits);
	TArray<TSharedPtr<FJsonValue>> Notes;
	for (const FString& Note : Result.Notes)
	{
		Notes.Add(MakeShared<FJsonValueString>(Note));
	}
	Object->SetArrayField(TEXT("notes"), Notes);
	TSharedPtr<FJsonObject> Convert;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ConvertJson), Convert) && Convert.IsValid())
	{
		Object->SetObjectField(TEXT("convert"), Convert);
	}
	return GarmentWriteJson(Object);
}
