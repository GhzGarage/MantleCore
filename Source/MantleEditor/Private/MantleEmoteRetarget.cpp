// Copyright Joshua Eger. All Rights Reserved.
#include "MantleEmoteRetarget.h"

#include "MantleAnimClipWriter.h"
#include "MantleAvatarContract.h"
#include "MantleBuiltInMannequin.h"
#include "MantleCosmeticExport.h"
#include "MantleCosmeticValidation.h"
#include "MantleRuntime.h"

#include "Animation/AnimSequence.h"
#include "Animation/IAnimationSequenceCompiler.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "ReferenceSkeleton.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleEmoteRetargetPrivate
{
	const TCHAR* const BatchClassPath = TEXT("/Script/IKRigEditor.IKRetargetBatchOperation");
	const TCHAR* const BatchFunctionName = TEXT("RunBatchRetarget");
	const TCHAR* const RetargeterClassPath = TEXT("/Script/IKRig.IKRetargeter");
	const TCHAR* const EmoteMetaHumanId = TEXT("metahuman");
	const TCHAR* const EmoteUE5MannequinId = TEXT("ue5mannequin");
	const TCHAR* const EmoteUE4MannequinId = TEXT("ue4mannequin");

	/** FIKRetargetBatchOperationInputs' string and bool inputs, all set on every call. */
	const TCHAR* const BatchStringInputs[] = { TEXT("Search"), TEXT("Replace"), TEXT("Prefix"), TEXT("Suffix"), TEXT("TargetPath") };
	const TCHAR* const BatchBoolInputs[] = { TEXT("bUseSourcePath"), TEXT("bIncludeReferencedAssets"), TEXT("bOverwriteExistingFiles"), TEXT("bRetainAdditiveFlags") };

	/** RunBatchRetarget and the properties of its input struct, found by name. */
	struct FBatchCall
	{
		/** The class default object: the function is static, and a static UFunction runs on it. */
		UObject* Target = nullptr;
		UFunction* Function = nullptr;
		const FStructProperty* Inputs = nullptr;
		const FArrayProperty* Result = nullptr;
		const FArrayProperty* Assets = nullptr;
		const FObjectPropertyBase* SourceMesh = nullptr;
		const FObjectPropertyBase* TargetMesh = nullptr;
		const FObjectPropertyBase* Retargeter = nullptr;
		TMap<FName, const FStrProperty*> Strings;
		TMap<FName, const FBoolProperty*> Bools;
	};

	/** An array of FAssetData (the struct by name and size: CoreUObject's noexport FAssetData). */
	bool IsAssetDataArray(const FArrayProperty* Array)
	{
		const FStructProperty* Inner = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
		return Inner && Inner->Struct && Inner->Struct->GetFName() == FName(TEXT("AssetData"))
			&& Inner->Struct->GetStructureSize() == static_cast<int32>(sizeof(FAssetData));
	}

	bool FindBatchCall(FBatchCall& Out, FString* OutError)
	{
		Out = FBatchCall();
		auto Fail = [OutError](const FString& Why)
		{
			if (OutError)
			{
				*OutError = Why;
			}
			return false;
		};
		UClass* Class = FindObject<UClass>(nullptr, BatchClassPath);
		if (!Class && FModuleManager::Get().ModuleExists(TEXT("IKRigEditor")))
		{
			// IKRigEditor loads at PostEngineInit; a commandlet may not have it yet.
			FModuleManager::Get().LoadModule(TEXT("IKRigEditor"));
			Class = FindObject<UClass>(nullptr, BatchClassPath);
		}
		if (!Class)
		{
			return Fail(FString::Printf(TEXT("%s is not loaded (the IK Rig plugin's editor module)"), BatchClassPath));
		}
		UFunction* Function = Class->FindFunctionByName(FName(BatchFunctionName));
		if (!Function || !Function->HasAnyFunctionFlags(FUNC_Static))
		{
			return Fail(FString::Printf(TEXT("%s has no static %s"), BatchClassPath, BatchFunctionName));
		}
		Out.Inputs = CastField<FStructProperty>(Function->FindPropertyByName(FName(TEXT("Inputs"))));
		Out.Result = CastField<FArrayProperty>(Function->GetReturnProperty());
		if (!Out.Inputs || !Out.Inputs->Struct || !IsAssetDataArray(Out.Result))
		{
			return Fail(FString::Printf(TEXT("%s does not take Inputs and return the assets it made"), BatchFunctionName));
		}
		const UScriptStruct* Struct = Out.Inputs->Struct;
		Out.Assets = FindFProperty<FArrayProperty>(Struct, TEXT("AssetsToRetarget"));
		Out.SourceMesh = FindFProperty<FObjectPropertyBase>(Struct, TEXT("SourceMesh"));
		Out.TargetMesh = FindFProperty<FObjectPropertyBase>(Struct, TEXT("TargetMesh"));
		Out.Retargeter = FindFProperty<FObjectPropertyBase>(Struct, TEXT("IKRetargetAsset"));
		if (!IsAssetDataArray(Out.Assets) || !Out.SourceMesh || !Out.TargetMesh || !Out.Retargeter)
		{
			return Fail(FString::Printf(TEXT("%s has no AssetsToRetarget, SourceMesh, TargetMesh or IKRetargetAsset"), *Struct->GetName()));
		}
		for (const TCHAR* Name : BatchStringInputs)
		{
			const FStrProperty* Property = FindFProperty<FStrProperty>(Struct, Name);
			if (!Property)
			{
				return Fail(FString::Printf(TEXT("%s has no string %s"), *Struct->GetName(), Name));
			}
			Out.Strings.Add(FName(Name), Property);
		}
		for (const TCHAR* Name : BatchBoolInputs)
		{
			const FBoolProperty* Property = FindFProperty<FBoolProperty>(Struct, Name);
			if (!Property)
			{
				return Fail(FString::Printf(TEXT("%s has no bool %s"), *Struct->GetName(), Name));
			}
			Out.Bools.Add(FName(Name), Property);
		}
		Out.Target = Class->GetDefaultObject();
		Out.Function = Function;
		return Out.Target != nullptr;
	}

	/**
	 * One RunBatchRetarget: Sequence alone (nothing it references), from SourceMesh onto TargetMesh through Retargeter,
	 * into Folder under its own name (a numbered one when taken), never overwriting anything. What it made in OutCreated.
	 */
	void RunBatch(const FBatchCall& Call, UAnimSequence* Sequence, USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh, UObject* Retargeter,
		const FString& Folder, TArray<FAssetData>& OutCreated)
	{
		UFunction* Function = Call.Function;
		const int32 Size = FMath::Max<int32>(1, Function->ParmsSize);
		uint8* Params = static_cast<uint8*>(FMemory::Malloc(Size, Function->GetMinAlignment()));
		FMemory::Memzero(Params, Size);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
		}

		void* Inputs = Call.Inputs->ContainerPtrToValuePtr<void>(Params);
		Call.Assets->ContainerPtrToValuePtr<TArray<FAssetData>>(Inputs)->Add(FAssetData(Sequence));
		Call.SourceMesh->SetObjectPropertyValue_InContainer(Inputs, SourceMesh);
		Call.TargetMesh->SetObjectPropertyValue_InContainer(Inputs, TargetMesh);
		Call.Retargeter->SetObjectPropertyValue_InContainer(Inputs, Retargeter);
		for (const TPair<FName, const FStrProperty*>& Pair : Call.Strings)
		{
			Pair.Value->SetPropertyValue_InContainer(Inputs, Pair.Key == FName(TEXT("TargetPath")) ? Folder : FString());
		}
		for (const TPair<FName, const FBoolProperty*>& Pair : Call.Bools)
		{
			// Only bRetainAdditiveFlags: never the source's folder, never what it references, never an overwrite.
			Pair.Value->SetPropertyValue_InContainer(Inputs, Pair.Key == FName(TEXT("bRetainAdditiveFlags")));
		}

		Call.Target->ProcessEvent(Function, Params);
		OutCreated = *Call.Result->ContainerPtrToValuePtr<TArray<FAssetData>>(Params);

		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
		FMemory::Free(Params);
	}

	/**
	 * Drops an asset the retarget made: gone from the asset registry, no longer kept, its package clean so nothing
	 * offers to save it; garbage collection frees it. Only ever an asset under Folder.
	 */
	void DiscardTemporary(UObject* Asset, const FString& Folder)
	{
		UPackage* Package = Asset ? Asset->GetPackage() : nullptr;
		if (!Package || !Package->GetName().StartsWith(Folder + TEXT("/"), ESearchCase::CaseSensitive))
		{
			return;
		}
		if (UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
		{
			// The copy's compression runs asynchronously; let it land before the copy goes (else it lands on garbage).
			UAnimSequence* const Sequences[] = { Sequence };
			UE::Anim::IAnimSequenceCompilingManager::FinishCompilation(MakeArrayView(Sequences));
		}
		FAssetRegistryModule::AssetDeleted(Asset);
		Asset->ClearFlags(RF_Public | RF_Standalone);
		Asset->MarkAsGarbage();
		Package->SetDirtyFlag(false);
	}

	/** Takes Folder out of the Content Browser's tree once nothing in it is left (it was only ever in memory). */
	void RemoveFolderIfEmpty(const FString& Folder)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Left;
		Registry.GetAssetsByPath(FName(*Folder), Left, /*bRecursive*/ true);
		if (Left.Num() == 0)
		{
			Registry.RemovePath(Folder);
		}
	}
}

