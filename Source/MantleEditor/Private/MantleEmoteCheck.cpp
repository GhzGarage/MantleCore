// Copyright Joshua Eger. All Rights Reserved.
//
// Maintainer command: Mantle.Dev.CheckEmote <AnimSequence> <Directory> [Label] [female|male|<SkeletalMesh>] [size]
// See FMantleEmoteCheck (MantleEmoteCheck.h).
#include "MantleEmoteCheck.h"

#include "MantleAnimClip.h"
#include "MantleAvatarContract.h"
#include "MantleBuiltInMannequin.h"
#include "MantleClipAnimInstance.h"
#include "MantleCosmeticExport.h"
#include "MantleCosmeticPackage.h"
#include "MantleCosmeticPreview.h"
#include "MantleCosmeticPreviewRetarget.h"
#include "MantleCosmeticThumbnail.h"
#include "MantleEditorActions.h"
#include "MantleEmoteRetarget.h"
#include "MantleFitBodies.h"
#include "MantlePng.h"

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PreviewScene.h"
#include "ReferenceSkeleton.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogMantleCheckEmote, Log, All);

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleEmoteCheckPrivate
{
	/** Standard bodies made for the sheet, kept for the editor's life (made once: Creator takes a while). Never freed. */
	TMap<FString, FMantleFitBody*>& MadeBodies()
	{
		static TMap<FString, FMantleFitBody*> Bodies;
		return Bodies;
	}

	/** Body: "" or "male" (m_med_nrw), "female" (f_med_nrw), else a skeletal mesh path. OutFace may stay null. */
	bool ResolveBody(const FString& Body, USkeletalMesh*& OutBody, USkeletalMesh*& OutFace, FString& OutDescribe, FString& OutError)
	{
		OutBody = nullptr;
		OutFace = nullptr;
		const FString Choice = Body.TrimStartAndEnd();
		if (Choice.IsEmpty() || Choice.Equals(TEXT("male"), ESearchCase::IgnoreCase) || Choice.Equals(TEXT("female"), ESearchCase::IgnoreCase))
		{
			const FString Id = Choice.Equals(TEXT("female"), ESearchCase::IgnoreCase) ? TEXT("f_med_nrw") : TEXT("m_med_nrw");
			FMantleFitBody*& Made = MadeBodies().FindOrAdd(Id);
			if (!Made || !Made->BodyMesh.IsValid())
			{
				EMetaHumanBodyType Type;
				if (!FMantleFitBodies::ParseBodyType(Id, Type))
				{
					OutError = FString::Printf(TEXT("%s is not a standard MetaHuman body type"), *Id);
					return false;
				}
				Made = Made ? Made : new FMantleFitBody();
				if (!FMantleFitBodies::Make(Id, Type, *Made, OutError))
				{
					return false;
				}
			}
			OutBody = Made->BodyMesh.Get();
			OutFace = Made->FaceMesh.Get();
			OutDescribe = FString::Printf(TEXT("Epic's standard MetaHuman body %s (made in memory)"), *Id);
			return OutBody != nullptr;
		}
		FString Path = Choice;
		if (!FPackageName::GetShortName(Path).Contains(TEXT(".")))
		{
			Path += TEXT(".") + FPackageName::GetShortName(Path);
		}
		OutBody = LoadObject<USkeletalMesh>(nullptr, *Path);
		if (!OutBody)
		{
			OutError = FString::Printf(TEXT("there is no skeletal mesh at %s"), *Choice);
			return false;
		}
		OutDescribe = OutBody->GetPathName();
		return true;
	}

	/** Clip without its root track: players' clients keep the root where the character's graph put it (root motion is ignored). */
	TSharedRef<const FMantleAnimClip> WithoutRoot(const FMantleAnimClip& Clip, const FName Root)
	{
		TSharedRef<FMantleAnimClip> Copy = MakeShared<FMantleAnimClip>(Clip);
		Copy->Tracks.RemoveAll([Root](const FMantleAnimClipTrack& Track) { return Track.Bone == Root; });
		return Copy;
	}

	/** The bone's local transform at Time: its track's keys, else its reference pose. */
	FTransform LocalAt(const FMantleAnimClip& Clip, const int32 Track, const FTransform& Reference, const float Time)
	{
		FTransform Out = Reference;
		FQuat4f Rotation;
		FVector3f Translation;
		bool bHasTranslation = false;
		if (Track != INDEX_NONE && Clip.SampleTrack(Track, Time, Rotation, Translation, bHasTranslation))
		{
			Out.SetRotation(FQuat(Rotation));
			if (bHasTranslation)
			{
				Out.SetTranslation(FVector(Translation));
			}
		}
		return Out;
	}

	void Evaluate(USkeletalMeshComponent* Component)
	{
		if (!Component || !Component->GetSkeletalMeshAsset())
		{
			return;
		}
		Component->TickAnimation(0.0f, false);
		// No tick function: the pose is evaluated here, on the game thread.
		Component->RefreshBoneTransforms(nullptr);
		Component->UpdateComponentToWorld();
		Component->UpdateBounds();
		Component->MarkRenderTransformDirty();
		Component->MarkRenderDynamicDataDirty();
	}

	/** A standing body's whole height from the floor, with room around it to move, the same at every moment. */
	FBox StandingFrame(const USkeletalMesh* Mesh)
	{
		const FBox Bounds = Mesh->GetImportedBounds().GetBox();
		const FVector Center = Bounds.GetCenter();
		const double Top = FMath::Max(Bounds.Max.Z, 50.0);
		UE_LOG(LogMantleCheckEmote, Log, TEXT("MANTLE_CHECK_EMOTE frame of %s from bounds %s"), *Mesh->GetName(), *Bounds.ToString());
		return FBox(FVector(Center.X - 60.0, Center.Y - 60.0, 0.0), FVector(Center.X + 60.0, Center.Y + 60.0, Top + 15.0));
	}

	/** "0.00 s, 0.37 s, ...". */
	FString DescribeTimes(const TArray<float>& Times)
	{
		TArray<FString> Out;
		for (const float Time : Times)
		{
			Out.Add(FString::Printf(TEXT("%.2f s"), Time));
		}
		return FString::Join(Out, TEXT(", "));
	}

	/**
	 * Players' clients' pose at Time: the hidden UEFN driver plays Clip held at Time (the client's emote layer, the root left
	 * out), and the body (Components[0]) copies it through Retargeter; its face (Components[1]) follows by leader pose.
	 */
	void PoseOnBody(FPreviewScene& Scene, TConstArrayView<USkeletalMeshComponent*> Components, USkeletalMesh* Driver, UObject* Retargeter,
		const TSharedRef<const FMantleAnimClip>& Clip, const float Time)
	{
		USkeletalMeshComponent* BodyComponent = Components.IsValidIndex(0) ? Components[0] : nullptr;
		if (!BodyComponent)
		{
			return;
		}
		USkeletalMeshComponent* DriverComponent = NewObject<USkeletalMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		DriverComponent->SetVisibility(false);
		DriverComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		DriverComponent->SetSkeletalMeshAsset(Driver);
		Scene.AddComponent(DriverComponent, FTransform::Identity);
		DriverComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		DriverComponent->SetAnimInstanceClass(UMantleClipAnimInstance::StaticClass());
		if (UMantleClipAnimInstance* ClipInstance = Cast<UMantleClipAnimInstance>(DriverComponent->GetAnimInstance()))
		{
			FMantleClipPlayback Playback;
			Playback.bLoop = false;
			Playback.PlayRate = 0.0;
			Playback.StartOffset = Time;
			ClipInstance->SetApplyRootTranslation(false);
			ClipInstance->SetTimeSource([] { return 0.0; });
			ClipInstance->SetClip(Clip, Playback);
		}
		BodyComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		BodyComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		BodyComponent->SetAnimInstanceClass(UMantlePreviewRetargetAnimInstance::StaticClass());
		if (UMantlePreviewRetargetAnimInstance* Instance = Cast<UMantlePreviewRetargetAnimInstance>(BodyComponent->GetAnimInstance()))
		{
			Instance->Retargeter = Retargeter;
			Instance->Source = DriverComponent;
		}
		USkeletalMeshComponent* FaceComponent = Components.IsValidIndex(1) ? Components[1] : nullptr;
		if (FaceComponent)
		{
			FaceComponent->SetLeaderPoseComponent(BodyComponent, /*bForceUpdate*/ true);
		}
		// Twice: the retarget node reads the driver's pose before it evaluates, and sets itself up on its first update.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Evaluate(DriverComponent);
			Evaluate(BodyComponent);
		}
		if (FaceComponent)
		{
			FaceComponent->SetLeaderPoseComponent(BodyComponent, /*bForceUpdate*/ true);
			FaceComponent->UpdateBounds();
			FaceComponent->MarkRenderDynamicDataDirty();
		}
		if (UWorld* World = Scene.GetWorld())
		{
			World->SendAllEndOfFrameUpdates();
		}
	}

	/** The original clip on its own mesh (Components[0]) at Time, as the animation editor plays it. */
	void PoseSource(FPreviewScene& Scene, TConstArrayView<USkeletalMeshComponent*> Components, UAnimSequence* Sequence, const float Time)
	{
		USkeletalMeshComponent* Component = Components.IsValidIndex(0) ? Components[0] : nullptr;
		if (!Component)
		{
			return;
		}
		Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Component->SetAnimation(Sequence);
		Component->SetPosition(FMath::Min(Time, Sequence->GetPlayLength()), /*bFireNotifies*/ false);
		Evaluate(Component);
		if (UWorld* World = Scene.GetWorld())
		{
			World->SendAllEndOfFrameUpdates();
		}
	}

	/** A close-up of Bone's place on the posed mesh (Components[0]), 20 cm each way. */
	FBox HandFrame(TConstArrayView<USkeletalMeshComponent*> Components, const FName Bone)
	{
		const USkeletalMeshComponent* Component = Components.IsValidIndex(0) ? Components[0] : nullptr;
		const FVector Center = Component && Component->GetBoneIndex(Bone) != INDEX_NONE ? Component->GetBoneLocation(Bone) : FVector::ZeroVector;
		return FBox(Center - FVector(20.0), Center + FVector(20.0));
	}

	/** A sequence's bone keys, read once, sampled at any time as the emote writer does (slerp and lerp between keys). */
	class FSequenceSampler
	{
	public:
		FSequenceSampler(const UAnimSequence* Sequence, const FReferenceSkeleton& InSkeleton)
			: Skeleton(InSkeleton)
		{
			const IAnimationDataModel* Model = Sequence ? Sequence->GetDataModel() : nullptr;
			Keys.SetNum(Skeleton.GetRawBoneNum());
			if (!Model)
			{
				return;
			}
			Rate = Model->GetFrameRate();
			for (int32 Bone = 0; Bone < Skeleton.GetRawBoneNum(); ++Bone)
			{
				if (Model->IsValidBoneTrackName(Skeleton.GetBoneName(Bone)))
				{
					Model->GetBoneTrackTransforms(Skeleton.GetBoneName(Bone), Keys[Bone]);
				}
			}
		}

		TArray<FTransform> ComponentPoseAt(const double Time) const
		{
			const FFrameTime At = Rate.AsFrameTime(Time);
			return FMantleEmoteCheck::ComponentPose(Skeleton, [this, &At](const int32 Bone)
			{
				const TArray<FTransform>& Track = Keys[Bone];
				if (Track.Num() == 0)
				{
					return Skeleton.GetRawRefBonePose()[Bone];
				}
				const int32 Key0 = FMath::Clamp(At.GetFrame().Value, 0, Track.Num() - 1);
				const int32 Key1 = FMath::Min(Key0 + 1, Track.Num() - 1);
				const double Alpha = Key0 == Key1 ? 0.0 : FMath::Clamp(static_cast<double>(At.GetSubFrame()), 0.0, 1.0);
				return FTransform(FQuat::Slerp(Track[Key0].GetRotation(), Track[Key1].GetRotation(), Alpha).GetNormalized(),
					FMath::Lerp(Track[Key0].GetTranslation(), Track[Key1].GetTranslation(), Alpha));
			});
		}

	private:
		const FReferenceSkeleton& Skeleton;
		FFrameRate Rate;
		TArray<TArray<FTransform>> Keys;
	};

	/** The MAN1 clip's pose at Time on Skeleton (the driver), the root held at its reference as players' clients hold it. */
	TArray<FTransform> ClipComponentPose(const FMantleAnimClip& Clip, const FReferenceSkeleton& Skeleton, const float Time)
	{
		TMap<FName, int32> TrackOf;
		for (int32 Track = 0; Track < Clip.Tracks.Num(); ++Track)
		{
			TrackOf.Add(Clip.Tracks[Track].Bone, Track);
		}
		return FMantleEmoteCheck::ComponentPose(Skeleton, [&](const int32 Bone)
		{
			const int32* Track = Bone > 0 ? TrackOf.Find(Skeleton.GetBoneName(Bone)) : nullptr;
			return LocalAt(Clip, Track ? *Track : INDEX_NONE, Skeleton.GetRawRefBonePose()[Bone], Time);
		});
	}

	/** One capture of Subject per time (SetPose sets its Pose), tiled into File. Problems: one clause per moment that could not be drawn. */
	bool DrawSheet(FMantleThumbnailSubject Subject, const TArray<float>& Times, TFunctionRef<void(FMantleThumbnailSubject&, float)> SetPose, const int32 TileSize,
		const FString& File, FString& OutProblems)
	{
		TArray<TArray<FColor>> Tiles;
		for (const float Time : Times)
		{
			SetPose(Subject, Time);
			TArray<FColor> Pixels;
			FString Error;
			if (!FMantleCosmeticThumbnail::Capture(Subject, TileSize, Pixels, Error))
			{
				OutProblems += FString::Printf(TEXT("; the moment at %.2f s could not be drawn (%s)"), Time, *Error);
			}
			Tiles.Add(MoveTemp(Pixels));
			// Each capture's scene, components and render target go now, not at the next idle collection: a run of
			// sheets otherwise grows the GPU's memory until the device is lost.
			Subject.Pose = nullptr;
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
		int32 Width = 0;
		int32 Height = 0;
		const TArray<FColor> Sheet = FMantleEmoteCheck::Tile(Tiles, TileSize, FMantleEmoteCheck::SheetColumns, Width, Height);
		return FFileHelper::SaveArrayToFile(FMantlePng::EncodeRGBA8(Width, Height, Sheet), *File);
	}
}

