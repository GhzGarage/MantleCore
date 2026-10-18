// Copyright Joshua Eger. All Rights Reserved.
#include "MantleHeadFit.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Hats fit every head: the reference cranium maps onto a measured one, and the acc_hat socket carries that map. Maths only.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleHeadFitMapsReferenceOntoHeadTest,
	"Mantle.Editor.HeadFit.MapsReferenceOntoHead",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleHeadFitMapsReferenceOntoHeadTest::RunTest(const FString& Parameters)
{
	// The reference head as measured (MH_MantleNeutral) and a head 20% smaller (ROXY measured 0.82).
	const FBox3f Reference(FVector3f(0.03f, -7.66f, -8.92f), FVector3f(16.65f, 13.01f, 8.90f));
	const FBox3f Small(Reference.Min * 0.8f, Reference.Max * 0.8f);
	const FMantleHeadFit::FFit Fit = FMantleHeadFit::FitTo(Reference, Small);
	TestTrue(TEXT("the scale is the size ratio on every axis"), Fit.Scale.Equals(FVector3f(0.8f), 1e-4f));
	for (const FVector3f& Corner : { Reference.Min, Reference.Max, Reference.GetCenter() })
	{
		const FVector3f Mapped = Fit.Scale * Corner + Fit.Offset;
		const FVector3f Want = Corner * 0.8f;
		TestTrue(FString::Printf(TEXT("%s lands on %s"), *Corner.ToString(), *Want.ToString()), Mapped.Equals(Want, 1e-3f));
	}

	// The same head is identity.
	const FMantleHeadFit::FFit Same = FMantleHeadFit::FitTo(Reference, Reference);
	TestTrue(TEXT("the reference onto itself scales by 1"), Same.Scale.Equals(FVector3f::OneVector, 1e-5f));
	TestTrue(TEXT("the reference onto itself moves nothing"), Same.Offset.Equals(FVector3f::ZeroVector, 1e-4f));

	// A head past the socket's bounds is clamped to them and stays centred.
	const FBox3f Huge(Reference.Min * 3.0f, Reference.Max * 3.0f);
	const FMantleHeadFit::FFit Clamped = FMantleHeadFit::FitTo(Reference, Huge);
	TestTrue(TEXT("the scale is clamped to 2"), Clamped.Scale.Equals(FVector3f(2.0f), 1e-5f));
	TestTrue(TEXT("the clamped map keeps the centres together"),
		(Clamped.Scale * Reference.GetCenter() + Clamped.Offset).Equals(Huge.GetCenter(), 1e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleHeadFitHatSocketTest,
	"Mantle.Editor.HeadFit.HatSocketCarriesTheMap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleHeadFitHatSocketTest::RunTest(const FString& Parameters)
{
	const FBox3f Reference(FVector3f(0.0f, -8.0f, -9.0f), FVector3f(16.0f, 12.0f, 9.0f));
	const FBox3f Measured(FVector3f(1.0f, -7.0f, -8.0f), FVector3f(15.0f, 11.0f, 8.0f));
	const FMantleHeadFit::FFit Fit = FMantleHeadFit::FitTo(Reference, Measured);

	// Mantle's sockets sit at identity on the head: the override is the map itself.
	const FMantleCosmeticSocketOverride Identity = FMantleHeadFit::HatSocket(Reference, Measured, FTransform::Identity);
	TestEqual(TEXT("the override is for acc_hat"), Identity.Socket, FString(TEXT("acc_hat")));
	TestTrue(TEXT("location and scale are on, rotation is the socket's"), Identity.bLocation && Identity.bScale && !Identity.bRotation);
	TestTrue(TEXT("the location is the offset"), Identity.Location.Equals(Fit.Offset, 1e-4f));
	TestTrue(TEXT("the scale is the fit's"), Identity.Scale.Equals(Fit.Scale, 1e-5f));

	// A socket turned and moved on the head: a hat point lands where the map, in the socket's frame, puts it.
	const FTransform Socket(FRotator(0.0f, 90.0f, 0.0f), FVector(2.0f, 3.0f, 4.0f));
	const FMantleCosmeticSocketOverride Turned = FMantleHeadFit::HatSocket(Reference, Measured, Socket);
	const FTransform Overridden(Socket.GetRotation(), FVector(Turned.Location), FVector(Turned.Scale));
	const FVector3f HatPoint(16.0f, 0.0f, 0.0f);
	const FVector Got = Overridden.TransformPosition(FVector(HatPoint));
	const FVector Want = Socket.TransformPosition(FVector(Fit.Scale * HatPoint + Fit.Offset));
	TestTrue(FString::Printf(TEXT("a hat point lands at %s (got %s)"), *Want.ToString(), *Got.ToString()), Got.Equals(Want, 1e-3));
	return true;
}

#endif