// Under /Game, a writable mount: IK Rig's batch retarget duplicates through the asset tools, which refuse to create an
// asset under /Temp (a read-only root for them): the first run made nothing there. In memory only, never saved, and
// emptied again after every retarget.
const TCHAR* const FMantleEmoteRetarget::TransientFolder = TEXT("/Game/__MantleEmoteRetarget");

const TCHAR* FMantleEmoteRetarget::RetargeterPathFor(const FString& SkeletonId)
{
	using namespace MantleEmoteRetargetPrivate;
	if (SkeletonId.Equals(EmoteMetaHumanId, ESearchCase::CaseSensitive))
	{
		return TEXT("/MantleCore/Avatars/EmoteRetarget/RTG_Mantle_MetaHuman_to_UEFN.RTG_Mantle_MetaHuman_to_UEFN");
	}
	if (SkeletonId.Equals(EmoteUE5MannequinId, ESearchCase::CaseSensitive))
	{
		return TEXT("/MantleCore/Avatars/EmoteRetarget/RTG_Mantle_UE5Mannequin_to_UEFN.RTG_Mantle_UE5Mannequin_to_UEFN");
	}
	if (SkeletonId.Equals(EmoteUE4MannequinId, ESearchCase::CaseSensitive))
	{
		return TEXT("/MantleCore/Avatars/EmoteRetarget/RTG_Mantle_UE4Mannequin_to_UEFN.RTG_Mantle_UE4Mannequin_to_UEFN");
	}
	return nullptr;
}