TArray<float> FMantleEmoteCheck::Moments(const float Duration, const int32 Count)
{
	TArray<float> Out;
	if (!(Duration > 0.0f) || Count <= 1)
	{
		Out.Add(0.0f);
		return Out;
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Out.Add(Duration * static_cast<float>(Index) / static_cast<float>(Count - 1));
	}
	return Out;
}

FMantleEmoteMotion FMantleEmoteCheck::Measure(const FMantleAnimClip& Clip, const FReferenceSkeleton& Skeleton)
{
	using namespace MantleEmoteCheckPrivate;
	FMantleEmoteMotion Out;
	if (Skeleton.GetRawBoneNum() == 0 || Clip.FrameCount < 1 || !(Clip.FrameRate > 0.0f))
	{
		return Out;
	}
	const FName RootName = Skeleton.GetBoneName(0);
	const int32 PelvisBone = Skeleton.FindRawBoneIndex(FName(TEXT("pelvis")));
	const FTransform RootReference = Skeleton.GetRawRefBonePose()[0];
	const FTransform PelvisReference = PelvisBone != INDEX_NONE ? Skeleton.GetRawRefBonePose()[PelvisBone] : FTransform::Identity;
	int32 RootTrack = INDEX_NONE;
	int32 PelvisTrack = INDEX_NONE;
	for (int32 Track = 0; Track < Clip.Tracks.Num(); ++Track)
	{
		RootTrack = Clip.Tracks[Track].Bone == RootName ? Track : RootTrack;
		PelvisTrack = Clip.Tracks[Track].Bone == FName(TEXT("pelvis")) ? Track : PelvisTrack;
	}
	// The pelvis in component space with the root held at its reference (as the client holds it): pelvis local, then the
	// bones between the pelvis and the root at their reference (the UEFN skeleton has none).
	FTransform PelvisToRoot = FTransform::Identity;
	for (int32 Bone = PelvisBone != INDEX_NONE ? Skeleton.GetRawParentIndex(PelvisBone) : INDEX_NONE; Bone > 0; Bone = Skeleton.GetRawParentIndex(Bone))
	{
		PelvisToRoot = PelvisToRoot * Skeleton.GetRawRefBonePose()[Bone];
	}
	const FTransform Root0 = LocalAt(Clip, RootTrack, RootReference, 0.0f);
	const FVector Pelvis0 = (LocalAt(Clip, PelvisTrack, PelvisReference, 0.0f) * PelvisToRoot * RootReference).GetLocation();
	for (int32 Frame = 0; Frame < Clip.FrameCount; ++Frame)
	{
		const float Time = static_cast<float>(Frame) / Clip.FrameRate;
		if (RootTrack != INDEX_NONE)
		{
			const FTransform Root = LocalAt(Clip, RootTrack, RootReference, Time);
			Out.RootTravelCm = FMath::Max(Out.RootTravelCm, FVector::Dist(Root.GetTranslation(), Root0.GetTranslation()));
			Out.RootTurnDegrees = FMath::Max(Out.RootTurnDegrees, FMath::RadiansToDegrees(Root.GetRotation().AngularDistance(Root0.GetRotation())));
		}
		if (PelvisBone != INDEX_NONE)
		{
			const FVector Pelvis = (LocalAt(Clip, PelvisTrack, PelvisReference, Time) * PelvisToRoot * RootReference).GetLocation();
			Out.PelvisDriftCm = FMath::Max(Out.PelvisDriftCm, FVector::Dist2D(Pelvis, Pelvis0));
		}
	}
	return Out;
}

