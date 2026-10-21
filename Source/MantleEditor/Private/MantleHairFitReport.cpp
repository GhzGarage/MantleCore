// Copyright Joshua Eger. All Rights Reserved.
#include "MantleHairFitReport.h"

bool FMantleHairFitReport::IsBakedHeadName(const FString& SectionName)
{
	const FString Name = SectionName.ToLower();
	if (Name.Contains(TEXT("cap")) || Name.Contains(TEXT("scalp"))) return false;
	return Name.Contains(TEXT("head")) || Name.Contains(TEXT("face")) || Name.Contains(TEXT("skin")) || Name.Contains(TEXT("body"));
}

FMantleHairFitReport::FResult FMantleHairFitReport::Describe(const FInput& Input)
{
	FResult Out;
	Out.Headline = Headline();

	const bool bManyInside = Input.CardVertices > 0
		&& 100.0 * Input.CardInsideAfter / Input.CardVertices > LowInsidePercent;
	const bool bRootsFloat = !Input.bHadCap && Input.RootsWithin5mmAfter >= 0.0 && Input.RootsWithin5mmAfter < LowRootPercent;
	Out.bLowConfidence = !Input.bMeetsOutsideBar || Input.bScalpCheckFit || bManyInside || bRootsFloat;
	if (Out.bLowConfidence)
	{
		Out.Messages.Add(TEXT("We are not sure this hair sits right on the head. Look closely at the picture; if it is off, press Adjust and the hair uploads where you made it."));
	}

	Out.bBakedHead = Input.bBakedHead;
	if (Input.bBakedHead)
	{
		Out.Messages.Add(FString::Printf(TEXT("This hair file has a head built into it (\"%s\"). We used only the hair to size it, and the built-in head is still in the file, so check that it does not show through or remove it before you upload."),
			*Input.BakedHeadSection));
	}
	return Out;
}
