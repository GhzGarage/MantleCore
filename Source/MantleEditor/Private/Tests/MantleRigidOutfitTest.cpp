// Copyright Joshua Eger. All Rights Reserved.
#include "MantleParametricOutfit.h"
#include "MantleAvatarContract.h"
#include "MantleCosmeticValidation.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// An Outfit made into a rigid accessory hangs from its slot's socket bone: a hat from the head (fitted to the reference
// head, as before), a waist piece from the pelvis. Maths only, on the plugin's contract.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleRigidOutfitSocketBoneTest,
	"Mantle.Editor.ParametricOutfit.RigidHangsFromSocketBone",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleRigidOutfitSocketBoneTest::RunTest(const FString& Parameters)
{
	FString ContractError;
	const FMantleAvatarContract* Contract = FMantleAvatarContract::Get(&ContractError);
	if (!TestNotNull(FString::Printf(TEXT("the contract loads (%s)"), *ContractError), Contract)) { return false; }

	FMantleParametricOutfit::FRigidPivot Hat, Waist, Face, Top;
	FString Error;
	TestTrue(TEXT("acc_hat has a pivot"), FMantleParametricOutfit::RigidPivot(*Contract, TEXT("acc_hat"), Hat, Error));
	TestEqual(TEXT("a hat hangs from the head"), Hat.Bone, FString(TEXT("head")));
	TestTrue(TEXT("a hat is fitted to the reference head"), Hat.bHeadFit);
	TestTrue(TEXT("acc_waist has a pivot"), FMantleParametricOutfit::RigidPivot(*Contract, TEXT("acc_waist"), Waist, Error));
	TestEqual(TEXT("a waist piece hangs from the pelvis"), Waist.Bone, FString(TEXT("pelvis")));
	TestFalse(TEXT("a waist piece keeps its size"), Waist.bHeadFit);
	TestTrue(TEXT("acc_face has a pivot"), FMantleParametricOutfit::RigidPivot(*Contract, TEXT("acc_face"), Face, Error));
	TestFalse(TEXT("only the hat socket is fitted per avatar"), Face.bHeadFit);
	TestFalse(TEXT("a clothing slot is not an accessory"), FMantleParametricOutfit::RigidPivot(*Contract, TEXT("top"), Top, Error));

	// A MetaHuman-like bind pose: the pelvis 95 cm up, the head 165 cm up, each with its own rotation.
	const FTransform Pelvis(FRotator(0.0, -90.0, 90.0).Quaternion(), FVector(0.0, 1.5, 95.0));
	const FTransform Head(FRotator(-5.0, 90.0, -88.0).Quaternion(), FVector(0.0, -2.0, 165.0));

	// The hat route is unchanged: in the head's frame, then the reference-head fit undone.
	FMantleHeadFit::FFit Fit;
	Fit.Scale = FVector3f(0.9f, 1.1f, 0.95f);
	Fit.Offset = FVector3f(1.0f, -0.5f, 0.25f);
	const FVector3f Crown(2.0f, -1.0f, 180.0f), Up(0.0f, 0.0f, 1.0f);
	const FVector3f WasPosition = (FVector3f(Head.InverseTransformPositionNoScale(FVector(Crown))) - Fit.Offset) / Fit.Scale;
	const FVector3f WasNormal = (FVector3f(Head.InverseTransformVectorNoScale(FVector(Up))) * Fit.Scale).GetSafeNormal();
	TestTrue(TEXT("a hat vertex lands where it did"), FMantleParametricOutfit::RigidPosition(Crown, Head, Fit).Equals(WasPosition, 1e-4f));
	TestTrue(TEXT("a hat normal points where it did"), FMantleParametricOutfit::RigidNormal(Up, Head, Fit).Equals(WasNormal, 1e-5f));

	// A shirt tied round the hips: a ring 18 cm out, 82 to 100 cm up, with a flap hanging to 70 cm at the back.
	TArray<FVector3f> Shirt;
	for (int32 Step = 0; Step < 24; ++Step)
	{
		const float Angle = 2.0f * UE_PI * Step / 24.0f;
		for (const float Height : { 82.0f, 100.0f })
		{
			Shirt.Add(FVector3f(18.0f * FMath::Cos(Angle), 18.0f * FMath::Sin(Angle), Height));
		}
	}
	Shirt.Add(FVector3f(-10.0f, 16.0f, 70.0f));
	Shirt.Add(FVector3f(10.0f, 16.0f, 70.0f));
	auto Placed = [&Shirt](const FTransform& Pose)
	{
		FBox3f Box(ForceInit);
		for (const FVector3f& Point : Shirt) { Box += FMantleParametricOutfit::RigidPosition(Point, Pose, FMantleHeadFit::FFit()); }
		return Box;
	};
	TArray<FMantleCosmeticIssue> FromPelvis, FromHead;
	FMantleCosmeticChecks::CheckAccessoryBounds(TEXT("acc_waist"), Placed(Pelvis), *Contract, FromPelvis);
	FMantleCosmeticChecks::CheckAccessoryBounds(TEXT("acc_waist"), Placed(Head), *Contract, FromHead);
	TestEqual(FString::Printf(TEXT("from the pelvis it is within acc_waist's reach (%s)"), FromPelvis.IsEmpty() ? TEXT("") : *FromPelvis[0].ToString()),
		FromPelvis.Num(), 0);
	TestTrue(TEXT("from the head (the old route) it is out of reach"), FromHead.Num() > 0);
	return true;
}

#endif