FString FMantleEmoteRetarget::IdentifySkeleton(const USkeleton* Skeleton, const FMantleAvatarContract& Contract)
{
	using namespace MantleEmoteRetargetPrivate;
	if (!Skeleton)
	{
		return FString();
	}
	const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
	const TArray<FMantleSkinJointName> Joints = FMantleCosmeticChecks::JointsOf(Reference);
	for (const FString& Id : { Contract.EmoteSkeleton, FString(EmoteMetaHumanId), FString(EmoteUE5MannequinId), FString(EmoteUE4MannequinId) })
	{
		const FMantleContractSkeleton* Fixture = Contract.FindSkeleton(Id);
		FString Error;
		if (Fixture && FMantleCosmeticChecks::MatchSkeleton(Joints, *Fixture, /*bRequireRoot*/ true, Error))
		{
			return Id;
		}
	}
	return FString();
}

USkeletalMesh* FMantleEmoteRetarget::FindSourceMesh(USkeleton* Skeleton)
{
	if (!Skeleton)
	{
		return nullptr;
	}
	// The preview mesh only when it is already loaded: the const overload never loads, so a preview mesh the project does
	// not have (a git-ignored MetaHuman) is not looked for; the asset registry search below finds the meshes it has.
	USkeletalMesh* Preview = static_cast<const USkeleton*>(Skeleton)->GetPreviewMesh();
	if (Preview && Preview->GetSkeleton() == Skeleton)
	{
		return Preview;
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Meshes;
	Registry.GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), Meshes, true);
	const FString SkeletonPath = Skeleton->GetPathName();
	TArray<FAssetData> Candidates;
	for (const FAssetData& Data : Meshes)
	{
		FString Tag;
		Data.GetTagValue(TEXT("Skeleton"), Tag);
		if (FPackageName::ExportTextPathToObjectPath(Tag).Equals(SkeletonPath, ESearchCase::CaseSensitive))
		{
			Candidates.Add(Data);
		}
	}
	Candidates.StableSort([](const FAssetData& A, const FAssetData& B)
	{
		const bool bA = A.AssetName.ToString().Contains(TEXT("Body"));
		const bool bB = B.AssetName.ToString().Contains(TEXT("Body"));
		return bA != bB ? bA : A.PackageName.LexicalLess(B.PackageName);
	});
	for (int32 Index = 0; Index < FMath::Min(8, Candidates.Num()); ++Index)
	{
		USkeletalMesh* Mesh = Cast<USkeletalMesh>(Candidates[Index].GetAsset());
		if (Mesh && Mesh->GetSkeleton() == Skeleton)
		{
			return Mesh;
		}
	}
	return nullptr;
}