TArray<FTransform> FMantleEmoteCheck::ComponentPose(const FReferenceSkeleton& Skeleton, TFunctionRef<FTransform(int32 Bone)> LocalOf)
{
	TArray<FTransform> Out;
	Out.Reserve(Skeleton.GetRawBoneNum());
	for (int32 Bone = 0; Bone < Skeleton.GetRawBoneNum(); ++Bone)
	{
		const int32 Parent = Skeleton.GetRawParentIndex(Bone);
		const FTransform Local = LocalOf(Bone);
		Out.Add(Parent == INDEX_NONE ? Local : Local * Out[Parent]);
	}
	return Out;
}

FMantleHandMatch FMantleEmoteCheck::CompareHands(TConstArrayView<TArray<FTransform>> SourcePoses, const FReferenceSkeleton& Source,
	TConstArrayView<TArray<FTransform>> ConvertedPoses, const FReferenceSkeleton& Converted)
{
	FMantleHandMatch Out;
	const int32 Frames = FMath::Min(SourcePoses.Num(), ConvertedPoses.Num());
	auto Bone = [](const FReferenceSkeleton& Skeleton, const FString& Name) { return Skeleton.FindRawBoneIndex(FName(*Name)); };

	// Each finger's first two joints: the bend between hand-to-01 and 01-to-02, and between 01-to-02 and 02-to-03. Angles
	// between segments, so neither skeleton's bone axes matter.
	struct FJoint
	{
		int32 SourceBones[3];
		int32 ConvertedBones[3];
		FName Name;
	};
	TArray<FJoint> Joints;
	for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
	{
		for (const TCHAR* Finger : { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
		{
			const FString Names[4] = { FString::Printf(TEXT("hand_%s"), Side), FString::Printf(TEXT("%s_01_%s"), Finger, Side),
				FString::Printf(TEXT("%s_02_%s"), Finger, Side), FString::Printf(TEXT("%s_03_%s"), Finger, Side) };
			for (int32 First = 0; First < 2; ++First)
			{
				FJoint Joint;
				bool bAll = true;
				for (int32 Index = 0; Index < 3; ++Index)
				{
					Joint.SourceBones[Index] = Bone(Source, Names[First + Index]);
					Joint.ConvertedBones[Index] = Bone(Converted, Names[First + Index]);
					bAll = bAll && Joint.SourceBones[Index] != INDEX_NONE && Joint.ConvertedBones[Index] != INDEX_NONE;
				}
				Joint.Name = FName(*Names[First + 1]);
				if (bAll)
				{
					Joints.Add(Joint);
				}
			}
		}
	}
	auto BendAt = [](const TArray<FTransform>& Pose, const int32 (&Bones)[3])
	{
		const FVector A = (Pose[Bones[1]].GetLocation() - Pose[Bones[0]].GetLocation()).GetSafeNormal();
		const FVector B = (Pose[Bones[2]].GetLocation() - Pose[Bones[1]].GetLocation()).GetSafeNormal();
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(A | B, -1.0, 1.0)));
	};

	// Wrists from the chest, in the chest's frame, and their distance from the head; the source's scaled to the converted
	// body's head height. The chest is the clavicles' parent (spine_03 on the UE4 Mannequin, spine_05 on the others).
	const int32 SourceHead = Bone(Source, TEXT("head"));
	const int32 ConvertedHead = Bone(Converted, TEXT("head"));
	auto Chest = [&Bone](const FReferenceSkeleton& Skeleton)
	{
		const int32 Clavicle = Bone(Skeleton, TEXT("clavicle_l"));
		return Clavicle == INDEX_NONE ? INDEX_NONE : Skeleton.GetRawParentIndex(Clavicle);
	};
	const int32 SourceChest = Chest(Source);
	const int32 ConvertedChest = Chest(Converted);
	double Scale = 1.0;
	if (SourceHead != INDEX_NONE && ConvertedHead != INDEX_NONE)
	{
		const double SourceHeight = FAnimationRuntime::GetComponentSpaceTransformRefPose(Source, SourceHead).GetLocation().Z;
		const double ConvertedHeight = FAnimationRuntime::GetComponentSpaceTransformRefPose(Converted, ConvertedHead).GetLocation().Z;
		Scale = SourceHeight > 1.0 ? ConvertedHeight / SourceHeight : 1.0;
	}
	TArray<TPair<int32, int32>> Wrists;
	for (const TCHAR* Hand : { TEXT("hand_l"), TEXT("hand_r") })
	{
		if (Bone(Source, Hand) != INDEX_NONE && Bone(Converted, Hand) != INDEX_NONE && SourceHead != INDEX_NONE && ConvertedHead != INDEX_NONE
			&& SourceChest != INDEX_NONE && ConvertedChest != INDEX_NONE)
		{
			Wrists.Emplace(Bone(Source, Hand), Bone(Converted, Hand));
		}
	}

	TArray<double> PerJoint;
	PerJoint.Init(0.0, Joints.Num());
	double Bias = 0.0;
	double Error = 0.0;
	double WristError = 0.0;
	double WristCloser = 0.0;
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		const TArray<FTransform>& S = SourcePoses[Frame];
		const TArray<FTransform>& C = ConvertedPoses[Frame];
		double FrameBias = 0.0;
		for (int32 Index = 0; Index < Joints.Num(); ++Index)
		{
			const double Difference = BendAt(C, Joints[Index].ConvertedBones) - BendAt(S, Joints[Index].SourceBones);
			FrameBias += Difference;
			Error += FMath::Abs(Difference);
			PerJoint[Index] += FMath::Abs(Difference);
		}
		Bias += FrameBias;
		Out.FrameBendBiasDegrees.Add(FrameBias / FMath::Max(1, Joints.Num()));
		for (const TPair<int32, int32>& Wrist : Wrists)
		{
			const FVector SourceVector = S[SourceChest].GetRotation().UnrotateVector(S[Wrist.Key].GetLocation() - S[SourceChest].GetLocation()) * Scale;
			const FVector ConvertedVector = C[ConvertedChest].GetRotation().UnrotateVector(C[Wrist.Value].GetLocation() - C[ConvertedChest].GetLocation());
			WristError += FVector::Dist(SourceVector, ConvertedVector);
			WristCloser += FVector::Dist(C[Wrist.Value].GetLocation(), C[ConvertedHead].GetLocation())
				- FVector::Dist(S[Wrist.Key].GetLocation(), S[SourceHead].GetLocation()) * Scale;
		}
	}
	Out.Frames = Frames;
	Out.Joints = Joints.Num();
	const double Samples = static_cast<double>(FMath::Max(1, Frames * Joints.Num()));
	Out.BendBiasDegrees = Bias / Samples;
	Out.BendErrorDegrees = Error / Samples;
	for (int32 Index = 0; Index < Joints.Num(); ++Index)
	{
		const double Mean = PerJoint[Index] / FMath::Max(1, Frames);
		if (Mean > Out.BendWorstDegrees)
		{
			Out.BendWorstDegrees = Mean;
			Out.BendWorstJoint = Joints[Index].Name;
		}
	}
	const double WristSamples = static_cast<double>(FMath::Max(1, Frames * Wrists.Num()));
	Out.WristErrorCm = WristError / WristSamples;
	Out.WristCloserCm = WristCloser / WristSamples;
	return Out;
}

