// Copyright Joshua Eger. All Rights Reserved.
#include "MantleAnimClip.h"
#include "MantleAnimClipWriter.h"
#include "MantleAvatarContract.h"
#include "MantleBuiltInMannequin.h"
#include "MantleCosmeticValidation.h"
#include "MantleEmoteCheck.h"
#include "MantleEmoteRetarget.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "ReferenceSkeleton.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

#if WITH_DEV_AUTOMATION_TESTS

// Emotes end to end in the plugin: the MAN1 rules the cook applies, the bytes players' clients read back, which contract
// skeleton a clip is on, and IK Rig's batch retarget the upload runs.
namespace MantleEmoteTestPrivate
{
	const FMantleAvatarContract* Contract(FAutomationTestBase& Test)
	{
		FString Error;
		const FMantleAvatarContract* Out = FMantleAvatarContract::Get(&Error);
		if (!Out)
		{
			Test.AddError(FString::Printf(TEXT("the avatar contract does not load: %s"), *Error));
		}
		return Out;
	}

	/**
	 * A one-second clip (31 frames) on the uefn skeleton: the root still, the pelvis bobbing 4 cm and turning 30 degrees, the
	 * left upper arm raising 60 degrees, the head constant, and a footstep event.
	 */
	FMantleAnimClipData MakeClip(const FMantleAvatarContract& Contract)
	{
		const FMantleContractSkeleton* Skeleton = Contract.FindSkeleton(Contract.EmoteSkeleton);
		FMantleAnimClipData Clip;
		Clip.FrameRate = Contract.EmoteFrameRate;
		Clip.FrameCount = 31;
		// Out of skeleton order on purpose: the writer sorts them.
		FMantleAnimClipTrackData& Arm = Clip.Tracks.AddDefaulted_GetRef();
		Arm.Bone = TEXT("upperarm_l");
		FMantleAnimClipTrackData& Pelvis = Clip.Tracks.AddDefaulted_GetRef();
		Pelvis.Bone = TEXT("pelvis");
		FMantleAnimClipTrackData& Root = Clip.Tracks.AddDefaulted_GetRef();
		Root.Bone = Skeleton ? Skeleton->Root() : FName(TEXT("root"));
		Root.Rotations.Add(FQuat4f::Identity);
		Root.Translations.Add(FVector3f::ZeroVector);
		FMantleAnimClipTrackData& Head = Clip.Tracks.AddDefaulted_GetRef();
		Head.Bone = TEXT("head");
		for (int32 Frame = 0; Frame < Clip.FrameCount; ++Frame)
		{
			const float Alpha = static_cast<float>(Frame) / (Clip.FrameCount - 1);
			Pelvis.Rotations.Add(FQuat4f(FVector3f::UpVector, FMath::DegreesToRadians(30.0f * Alpha)));
			Pelvis.Translations.Add(FVector3f(0.0f, 0.0f, 95.0f + 4.0f * FMath::Sin(Alpha * UE_TWO_PI)));
			Arm.Rotations.Add(FQuat4f(FVector3f::ForwardVector, FMath::DegreesToRadians(60.0f * Alpha)));
			Head.Rotations.Add(FQuat4f(FVector3f::RightVector, 0.1f));
		}
		FMantleAnimClipEventData& Step = Clip.Events.AddDefaulted_GetRef();
		Step.Name = TEXT("footstep_l");
		Step.TimeSeconds = 0.5f;
		return Clip;
	}

	bool HasError(const TArray<FMantleCosmeticIssue>& Issues, const TCHAR* Text)
	{
		return Issues.ContainsByPredicate([Text](const FMantleCosmeticIssue& Issue)
		{
			return Issue.Severity == EMantleCosmeticSeverity::Error && Issue.Message.Contains(Text);
		});
	}

	TArray<FMantleCosmeticIssue> ValidateWith(const FMantleAvatarContract& Contract, TFunctionRef<void(FMantleAnimClipData&)> Change)
	{
		FMantleAnimClipData Clip = MakeClip(Contract);
		Change(Clip);
		TArray<FMantleCosmeticIssue> Issues;
		FMantleAnimClipWriter::Validate(Clip, Contract, Issues);
		return Issues;
	}