bool FMantleEmoteRetarget::RetargetAndSample(UAnimSequence* Sequence, const FString& SkeletonId, USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh,
	const FMantleAvatarContract& Contract, FMantleAnimClipData& OutClip, FString& OutError)
{
	using namespace MantleEmoteRetargetPrivate;
	OutClip = FMantleAnimClipData();
	const TCHAR* const RetargeterPath = RetargeterPathFor(SkeletonId);
	if (!Sequence)
	{
		OutError = TEXT("no animation was chosen.");
		return false;
	}
	if (!RetargeterPath)
	{
		OutError = FString::Printf(TEXT("Mantle retargets emotes from the MetaHuman, UE5 Mannequin and UE4 Mannequin skeletons only, not from '%s'. Retarget %s onto SK_UEFN_Mannequin (right-click it > Retarget Animations) and upload the retargeted copy."),
			*SkeletonId, *Sequence->GetName());
		return false;
	}
	if (!TargetMesh)
	{
		OutError = FString::Printf(TEXT("%s is on the %s skeleton and emotes play on the UEFN mannequin, but this project has none and the Mantle plugin could not make its own to retarget it onto (see the Output Log): reinstall the Mantle plugin, or add SKM_UEFN_Mannequin (from the Game Animation Sample) to the project, and upload again."),
			*Sequence->GetName(), *SkeletonId);
		return false;
	}
	if (!SourceMesh)
	{
		OutError = FString::Printf(TEXT("no skeletal mesh in this project uses %s's skeleton %s, so it cannot be retargeted. Give the skeleton a preview mesh and upload again."),
			*Sequence->GetName(), Sequence->GetSkeleton() ? *Sequence->GetSkeleton()->GetName() : TEXT("(none)"));
		return false;
	}
	if (SourceMesh->GetSkeleton() != Sequence->GetSkeleton() || SourceMesh == TargetMesh)
	{
		OutError = FString::Printf(TEXT("%s is not on %s's skeleton, so it cannot be retargeted from."), *SourceMesh->GetName(), *Sequence->GetName());
		return false;
	}
	// The plugin's built-in mannequin is made from the rig every emote retargeter targets, so it has every bone they
	// drive; it lacks only the fixture's prop bones, which then get no track and stay in their reference pose.
	const FMantleContractSkeleton* Uefn = Contract.FindSkeleton(Contract.EmoteSkeleton);
	FString MissingBone;
	if (!FMantleBuiltInMannequin::Is(TargetMesh) && (!Uefn || !FMantleCosmeticExport::HasFixtureBones(TargetMesh->GetSkeleton(), *Uefn, &MissingBone)))
	{
		OutError = FString::Printf(TEXT("%s is not a UEFN mannequin (its skeleton has no bone %s), so emotes cannot be retargeted onto it."),
			*TargetMesh->GetName(), Uefn ? *MissingBone : TEXT("of the uefn fixture"));
		return false;
	}

	const FSoftObjectPath Path(RetargeterPath);
	UObject* Retargeter = FPackageName::DoesPackageExist(Path.GetLongPackageName()) ? Path.TryLoad() : nullptr;
	const UClass* RetargeterClass = FindObject<UClass>(nullptr, RetargeterClassPath);
	if (!Retargeter || !RetargeterClass || !Retargeter->IsA(RetargeterClass))
	{
		OutError = FString::Printf(TEXT("the Mantle plugin's retargeter %s is missing or not an IK retargeter: reinstall the Mantle plugin."), RetargeterPath);
		return false;
	}
	const double Frame = Contract.EmoteFrameRate > 0.0f ? 1.0 / Contract.EmoteFrameRate : 1.0 / 30.0;
	return RetargetThrough(Sequence, SourceMesh, TargetMesh, Retargeter, Frame,
		[&Contract, &OutClip, Sequence](UAnimSequence& Retargeted, FString& OutUseError)
		{
			FString SampleError;
			if (!FMantleAnimClipWriter::Sample(&Retargeted, Contract, OutClip, SampleError))
			{
				OutUseError = FString::Printf(TEXT("the retargeted %s could not be read: %s"), *Sequence->GetName(), *SampleError);
				return false;
			}
			return true;
		}, OutError);
}

