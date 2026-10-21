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
#include "MantleFitBodies.h"
#include "MantlePng.h"

#include "Animation/AnimSequence.h"
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
	// The whole standing body with room to move, the same frame at every moment, on the thumbnail scene's floor.
	Subject.Frame = BodyMesh->GetImportedBounds().GetBox().ExpandBy(FVector(45.0, 45.0, 10.0));
	Subject.bFloor = true;
	const TArray<float> Times = Moments(Clip.GetDuration(), SheetMoments);
	TArray<TArray<FColor>> Tiles;
	TArray<FString> Shown;
	for (const float Time : Times)
	{
		UObject* const Retargeter = Inputs.Retargeter;
		Subject.Pose = [Driver, Retargeter, Played, Time](FPreviewScene& Scene, TConstArrayView<USkeletalMeshComponent*> Components)
		{
			USkeletalMeshComponent* BodyComponent = Components.IsValidIndex(0) ? Components[0] : nullptr;
			if (!BodyComponent)
			{
				return;
			}
			// The hidden UEFN driver plays the clip, held at Time (the client's emote layer, the root left out).
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
				ClipInstance->SetClip(Played, Playback);
			}
			// The body copies the driver through its retargeter, as every avatar does.
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
				MantleEmoteCheckPrivate::Evaluate(DriverComponent);
				MantleEmoteCheckPrivate::Evaluate(BodyComponent);
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
		};
		TArray<FColor> Pixels;
		FString Error;
		if (!FMantleCosmeticThumbnail::Capture(Subject, TileSize, Pixels, Error))
		{
			Say(FString::Printf(TEXT("Sheet: the moment at %.2f s could not be drawn (%s)"), Time, *Error));
		}
		Tiles.Add(MoveTemp(Pixels));
		Shown.Add(FString::Printf(TEXT("%.2f s"), Time));
	}
	int32 Width = 0;
	int32 Height = 0;
	const TArray<FColor> Sheet = Tile(Tiles, TileSize, SheetColumns, Width, Height);
	const FString SheetFile = FPaths::Combine(Directory, Label + TEXT("_sheet.png"));
	const bool bWritten = FFileHelper::SaveArrayToFile(FMantlePng::EncodeRGBA8(Width, Height, Sheet), *SheetFile);
	Say(FString::Printf(TEXT("Sheet: %s, %s, moments left to right then down: %s"), bWritten ? *SheetFile : TEXT("NOT WRITTEN"), *BodyDescribe,
		*FString::Join(Shown, TEXT(", "))));
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