	FMantleAnimClipTrackData* Find(FMantleAnimClipData& Clip, const TCHAR* Bone)
	{
		return Clip.Tracks.FindByPredicate([Bone](const FMantleAnimClipTrackData& Track) { return Track.Bone == FName(Bone); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMantleEmoteValidateTest, "Mantle.Editor.Emote.Validate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMantleEmoteValidateTest::RunTest(const FString& Parameters)
{
	using namespace MantleEmoteTestPrivate;
	const FMantleAvatarContract* C = Contract(*this);
	if (!C)
	{
		return false;
	}
	const FMantleAvatarContract& K = *C;
	{
		TArray<FMantleCosmeticIssue> Issues = ValidateWith(K, [](FMantleAnimClipData&) {});
		TestFalse(FString::Printf(TEXT("a good clip passes (%s)"), Issues.Num() > 0 ? *Issues[0].ToString() : TEXT("")), FMantleCosmeticChecks::HasErrors(Issues));
	}
	TestTrue(TEXT("another frame rate is refused"), HasError(ValidateWith(K, [](FMantleAnimClipData& Clip) { Clip.FrameRate = 24.0f; }), TEXT("frame rate")));
	TestTrue(TEXT("no frames is refused"), HasError(ValidateWith(K, [](FMantleAnimClipData& Clip) { Clip.FrameCount = 0; }), TEXT("no frames")));
	TestTrue(TEXT("a clip over the length limit is refused"), HasError(ValidateWith(K, [&K](FMantleAnimClipData& Clip)
	{
		Clip.FrameCount = FMath::RoundToInt(K.EmoteMaxSeconds * K.EmoteFrameRate) + 2;
		Clip.Tracks.SetNum(1);
		Clip.Tracks[0].Rotations.SetNum(1);
	}), TEXT("the limit is")));
	{
		// Exactly the limit is allowed.
		TArray<FMantleCosmeticIssue> Issues = ValidateWith(K, [&K](FMantleAnimClipData& Clip)
		{
			Clip.FrameCount = FMath::RoundToInt(K.EmoteMaxSeconds * K.EmoteFrameRate) + 1;
			for (FMantleAnimClipTrackData& Track : Clip.Tracks)
			{
				Track.Rotations.SetNum(1);
				Track.Translations.SetNum(FMath::Min(1, Track.Translations.Num()));
			}
		});
		TestFalse(TEXT("a clip of exactly the length limit passes"), HasError(Issues, TEXT("long")));
	}
	TestTrue(TEXT("a root that moves past the limit is refused"), HasError(ValidateWith(K, [&K](FMantleAnimClipData& Clip)
	{
		FMantleAnimClipTrackData* Root = Find(Clip, TEXT("root"));
		Root->Translations.SetNum(Clip.FrameCount);
		for (int32 Frame = 0; Frame < Clip.FrameCount; ++Frame)
		{
			Root->Translations[Frame] = FVector3f((K.EmoteMaxRootCm + 10.0f) * Frame / (Clip.FrameCount - 1), 0.0f, 0.0f);
		}
	}), TEXT("the root moves")));
	TestFalse(TEXT("a root that moves within the limit passes"), HasError(ValidateWith(K, [&K](FMantleAnimClipData& Clip)
	{
		FMantleAnimClipTrackData* Root = Find(Clip, TEXT("root"));
		Root->Translations.SetNum(Clip.FrameCount);
		for (int32 Frame = 0; Frame < Clip.FrameCount; ++Frame)
		{
			Root->Translations[Frame] = FVector3f((K.EmoteMaxRootCm - 10.0f) * Frame / (Clip.FrameCount - 1), 0.0f, 0.0f);
		}
	}), TEXT("the root moves")));
	TestTrue(TEXT("a bone the uefn skeleton lacks is refused"), HasError(ValidateWith(K, [](FMantleAnimClipData& Clip)
	{
		Clip.Tracks.Last().Bone = TEXT("FACIAL_C_Jaw");
	}), TEXT("is not a bone of the")));
	TestTrue(TEXT("two tracks on one bone are refused"), HasError(ValidateWith(K, [](FMantleAnimClipData& Clip)
	{
		Clip.Tracks.Add(Clip.Tracks[0]);
	}), TEXT("has two tracks")));
	TestTrue(TEXT("a clip without a pelvis track is refused"), HasError(ValidateWith(K, [](FMantleAnimClipData& Clip)
	{
		Clip.Tracks.RemoveAll([](const FMantleAnimClipTrackData& Track) { return Track.Bone == FName(TEXT("pelvis")); });
	}), TEXT("no \"pelvis\" track")));
	TestTrue(TEXT("a key count that is neither 1 nor the frame count is refused"), HasError(ValidateWith(K, [](FMantleAnimClipData& Clip)
	{
		Find(Clip, TEXT("pelvis"))->Rotations.SetNum(5);
	}), TEXT("rotation keys")));
	TestTrue(TEXT("a rotation that is not a unit quaternion is refused"), HasError(ValidateWith(K, [](FMantleAnimClipData& Clip)
	{
		Find(Clip, TEXT("pelvis"))->Rotations[3] = FQuat4f(0.5f, 0.0f, 0.0f, 0.5f);
	}), TEXT("unit quaternion")));
	TestTrue(TEXT("a translation out of range is refused"), HasError(ValidateWith(K, [](FMantleAnimClipData& Clip)
	{
		Find(Clip, TEXT("pelvis"))->Translations[3] = FVector3f(0.0f, 0.0f, 20000.0f);
	}), TEXT("translation of")));
	TestTrue(TEXT("an event outside the allowlist is refused"), HasError(ValidateWith(K, [](FMantleAnimClipData& Clip)
	{
		Clip.Events[0].Name = TEXT("PlaySound");
	}), TEXT("not one of the allowed events")));
	TestTrue(TEXT("an event after the clip's end is refused"), HasError(ValidateWith(K, [](FMantleAnimClipData& Clip)
	{
		Clip.Events[0].TimeSeconds = 5.0f;
	}), TEXT("outside the clip")));
	TestTrue(TEXT("more events than the limit are refused"), HasError(ValidateWith(K, [&K](FMantleAnimClipData& Clip)
	{
		for (int32 Index = 0; Index < K.EmoteMaxEvents; ++Index)
		{
			Clip.Events.Add(Clip.Events[0]);
		}
	}), TEXT("events, the limit")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMantleEmoteRoundTripTest, "Mantle.Editor.Emote.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMantleEmoteRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace MantleEmoteTestPrivate;
	const FMantleAvatarContract* C = Contract(*this);
	const FMantleContractSkeleton* Skeleton = C ? C->FindSkeleton(C->EmoteSkeleton) : nullptr;
	if (!TestNotNull(TEXT("the contract has the emote skeleton"), Skeleton))
	{
		return false;
	}
	const FMantleAnimClipData Clip = MakeClip(*C);
	const TArray<uint8> Bytes = FMantleAnimClipWriter::Encode(Clip, Skeleton);
	TestEqual(TEXT("encoding is deterministic"), FMantleAnimClipWriter::Encode(Clip, Skeleton), Bytes);
	TestTrue(TEXT("the clip is within the size limit"), Bytes.Num() > 0 && Bytes.Num() <= C->EmoteMaxBytes);

	// Read back as players' clients read it.
	const FMantleAnimClipReadResult Read = FMantleAnimClipReader::Read(Bytes);
	if (!TestTrue(FString::Printf(TEXT("the client reads it (%s)"), *Read.Error), Read.bSuccess))
	{
		return false;
	}
	const FMantleAnimClip& Back = Read.Clip;
	TestEqual(TEXT("frame rate"), Back.FrameRate, Clip.FrameRate);
	TestEqual(TEXT("frame count"), Back.FrameCount, Clip.FrameCount);
	TestFalse(TEXT("an emote carries no skeleton block"), Back.HasSkeleton());
	if (!TestEqual(TEXT("every track"), Back.Tracks.Num(), Clip.Tracks.Num()))
	{
		return false;
	}
	for (int32 Index = 1; Index < Back.Tracks.Num(); ++Index)
	{
		TestTrue(FString::Printf(TEXT("tracks in skeleton order (%s before %s)"), *Back.Tracks[Index - 1].Bone.ToString(), *Back.Tracks[Index].Bone.ToString()),
			Skeleton->Find(Back.Tracks[Index - 1].Bone) < Skeleton->Find(Back.Tracks[Index].Bone));
	}
	for (const FMantleAnimClipTrack& Track : Back.Tracks)
	{
		const FMantleAnimClipTrackData* Source = Clip.Tracks.FindByPredicate([&Track](const FMantleAnimClipTrackData& Each) { return Each.Bone == Track.Bone; });
		if (!TestNotNull(FString::Printf(TEXT("%s came from the clip"), *Track.Bone.ToString()), Source))
		{
			continue;
		}
		bool bConstant = true;
		for (const FQuat4f& Q : Source->Rotations)
		{
			bConstant = bConstant && Q == Source->Rotations[0];
		}
		TestEqual(FString::Printf(TEXT("%s: a constant track is one key, an animated one every frame"), *Track.Bone.ToString()), Track.Rotations.Num(),
			bConstant ? 1 : Clip.FrameCount);
		TestEqual(FString::Printf(TEXT("%s: translations kept as given"), *Track.Bone.ToString()), Track.HasTranslation(), Source->Translations.Num() > 0);
		for (int32 Frame = 0; Frame < Clip.FrameCount; Frame += 5)
		{
			FQuat4f Rotation;
			FVector3f Translation;
			bool bHasTranslation = false;
			Back.SampleTrack(Back.Tracks.IndexOfByPredicate([&Track](const FMantleAnimClipTrack& Each) { return Each.Bone == Track.Bone; }),
				Frame / Clip.FrameRate, Rotation, Translation, bHasTranslation);
			const FQuat4f Want = Source->Rotations[FMath::Min(Frame, Source->Rotations.Num() - 1)];
			TestTrue(FString::Printf(TEXT("%s frame %d: rotation within the 16-bit step (%.5f)"), *Track.Bone.ToString(), Frame, FMath::Abs(Rotation | Want)),
				FMath::Abs(Rotation | Want) > 1.0f - 1e-6f);
			if (bHasTranslation)
			{
				const FVector3f WantT = Source->Translations[FMath::Min(Frame, Source->Translations.Num() - 1)];
				TestTrue(FString::Printf(TEXT("%s frame %d: translation within a hundredth of a cm"), *Track.Bone.ToString(), Frame),
					FVector3f::Distance(Translation, WantT) < 0.01f);
			}
		}
	}
	if (TestEqual(TEXT("the event"), Back.Events.Num(), 1))
	{
		TestEqual(TEXT("event name"), Back.Events[0].Name, FName(TEXT("footstep_l")));
		TestEqual(TEXT("event time"), Back.Events[0].Time, 0.5f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMantleEmoteIdentifySkeletonTest, "Mantle.Editor.Emote.IdentifySkeleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMantleEmoteIdentifySkeletonTest::RunTest(const FString& Parameters)
{
	using namespace MantleEmoteTestPrivate;
	const FMantleAvatarContract* C = Contract(*this);
	if (!C)
	{
		return false;
	}
	auto JointsOf = [](const FMantleContractSkeleton& Fixture)
	{
		TArray<FMantleSkinJointName> Joints;
		for (int32 Bone = 0; Bone < Fixture.Bones.Num(); ++Bone)
		{
			Joints.Add({ Fixture.Bones[Bone], Fixture.Parents[Bone] });
		}
		return Joints;
	};
	// Each skeleton an emote is accepted on is told apart from the others, by its whole bone table.
	for (const FString& Id : { C->EmoteSkeleton, FString(TEXT("metahuman")), FString(TEXT("ue5mannequin")), FString(TEXT("ue4mannequin")) })
	{
		const FMantleContractSkeleton* Fixture = C->FindSkeleton(Id);
		if (TestNotNull(FString::Printf(TEXT("the contract has %s"), *Id), Fixture))
		{
			TestEqual(FString::Printf(TEXT("the %s skeleton is identified as itself"), *Id), FMantleEmoteRetarget::IdentifyJoints(JointsOf(*Fixture), *C), Id);
		}
	}
	// A clip only uses the bones it animates' skeleton: a UE5 Mannequin skeleton without its corrective and IK bones is still UE5.
	if (const FMantleContractSkeleton* UE5 = C->FindSkeleton(TEXT("ue5mannequin")))
	{
		TArray<FMantleSkinJointName> Joints;
		TArray<int32> NewIndex;
		for (int32 Bone = 0; Bone < UE5->Bones.Num(); ++Bone)
		{
			const FString Name = UE5->Bones[Bone].ToString();
			const int32 Parent = UE5->Parents[Bone];
			if (Name.StartsWith(TEXT("ik_")) || Name.Contains(TEXT("_corrective")) || (Parent != INDEX_NONE && NewIndex[Parent] == INDEX_NONE))
			{
				NewIndex.Add(INDEX_NONE);
				continue;
			}
			NewIndex.Add(Joints.Num());
			Joints.Add({ UE5->Bones[Bone], Parent == INDEX_NONE ? INDEX_NONE : NewIndex[Parent] });
		}
		TestEqual(TEXT("the UE5 Mannequin's deforming bones are identified as ue5mannequin"), FMantleEmoteRetarget::IdentifyJoints(Joints, *C), FString(TEXT("ue5mannequin")));
	}
	// A skeleton with a bone none of them has is none of them: it is refused with the retarget hint.
	if (const FMantleContractSkeleton* UE4 = C->FindSkeleton(TEXT("ue4mannequin")))
	{
		TArray<FMantleSkinJointName> Joints = JointsOf(*UE4);
		Joints.Add({ FName(TEXT("tail_01")), 0 });
		TestEqual(TEXT("an unknown bone is no contract skeleton"), FMantleEmoteRetarget::IdentifyJoints(Joints, *C), FString());
	}
	// The plugin's own UEFN mannequin is the emote skeleton: its clips need no retarget.
	FString Error;
	if (USkeletalMesh* Mannequin = FMantleBuiltInMannequin::Get(&Error))
	{
		TestEqual(TEXT("the built-in UEFN mannequin is the emote skeleton"), FMantleEmoteRetarget::IdentifySkeleton(Mannequin->GetSkeleton(), *C), C->EmoteSkeleton);
	}
	else
	{
		AddError(Error);
	}
	TestEqual(TEXT("no skeleton is none"), FMantleEmoteRetarget::IdentifySkeleton(nullptr, *C), FString());
	TestNull(TEXT("no retargeter for the emote skeleton itself"), FMantleEmoteRetarget::RetargeterPathFor(C->EmoteSkeleton));
	for (const TCHAR* Id : { TEXT("metahuman"), TEXT("ue5mannequin"), TEXT("ue4mannequin") })
	{
		const TCHAR* Path = FMantleEmoteRetarget::RetargeterPathFor(Id);
		TestTrue(FString::Printf(TEXT("the plugin ships a retargeter from %s"), Id), Path && FSoftObjectPath(Path).TryLoad() != nullptr);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMantleEmoteRetargetSmokeTest, "Mantle.Editor.Emote.Retarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMantleEmoteRetargetSmokeTest::RunTest(const FString& Parameters)
{
	using namespace MantleEmoteTestPrivate;
	const FMantleAvatarContract* C = Contract(*this);
	FString Error;
	USkeletalMesh* Mannequin = FMantleBuiltInMannequin::Get(&Error);
	if (!C || !TestNotNull(FString::Printf(TEXT("the built-in UEFN mannequin is made (%s)"), *Error), Mannequin))
	{
		return false;
	}
	// The UEFN-to-UEFN Mannequin retargeter maps every chain of one IK rig onto itself: what goes in comes out.
	UObject* Retargeter = FSoftObjectPath(TEXT("/MantleCore/Avatars/Retarget/RTG_UEFN_to_Mantle_UEFNMannequin.RTG_UEFN_to_Mantle_UEFNMannequin")).TryLoad();
	if (!TestNotNull(TEXT("the plugin's UEFN Mannequin retargeter loads"), Retargeter))
	{
		return false;
	}

	// A one-second clip made in memory: the pelvis turns 40 degrees, the left upper arm raises 50.
	UPackage* Package = CreatePackage(TEXT("/Game/__MantleEmoteTest/AS_EmoteRetargetSmoke"));
	UAnimSequence* Sequence = NewObject<UAnimSequence>(Package, TEXT("AS_EmoteRetargetSmoke"), RF_Public | RF_Standalone | RF_Transient);
	Sequence->SetSkeleton(Mannequin->GetSkeleton());
	Sequence->SetPreviewMesh(Mannequin);
	const FReferenceSkeleton& Reference = Mannequin->GetRefSkeleton();
	const int32 Frames = 30;
	{
		IAnimationDataController& Controller = Sequence->GetController();
		Controller.InitializeModel();
		Controller.OpenBracket(FText::FromString(TEXT("Emote retarget test")), false);
		Controller.SetFrameRate(FFrameRate(30, 1), false);
		Controller.SetNumberOfFrames(FFrameNumber(Frames), false);
		for (const TCHAR* Bone : { TEXT("pelvis"), TEXT("upperarm_l") })
		{
			const int32 Index = Reference.FindRawBoneIndex(FName(Bone));
			if (!TestTrue(FString::Printf(TEXT("the mannequin has %s"), Bone), Index != INDEX_NONE))
			{
				return false;
			}
			const FTransform Ref = Reference.GetRawRefBonePose()[Index];
			const FVector Axis = FCString::Strcmp(Bone, TEXT("pelvis")) == 0 ? FVector::UpVector : FVector::ForwardVector;
			const double Degrees = FCString::Strcmp(Bone, TEXT("pelvis")) == 0 ? 40.0 : 50.0;
			TArray<FVector> Positions;
			TArray<FQuat> Rotations;
			TArray<FVector> Scales;
			for (int32 Key = 0; Key <= Frames; ++Key)
			{
				Positions.Add(Ref.GetTranslation());
				Rotations.Add(FQuat(Axis, FMath::DegreesToRadians(Degrees * Key / Frames)) * Ref.GetRotation());
				Scales.Add(FVector::OneVector);
			}
			Controller.AddBoneCurve(FName(Bone), false);
			Controller.SetBoneTrackKeys(FName(Bone), Positions, Rotations, Scales, false);
		}
		Controller.NotifyPopulated();
		Controller.CloseBracket(false);
	}

	FMantleAnimClipData Clip;
	FMantleAnimClipData Original;
	FString SampleError;
	TestTrue(TEXT("the clip itself samples"), FMantleAnimClipWriter::Sample(Sequence, *C, Original, SampleError));
	const bool bRetargeted = FMantleEmoteRetarget::RetargetThrough(Sequence, Mannequin, Mannequin, Retargeter, 1.0 / 30.0,
		[&](UAnimSequence& Retargeted, FString& OutUseError)
		{
			TestTrue(TEXT("the copy is made in the transient folder"), Retargeted.GetPackage()->GetName().StartsWith(FString(FMantleEmoteRetarget::TransientFolder) + TEXT("/")));
			return FMantleAnimClipWriter::Sample(&Retargeted, *C, Clip, OutUseError);
		}, Error);
	Sequence->ClearFlags(RF_Public | RF_Standalone);
	Sequence->MarkAsGarbage();
	if (!TestTrue(FString::Printf(TEXT("IK Rig's batch retarget runs (%s)"), *Error), bRetargeted))
	{
		return false;
	}
	TestEqual(TEXT("the retargeted clip is as long"), Clip.FrameCount, Original.FrameCount);
	TArray<FMantleCosmeticIssue> Issues;
	FMantleAnimClipWriter::Validate(Clip, *C, Issues);
	TestFalse(FString::Printf(TEXT("the retargeted clip is a valid emote (%s)"), Issues.Num() > 0 ? *Issues[0].ToString() : TEXT("")), FMantleCosmeticChecks::HasErrors(Issues));
	for (const TCHAR* Bone : { TEXT("pelvis"), TEXT("upperarm_l") })
	{
		const FMantleAnimClipTrackData* Got = Clip.Tracks.FindByPredicate([Bone](const FMantleAnimClipTrackData& Track) { return Track.Bone == FName(Bone); });
		const FMantleAnimClipTrackData* Want = Original.Tracks.FindByPredicate([Bone](const FMantleAnimClipTrackData& Track) { return Track.Bone == FName(Bone); });
		if (!TestTrue(FString::Printf(TEXT("%s has a track before and after"), Bone), Got && Want && Got->Rotations.Num() > 1 && Want->Rotations.Num() > 1))
		{
			continue;
		}
		const FQuat4f& A = Got->Rotations.Last();
		const FQuat4f& B = Want->Rotations.Last();
		const float Degrees = FMath::RadiansToDegrees(FQuat4f(A).AngularDistance(B));
		TestTrue(FString::Printf(TEXT("%s ends where the source does (%.2f degrees apart)"), Bone, Degrees), Degrees < 2.0f);
	}
	// Nothing the retarget made is left behind.
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Left;
	Registry.GetAssetsByPath(FName(FMantleEmoteRetarget::TransientFolder), Left, true);
	TestEqual(TEXT("the transient folder is empty again"), Left.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMantleEmoteCheckSheetTest, "Mantle.Editor.Emote.CheckSheet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMantleEmoteCheckSheetTest::RunTest(const FString& Parameters)
{
	// The sheet's moments run from the start to the end.
	const TArray<float> Times = FMantleEmoteCheck::Moments(2.5f, 6);
	TestEqual(TEXT("six moments"), Times.Num(), 6);
	TestEqual(TEXT("the first is the start"), Times[0], 0.0f);
	TestEqual(TEXT("the last is the end"), Times.Last(), 2.5f);
	TestEqual(TEXT("a one-frame clip shows one moment"), FMantleEmoteCheck::Moments(0.0f, 6).Num(), 1);

	// Tiles are laid out left to right, then down.
	TArray<TArray<FColor>> Tiles;
	for (const FColor Color : { FColor::Red, FColor::Green, FColor::Blue, FColor::White })
	{
		TArray<FColor>& Tile = Tiles.AddDefaulted_GetRef();
		Tile.Init(Color, 4 * 4);
	}
	int32 Width = 0;
	int32 Height = 0;
	const TArray<FColor> Sheet = FMantleEmoteCheck::Tile(Tiles, 4, 3, Width, Height);
	TestEqual(TEXT("three tiles across"), Width, 12);
	TestEqual(TEXT("two rows"), Height, 8);
	TestEqual(TEXT("the second tile is beside the first"), Sheet[5], FColor::Green);
	TestEqual(TEXT("the fourth starts the second row"), Sheet[4 * Width + 1], FColor::White);
	TestEqual(TEXT("an empty place stays black"), Sheet[5 * Width + 9], FColor::Black);

	// How far an emote takes the body: a pelvis that walks 80 cm forward drifts 80 cm, whatever the root does.
	FMantleAnimClip Clip;
	Clip.FrameRate = 30.0f;
	Clip.FrameCount = 31;
	FMantleAnimClipTrack& Pelvis = Clip.Tracks.AddDefaulted_GetRef();
	Pelvis.Bone = TEXT("pelvis");
	FMantleAnimClipTrack& Root = Clip.Tracks.AddDefaulted_GetRef();
	Root.Bone = TEXT("root");
	for (int32 Frame = 0; Frame < Clip.FrameCount; ++Frame)
	{
		Pelvis.Rotations.Add(FQuat4f::Identity);
		Pelvis.Translations.Add(FVector3f(0.0f, 80.0f * Frame / 30.0f, 95.0f));
		Root.Rotations.Add(FQuat4f(FVector3f::UpVector, FMath::DegreesToRadians(90.0f * Frame / 30.0f)));
		Root.Translations.Add(FVector3f(20.0f * Frame / 30.0f, 0.0f, 0.0f));
	}
	FReferenceSkeleton Skeleton;
	{
		FReferenceSkeletonModifier Modifier(Skeleton, nullptr);
		Modifier.Add(FMeshBoneInfo(TEXT("root"), TEXT("root"), INDEX_NONE), FTransform::Identity);
		Modifier.Add(FMeshBoneInfo(TEXT("pelvis"), TEXT("pelvis"), 0), FTransform(FVector(0.0, 0.0, 95.0)));
	}
	const FMantleEmoteMotion Motion = FMantleEmoteCheck::Measure(Clip, Skeleton);
	TestTrue(FString::Printf(TEXT("the pelvis drifts 80 cm (%.1f)"), Motion.PelvisDriftCm), FMath::IsNearlyEqual(Motion.PelvisDriftCm, 80.0, 0.5));
	TestTrue(FString::Printf(TEXT("the root travels 20 cm (%.1f)"), Motion.RootTravelCm), FMath::IsNearlyEqual(Motion.RootTravelCm, 20.0, 0.5));
	TestTrue(FString::Printf(TEXT("the root turns 90 degrees (%.1f)"), Motion.RootTurnDegrees), FMath::IsNearlyEqual(Motion.RootTurnDegrees, 90.0, 0.5));
	return true;
}

#endif