bool FMantleEmoteRetarget::RetargetThrough(UAnimSequence* Sequence, USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh, UObject* Retargeter,
	const double LengthTolerance, TFunctionRef<bool(UAnimSequence& Retargeted, FString& OutUseError)> Use, FString& OutError)
{
	using namespace MantleEmoteRetargetPrivate;
	if (!Sequence || !SourceMesh || !TargetMesh || !Retargeter)
	{
		OutError = TEXT("the retarget needs a clip, a mesh to play it on, a mesh to retarget it onto and a retargeter.");
		return false;
	}
	const FString RetargeterPath = Retargeter->GetPathName();
	FBatchCall Call;
	FString CallError;
	if (!FindBatchCall(Call, &CallError))
	{
		OutError = FString::Printf(TEXT("IK Rig's batch retarget cannot be called (%s): enable the IK Rig plugin and restart the editor."), *CallError);
		return false;
	}
	if (!Call.Retargeter->PropertyClass || !Retargeter->IsA(Call.Retargeter->PropertyClass))
	{
		OutError = FString::Printf(TEXT("%s is not the retargeter class IK Rig's batch retarget takes."), *RetargeterPath);
		return false;
	}

	TArray<FAssetData> Created;
	RunBatch(Call, Sequence, SourceMesh, TargetMesh, Retargeter, TransientFolder, Created);

	// Whatever it made is dropped, whatever happens next.
	TArray<UObject*> Made;
	UAnimSequence* Retargeted = nullptr;
	for (const FAssetData& Data : Created)
	{
		if (UObject* Asset = Data.FastGetAsset(/*bLoad*/ false))
		{
			Made.Add(Asset);
			Retargeted = Retargeted ? Retargeted : Cast<UAnimSequence>(Asset);
		}
	}
	const FString Folder(TransientFolder);
	ON_SCOPE_EXIT
	{
		for (UObject* Asset : Made)
		{
			MantleEmoteRetargetPrivate::DiscardTemporary(Asset, Folder);
		}
		MantleEmoteRetargetPrivate::RemoveFolderIfEmpty(Folder);
	};
	if (Made.Num() != 1 || !Retargeted || !Retargeted->GetPackage()->GetName().StartsWith(Folder + TEXT("/"), ESearchCase::CaseSensitive))
	{
		OutError = FString::Printf(TEXT("IK Rig's batch retarget made %d assets from %s (played on %s, onto %s, through %s), not one animation in %s: see the Output Log."),
			Created.Num(), *Sequence->GetName(), *SourceMesh->GetPathName(), *TargetMesh->GetPathName(), *RetargeterPath, TransientFolder);
		return false;
	}
	if (Retargeted->GetSkeleton() != TargetMesh->GetSkeleton())
	{
		OutError = FString::Printf(TEXT("the retargeted %s is on %s, not on %s's skeleton."), *Sequence->GetName(),
			Retargeted->GetSkeleton() ? *Retargeted->GetSkeleton()->GetName() : TEXT("no skeleton"), *TargetMesh->GetName());
		return false;
	}
	const double SourceLength = Sequence->GetPlayLength();
	const double RetargetedLength = Retargeted->GetPlayLength();
	if (FMath::Abs(SourceLength - RetargetedLength) > LengthTolerance + 1e-3)
	{
		OutError = FString::Printf(TEXT("the retargeted %s is %.3f s long; the clip is %.3f s."), *Sequence->GetName(), RetargetedLength, SourceLength);
		return false;
	}
	return Use(*Retargeted, OutError);
}