TArray<FColor> FMantleEmoteCheck::Tile(const TArray<TArray<FColor>>& Tiles, const int32 TileSize, const int32 Columns, int32& OutWidth, int32& OutHeight)
{
	const int32 Across = FMath::Max(1, FMath::Min(Columns, Tiles.Num()));
	const int32 Down = FMath::Max(1, FMath::DivideAndRoundUp(Tiles.Num(), Across));
	OutWidth = Across * TileSize;
	OutHeight = Down * TileSize;
	TArray<FColor> Out;
	Out.Init(FColor::Black, OutWidth * OutHeight);
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		if (Tiles[Index].Num() != TileSize * TileSize)
		{
			continue;
		}
		const int32 Left = (Index % Across) * TileSize;
		const int32 Top = (Index / Across) * TileSize;
		for (int32 Row = 0; Row < TileSize; ++Row)
		{
			FMemory::Memcpy(&Out[(Top + Row) * OutWidth + Left], &Tiles[Index][Row * TileSize], TileSize * sizeof(FColor));
		}
	}
	return Out;
}

bool FMantleEmoteCheck::Run(UAnimSequence* Sequence, const FString& Directory, const FString& Label, const FString& Body, const int32 TileSize, FString& OutError)
{
	using namespace MantleEmoteCheckPrivate;
	const FMantleAvatarContract* Contract = FMantleAvatarContract::Get(&OutError);
	if (!Sequence || !Contract)
	{
		OutError = Sequence ? OutError : FString(TEXT("no animation sequence"));
		return false;
	}
	IFileManager::Get().MakeDirectory(*Directory, /*Tree*/ true);
	TArray<FString> Lines;
	auto Say = [&Lines](const FString& Line)
	{
		Lines.Add(Line);
		UE_LOG(LogMantleCheckEmote, Display, TEXT("MANTLE_CHECK_EMOTE %s"), *Line);
	};
	Say(FString::Printf(TEXT("Emote check of %s (%.2f s at %.0f fps, skeleton %s)"), *Sequence->GetPathName(), Sequence->GetPlayLength(),
		Sequence->GetSamplingFrameRate().AsDecimal(), Sequence->GetSkeleton() ? *Sequence->GetSkeleton()->GetPathName() : TEXT("none")));

	// The panel's export, exactly: the draft it makes for an emote, and Build.
	const double Start = FPlatformTime::Seconds();
	FMantleCosmeticDraft Draft = FMantleEditorActions::MakeCosmeticDraft(EMantleCosmeticClass::Emote, Sequence, TEXT("emote"), Label, *Contract, EMantleFitSex::Auto);
	const FMantleCosmeticPackage Package = FMantleCosmeticExport::Build(Draft, *Contract);
	const FMantleCosmeticUploadAsset* ClipAsset = Package.ClipKey.IsEmpty() ? nullptr : Package.FindAsset(Package.ClipKey);
	Say(FString::Printf(TEXT("Result: %s (export took %.1f s)"), Package.HasErrors() ? TEXT("REFUSED") : TEXT("PASS, can upload"), FPlatformTime::Seconds() - Start));
	for (const FMantleCosmeticIssue& Issue : Package.Issues)
	{
		Say(FString::Printf(TEXT("Issue: %s"), *Issue.ToString()));
	}
	for (const FString& Note : Package.Notes)
	{
		Say(FString::Printf(TEXT("Note: %s"), *Note));
	}
	for (const FMantleCosmeticUploadAsset& Asset : Package.Assets)
	{
		Say(FString::Printf(TEXT("File: %s, %s, %d bytes"), *Asset.Key, *Asset.Label, Asset.Bytes.Num()));
	}

	const FString NotesFile = FPaths::Combine(Directory, Label + TEXT("_notes.txt"));
	ON_SCOPE_EXIT
	{
		FFileHelper::SaveStringArrayToFile(Lines, *NotesFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogMantleCheckEmote, Display, TEXT("MANTLE_CHECK_EMOTE done %s"), *NotesFile);
	};
	if (!ClipAsset || Package.HasErrors())
	{
		return true;
	}

	// What players' clients read.
	const FMantleAnimClipReadResult Read = FMantleAnimClipReader::Read(ClipAsset->Bytes);
	if (!Read.bSuccess)
	{
		Say(FString::Printf(TEXT("Client read: FAILED: %s"), *Read.Error));
		return true;
	}
	const FMantleAnimClip& Clip = Read.Clip;
	TArray<FString> Events;
	for (const FMantleAnimClipEvent& Event : Clip.Events)
	{
		Events.Add(FString::Printf(TEXT("%s at %.2f s"), *Event.Name.ToString(), Event.Time));
	}
	Say(FString::Printf(TEXT("Client read: ok, %d frames at %g fps (%.2f s), %d tracks, events: %s"), Clip.FrameCount, Clip.FrameRate, Clip.GetDuration(),
		Clip.Tracks.Num(), Events.IsEmpty() ? TEXT("none") : *FString::Join(Events, TEXT(", "))));

	FString DriverError;
	USkeletalMesh* Driver = FMantleBuiltInMannequin::UEFNMannequin(*Contract, &DriverError);
	if (!Driver)
	{
		Say(FString::Printf(TEXT("Sheet: none, no UEFN mannequin to play the clip on (%s)"), *DriverError));
		return true;
	}
	const FMantleEmoteMotion Motion = Measure(Clip, Driver->GetRefSkeleton());
	Say(FString::Printf(TEXT("Motion: the root travels %.1f cm and turns %.1f degrees (players' clients hold the root still); the pelvis drifts %.1f cm across the floor"),
		Motion.RootTravelCm, Motion.RootTurnDegrees, Motion.PelvisDriftCm));

	// The hands against the source's, frame by frame (mantle #840: fingers that keep the rest hands' difference curl less).
	if (USkeletalMesh* HandsSource = FMantleEmoteRetarget::ChooseSourceMesh(Sequence, nullptr))
	{
		const FSequenceSampler Sampler(Sequence, HandsSource->GetRefSkeleton());
		const FReferenceSkeleton& DriverBones = Driver->GetRefSkeleton();
		TArray<TArray<FTransform>> SourcePoses;
		TArray<TArray<FTransform>> ConvertedPoses;
		for (int32 Frame = 0; Frame < Clip.FrameCount; ++Frame)
		{
			const float Time = Frame / Clip.FrameRate;
			SourcePoses.Add(Sampler.ComponentPoseAt(FMath::Min(static_cast<double>(Time), static_cast<double>(Sequence->GetPlayLength()))));
			ConvertedPoses.Add(ClipComponentPose(Clip, DriverBones, Time));
		}
		const FMantleHandMatch Hands = CompareHands(SourcePoses, HandsSource->GetRefSkeleton(), ConvertedPoses, DriverBones);
		Say(FString::Printf(TEXT("Hands: finger bend %+.1f degrees on average against the source (negative: straighter), %.1f degrees mean error over %d joints and %d frames, worst joint %s %.1f degrees; wrists %.1f cm from the source's place relative to the chest, %+.1f cm farther from the head (negative: closer)"),
			Hands.BendBiasDegrees, Hands.BendErrorDegrees, Hands.Joints, Hands.Frames, *Hands.BendWorstJoint.ToString(), Hands.BendWorstDegrees,
			Hands.WristErrorCm, Hands.WristCloserCm));
		TArray<FString> Rows = { TEXT("frame,bend_bias_degrees") };
		for (int32 Frame = 0; Frame < Hands.FrameBendBiasDegrees.Num(); ++Frame)
		{
			Rows.Add(FString::Printf(TEXT("%d,%.2f"), Frame, Hands.FrameBendBiasDegrees[Frame]));
		}
		FFileHelper::SaveStringArrayToFile(Rows, *FPaths::Combine(Directory, Label + TEXT("_hands.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	// The sheet: the clip on a standard body, posed as the client poses it.
	FString WhyNot;
	if (!FMantleCosmeticThumbnail::CanCapture(&WhyNot))
	{
		Say(FString::Printf(TEXT("Sheet: none, nothing can render (%s)"), *WhyNot));
		return true;
	}
	USkeletalMesh* BodyMesh = nullptr;
	USkeletalMesh* FaceMesh = nullptr;
	FString BodyDescribe;
	FString BodyError;
	FString SkeletonId;
	FMantleAvatarPreviewPlan Plan;
	if (!ResolveBody(Body, BodyMesh, FaceMesh, BodyDescribe, BodyError)
		|| !FMantleCosmeticExport::AvatarSkeletonId(BodyMesh, *Contract, SkeletonId, BodyError)
		|| !FMantleAvatarPreviewPlan::ForSkeleton(SkeletonId, FaceMesh != nullptr, Plan, BodyError))
	{
		Say(FString::Printf(TEXT("Sheet: none, no body to show it on (%s)"), *BodyError));
		return true;
	}
	const FMantleAvatarPreviewInputs Inputs = FMantleAvatarPreviewInputs::Gather(Plan, Driver, FString(), DriverError);
	if (!Inputs.Retargeter || !UMantlePreviewRetargetAnimInstance::IsRetargetNodeAvailable(&BodyError))
	{
		Say(FString::Printf(TEXT("Sheet: none, the body cannot be posed (%s%s)"), *FString::Join(Inputs.Missing, TEXT("; ")), *BodyError));
		return true;
	}
	FaceMesh = Plan.bShowFace ? FaceMesh : nullptr;
	const TSharedRef<const FMantleAnimClip> Played = WithoutRoot(Clip, Driver->GetRefSkeleton().GetBoneName(0));

	FMantleThumbnailSubject Subject = FMantleCosmeticThumbnail::AvatarSubject(BodyMesh, FaceMesh);
	Subject.Frame = StandingFrame(BodyMesh);
	Subject.bFloor = true;
	const TArray<float> Times = Moments(Clip.GetDuration(), SheetMoments);
	// TEMPORARY (trials, removed before merge): MANTLE_EMOTE_BODY_MATCH=1 poses the body through a copy of the client's
	// retargeter whose body pose has the UEFN mannequin's rest hands, to see what the same fix would do on players' clients.
	UObject* BodyMatched = nullptr;
	if (FPlatformMisc::GetEnvironmentVariable(TEXT("MANTLE_EMOTE_BODY_MATCH")) == TEXT("1"))
	{
		FString MatchNote;
		FString MatchError;
		BodyMatched = FMantleEmoteRetarget::MakeRestHandsCopy(Inputs.Retargeter, Driver->GetRefSkeleton(), BodyMesh->GetRefSkeleton(), MatchNote, MatchError);
		Say(FString::Printf(TEXT("TRIAL body rest hands: %s"), BodyMatched ? *MatchNote : *MatchError));
	}
	ON_SCOPE_EXIT
	{
		FMantleEmoteRetarget::ReleaseRestHandsCopy(BodyMatched);
	};
	UObject* const Retargeter = BodyMatched ? BodyMatched : Inputs.Retargeter;
	const FString SheetFile = FPaths::Combine(Directory, Label + TEXT("_sheet.png"));
	FString Problems;
	const bool bWritten = DrawSheet(Subject, Times, [Driver, Retargeter, Played](FMantleThumbnailSubject& Shot, const float Time)
	{
		Shot.Pose = [Driver, Retargeter, Played, Time](FPreviewScene& Scene, TConstArrayView<USkeletalMeshComponent*> Components, FBox&)
		{
			PoseOnBody(Scene, Components, Driver, Retargeter, Played, Time);
		};
	}, TileSize, SheetFile, Problems);
	Say(FString::Printf(TEXT("Sheet: %s, %s, moments left to right then down: %s%s"), bWritten ? *SheetFile : TEXT("NOT WRITTEN"), *BodyDescribe,
		*DescribeTimes(Times), *Problems));

	// The clip as it was made, on the body it was made on, at the same moments: what the sheet above should look like.
	if (USkeletalMesh* SourceMesh = FMantleEmoteRetarget::ChooseSourceMesh(Sequence, nullptr))
	{
		FMantleThumbnailSubject Source = FMantleCosmeticThumbnail::AvatarSubject(SourceMesh, nullptr);
		Source.Frame = StandingFrame(SourceMesh);
		Source.bFloor = true;
		const FString SourceFile = FPaths::Combine(Directory, Label + TEXT("_source.png"));
		FString SourceProblems;
		const bool bSourceWritten = DrawSheet(Source, Times, [Sequence](FMantleThumbnailSubject& Shot, const float Time)
		{
			Shot.Pose = [Sequence, Time](FPreviewScene& Scene, TConstArrayView<USkeletalMeshComponent*> Components, FBox&)
			{
				PoseSource(Scene, Components, Sequence, Time);
			};
		}, TileSize, SourceFile, SourceProblems);
		Say(FString::Printf(TEXT("Source: %s, the original clip on %s (root motion and all), same moments%s"), bSourceWritten ? *SourceFile : TEXT("NOT WRITTEN"),
			*SourceMesh->GetPathName(), *SourceProblems));

		// Hand close-ups, the source's and the converted side by side: right hand, then left, at three moments.
		const TArray<float> HandTimes = { Clip.GetDuration() * 0.25f, Clip.GetDuration() * 0.5f, Clip.GetDuration() * 0.75f };
		TArray<TArray<FColor>> Tiles;
		FString HandProblems;
		// The posed meshes' bones at each close-up, in capture order: what players see, both retargets in.
		TArray<TArray<FTransform>> ShownSource;
		TArray<TArray<FTransform>> ShownConverted;
		auto Keep = [](TConstArrayView<USkeletalMeshComponent*> Components, TArray<TArray<FTransform>>& Into)
		{
			Into.Add(Components.IsValidIndex(0) && Components[0] ? TArray<FTransform>(Components[0]->GetComponentSpaceTransforms()) : TArray<FTransform>());
		};
		const int32 HandSize = FMath::Max(256, TileSize * 3 / 4);
		for (const float Time : HandTimes)
		{
			for (const TCHAR* Hand : { TEXT("hand_r"), TEXT("hand_l") })
			{
				for (const bool bConverted : { false, true })
				{
					FMantleThumbnailSubject Shot = bConverted ? Subject : Source;
					const FName HandBone(Hand);
					if (bConverted)
					{
						Shot.Pose = [Driver, Retargeter, Played, Time, HandBone, &Keep, &ShownConverted](FPreviewScene& Scene, TConstArrayView<USkeletalMeshComponent*> Components, FBox& Frame)
						{
							PoseOnBody(Scene, Components, Driver, Retargeter, Played, Time);
							Frame = HandFrame(Components, HandBone);
							Keep(Components, ShownConverted);
						};
					}
					else
					{
						Shot.Pose = [Sequence, Time, HandBone, &Keep, &ShownSource](FPreviewScene& Scene, TConstArrayView<USkeletalMeshComponent*> Components, FBox& Frame)
						{
							PoseSource(Scene, Components, Sequence, Time);
							Frame = HandFrame(Components, HandBone);
							Keep(Components, ShownSource);
						};
					}
					TArray<FColor> Pixels;
					FString Error;
					if (!FMantleCosmeticThumbnail::Capture(Shot, HandSize, Pixels, Error))
					{
						HandProblems += FString::Printf(TEXT("; %s at %.2f s could not be drawn (%s)"), Hand, Time, *Error);
					}
					Tiles.Add(MoveTemp(Pixels));
					CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
				}
			}
		}
		int32 Width = 0;
		int32 Height = 0;
		const TArray<FColor> HandSheet = Tile(Tiles, HandSize, 4, Width, Height);
		const FString HandsFile = FPaths::Combine(Directory, Label + TEXT("_hands.png"));
		const bool bHandsWritten = FFileHelper::SaveArrayToFile(FMantlePng::EncodeRGBA8(Width, Height, HandSheet), *HandsFile);
		Say(FString::Printf(TEXT("Hand close-ups: %s, each row one moment (%s), columns: source right, converted right, source left, converted left%s"),
			bHandsWritten ? *HandsFile : TEXT("NOT WRITTEN"), *DescribeTimes(HandTimes), *HandProblems));
		ShownSource.SetNum(FMath::Min(ShownSource.Num(), ShownConverted.Num()));
		ShownConverted.SetNum(ShownSource.Num());
		bool bComplete = ShownSource.Num() > 0;
		for (int32 Index = 0; Index < ShownSource.Num(); ++Index)
		{
			bComplete = bComplete && ShownSource[Index].Num() == SourceMesh->GetRefSkeleton().GetRawBoneNum()
				&& ShownConverted[Index].Num() == BodyMesh->GetRefSkeleton().GetRawBoneNum();
		}
		if (bComplete)
		{
			const FMantleHandMatch Shown = CompareHands(ShownSource, SourceMesh->GetRefSkeleton(), ShownConverted, BodyMesh->GetRefSkeleton());
			Say(FString::Printf(TEXT("Hands as shown (the close-ups' bodies, the client's retarget onto %s included): finger bend %+.1f degrees on average against the source, %.1f degrees mean error over %d joints, worst joint %s %.1f degrees; wrists %.1f cm from the source's place relative to the chest, %+.1f cm farther from the head"),
				*BodyMesh->GetName(), Shown.BendBiasDegrees, Shown.BendErrorDegrees, Shown.Joints, *Shown.BendWorstJoint.ToString(), Shown.BendWorstDegrees,
				Shown.WristErrorCm, Shown.WristCloserCm));
		}
	}
	return true;
}

namespace MantleEmoteCheckPrivate
{
	void RunCommand(const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogMantleCheckEmote, Error, TEXT("MANTLE_CHECK_EMOTE usage: Mantle.Dev.CheckEmote <AnimSequence> <Directory> [Label] [female|male|<SkeletalMesh>] [size]"));
			return;
		}
		FString Path = Args[0];
		if (!FPackageName::GetShortName(Path).Contains(TEXT(".")))
		{
			Path += TEXT(".") + FPackageName::GetShortName(Path);
		}
		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *Path);
		const FString Label = Args.Num() > 2 && !Args[2].IsEmpty() && Args[2] != TEXT("-") ? Args[2] : (Sequence ? Sequence->GetName() : FString(TEXT("emote")));
		const FString Body = Args.Num() > 3 && Args[3] != TEXT("-") ? Args[3] : FString();
		const int32 Size = Args.Num() > 4 ? FMath::Clamp(FCString::Atoi(*Args[4]), 128, 2048) : 512;
		FString Error;
		if (!FMantleEmoteCheck::Run(Sequence, Args[1], Label, Body, Size, Error))
		{
			UE_LOG(LogMantleCheckEmote, Error, TEXT("MANTLE_CHECK_EMOTE failed %s: %s"), *Path, *Error);
		}
	}

	FAutoConsoleCommand GMantleDevCheckEmoteCommand(
		TEXT("Mantle.Dev.CheckEmote"),
		TEXT("Mantle.Dev.CheckEmote <AnimSequence> <Directory> [Label] [female|male|<SkeletalMesh>] [size] -- runs the emote export as the Cosmetics panel ")
		TEXT("would upload it (retarget, sample, validate, encode), reads the clip back as the client does, and draws it on a standard body at several ")
		TEXT("moments with the catalog thumbnail capture: <Label>_notes.txt and <Label>_sheet.png in Directory. Saves and uploads nothing."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RunCommand));
}
