// Copyright Joshua Eger. All Rights Reserved.
#include "MantleBodySilhouette.h"

FString FMantleSilhouette::ToString(const bool bShareOfHeight) const
{
	const double K = bShareOfHeight ? 100.0 : 1.0;
	return FString::Printf(TEXT("width %.1f / %.1f / %.1f, depth %.1f / %.1f / %.1f %s (chest / waist / hips)"), Width[0] * K, Width[1] * K, Width[2] * K, Depth[0] * K,
		Depth[1] * K, Depth[2] * K, bShareOfHeight ? TEXT("% of height") : TEXT("cm"));
}

FString FMantleSliderSeed::ToString() const
{
	return FString::Printf(TEXT("Weight %+.0f%%, Fat %+.0f%%, Muscularity %+.0f%% of range"), Weight * 100.0, Fat * 100.0, Muscularity * 100.0);
}

bool FMantleBodySilhouette::Measure(const TConstArrayView<FVector3f> Vertices, const double FeetZ, const double HeightCm, FMantleSilhouette& Out)
{
	Out = FMantleSilhouette();
	if (HeightCm <= 0.0 || Vertices.Num() == 0)
	{
		return false;
	}
	const double Gap = GapShare * HeightCm;
	for (int32 Band = 0; Band < FMantleSilhouette::Bands; ++Band)
	{
		double BestWidth = TNumericLimits<double>::Max();
		double BestDepth = 0.0;
		for (int32 Slab = -1; Slab <= 1; ++Slab)
		{
			const double Level = FeetZ + (BandHeightShare[Band] + Slab * SlabHalfShare) * HeightCm;
			const double Half = SlabHalfShare * HeightCm * 0.5;
			TArray<FVector2d> Points;
			for (const FVector3f& Vertex : Vertices)
			{
				if (FMath::Abs(Vertex.Z - Level) <= Half)
				{
					Points.Add(FVector2d(Vertex.X, Vertex.Y));
				}
			}
			if (Points.Num() < 3)
			{
				continue;
			}
			Points.Sort([](const FVector2d& A, const FVector2d& B) { return A.X < B.X; });
			// The torso is the run of vertices, across the slab, that holds the middle one; arms and capes stand apart from it by a gap.
			const int32 Middle = Points.Num() / 2;
			int32 First = Middle;
			int32 Last = Middle;
			while (First > 0 && Points[First].X - Points[First - 1].X <= Gap)
			{
				--First;
			}
			while (Last < Points.Num() - 1 && Points[Last + 1].X - Points[Last].X <= Gap)
			{
				++Last;
			}
			double MinY = TNumericLimits<double>::Max();
			double MaxY = -TNumericLimits<double>::Max();
			for (int32 Index = First; Index <= Last; ++Index)
			{
				MinY = FMath::Min(MinY, Points[Index].Y);
				MaxY = FMath::Max(MaxY, Points[Index].Y);
			}
			const double Width = Points[Last].X - Points[First].X;
			if (Width < BestWidth)
			{
				BestWidth = Width;
				BestDepth = MaxY - MinY;
			}
		}
		if (BestWidth == TNumericLimits<double>::Max() || BestWidth <= 0.0 || BestDepth <= 0.0)
		{
			return false;
		}
		Out.Width[Band] = BestWidth;
		Out.Depth[Band] = BestDepth;
	}
	Out.bValid = true;
	return true;
}

FMantleSilhouette FMantleBodySilhouette::Normalised(const FMantleSilhouette& Silhouette, const double HeightCm)
{
	FMantleSilhouette Out = Silhouette;
	if (HeightCm <= 0.0)
	{
		Out.bValid = false;
		return Out;
	}
	for (int32 Band = 0; Band < FMantleSilhouette::Bands; ++Band)
	{
		Out.Width[Band] /= HeightCm;
		Out.Depth[Band] /= HeightCm;
	}
	return Out;
}

FMantleSliderSeed FMantleBodySilhouette::Seed(const FMantleSilhouette& Source, const FMantleSilhouette& Default)
{
	FMantleSliderSeed Out;
	if (!Source.bValid || !Default.bValid)
	{
		return Out;
	}
	auto Ratio = [&](const int32 Band)
	{
		return 0.5 * (Source.Width[Band] / Default.Width[Band] + Source.Depth[Band] / Default.Depth[Band]);
	};
	auto Move = [](const double Ratio)
	{
		return FMath::Clamp((Ratio - 1.0) * SeedGain, -SeedLimit, SeedLimit);
	};
	const double Chest = Ratio(FMantleSilhouette::Chest);
	const double Waist = Ratio(FMantleSilhouette::Waist);
	const double Hips = Ratio(FMantleSilhouette::Hips);
	Out.Weight = Move((Chest + Waist + Hips) / 3.0);
	Out.Fat = Move((Waist + Hips) * 0.5);
	Out.Muscularity = Move(Chest);
	return Out;
}