USkeletalMesh* FMantleEmoteRetarget::ChooseSourceMesh(UAnimSequence* Sequence, USkeletalMesh* PickedBody, FString* OutFrom)
{
	USkeleton* Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
	auto Say = [OutFrom](const TCHAR* From)
	{
		if (OutFrom)
		{
			*OutFrom = From;
		}
	};
	Say(TEXT("none"));
	if (!Skeleton)
	{
		return nullptr;
	}
	// #584: the body the clip was animated on, when it says: its own preview mesh (loaded if it has to be; a mesh the
	// project does not have loads as nothing and is passed over).
	if (USkeletalMesh* Preview = Sequence->GetPreviewMesh(/*bFindIfNotSet*/ false))
	{
		if (Preview->GetSkeleton() == Skeleton)
		{
			Say(TEXT("the animation's preview mesh"));
			return Preview;
		}
	}
	// Then the body the creator picked in the panel, when it is on the clip's skeleton.
	if (PickedBody && PickedBody->GetSkeleton() == Skeleton)
	{
		Say(TEXT("the body picked in the panel"));
		return PickedBody;
	}
	USkeletalMesh* Found = FindSourceMesh(Skeleton);
	if (Found)
	{
		Say(TEXT("a mesh on the clip's skeleton"));
	}
	return Found;
}

bool FMantleEmoteRetarget::Sample(UAnimSequence* Sequence, const FString& SkeletonId, const FMantleAvatarContract& Contract, FMantleAnimClipData& OutClip,
	FString& OutError, FString& OutNote, USkeletalMesh* PickedBody)
{
	FString SourceFrom;
	USkeletalMesh* Source = ChooseSourceMesh(Sequence, PickedBody, &SourceFrom);
	USkeletalMesh* Target = FMantleBuiltInMannequin::UEFNMannequin(Contract);
	const double Start = FPlatformTime::Seconds();
	if (!RetargetAndSample(Sequence, SkeletonId, Source, Target, Contract, OutClip, OutError))
	{
		UE_LOG(LogMantle, Warning, TEXT("MANTLE_EMOTE_RETARGET failed %s skeleton=%s: %s"), Sequence ? *Sequence->GetPathName() : TEXT("(none)"), *SkeletonId, *OutError);
		return false;
	}
	const double Seconds = FPlatformTime::Seconds() - Start;
	OutNote = FString::Printf(TEXT("Emote retargeted from the %s skeleton (played on %s, %s) onto %s through %s in %.1f s"), *SkeletonId, *Source->GetName(),
		*SourceFrom, *Target->GetName(), *FPackageName::ObjectPathToObjectName(FString(RetargeterPathFor(SkeletonId))), Seconds);
	UE_LOG(LogMantle, Log, TEXT("MANTLE_EMOTE_RETARGET %s skeleton=%s source=%s (%s) target=%s frames=%d tracks=%d seconds=%.2f"), *Sequence->GetPathName(),
		*SkeletonId, *Source->GetPathName(), *SourceFrom, *Target->GetPathName(), OutClip.FrameCount, OutClip.Tracks.Num(), Seconds);
	return true;
}
