// Copyright Joshua Eger. All Rights Reserved.
#include "MantleEnvironmentExport.h"

#include "MantleClientFeatures.h"
#include "MantleCoverage.h"
#include "MantleEnvironment.h"
#include "MantleEnvironmentDefaults.h"
#include "MantleEnvironmentSubsystem.h"
#include "MantlePlaceSettingsActor.h"
#include "MantleSkyClock.h"
#include "MantleTargetFeatures.h"
#include "MantleWeather.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Scene.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleEnvironmentExportPrivate
{
	namespace Cat = MantleCoverageCategory;
	using EOutcome = EMantleCoverageOutcome;

	/** Below this elevation the level's sun is a sunrise or sunset, which the clock's own day and night own. */
	constexpr double MinSunElevationDegrees = 1.0;

	/** The post-process white balance that changes nothing, in Kelvin. */
	constexpr double NeutralWhiteKelvin = 6500.0;

	/** The cloud material's coverage when it has none to read: the engine's default cloud material's. */
	constexpr double DefaultCloudCoverage = -0.2;

	/** The names a bucket lists, at most this many a sentence. */
	constexpr int32 MaxNamesListed = 6;

	/** A value written to the place, rounded (to thousandths by default) so a float's own digits are not written out. */
	double Rounded(const double Value, const double Scale = 1000.0)
	{
		return FMath::RoundToDouble(Value * Scale) / Scale;
	}

	/** A fog value differs from Mantle's own fog's (the component's default) by more than rounding. */
	bool DiffersFromDefault(const double Value, const double Default)
	{
		return !FMath::IsNearlyEqual(Value, Default, 1.0e-6);
	}

	bool ColorDiffersFromDefault(const FLinearColor& Value, const FLinearColor& Default)
	{
		return DiffersFromDefault(Value.R, Default.R) || DiffersFromDefault(Value.G, Default.G) || DiffersFromDefault(Value.B, Default.B);
	}

	FLinearColor ClampedLuminance(const FLinearColor& Value, bool& bOutClamped)
	{
		const double Max = MantleEnvironmentDefaults::MaxFogColor;
		bOutClamped = Value.R > Max || Value.G > Max || Value.B > Max || Value.R < 0.0 || Value.G < 0.0 || Value.B < 0.0;
		return FLinearColor(
			static_cast<float>(Rounded(FMath::Clamp<double>(Value.R, 0.0, Max), 10000.0)),
			static_cast<float>(Rounded(FMath::Clamp<double>(Value.G, 0.0, Max), 10000.0)),
			static_cast<float>(Rounded(FMath::Clamp<double>(Value.B, 0.0, Max), 10000.0)),
			1.0f);
	}

	FString LabelOf(const AActor* Actor)
	{
		return Actor->GetActorLabel();
	}

	FString JoinNames(const TArray<FString>& Names)
	{
		TArray<FString> Shown;
		for (int32 Index = 0; Index < FMath::Min(Names.Num(), MaxNamesListed); ++Index)
		{
			Shown.Add(Names[Index]);
		}
		FString Out = FString::Join(Shown, TEXT(", "));
		if (Names.Num() > MaxNamesListed)
		{
			Out += FString::Printf(TEXT(" and %d more"), Names.Num() - MaxNamesListed);
		}
		return Out;
	}

	FString HourText(const double Hour)
	{
		const int32 Minutes = FMath::RoundToInt32(Hour * 60.0);
		return FString::Printf(TEXT("%d:%02d"), (Minutes / 60) % 24, Minutes % 60);
	}

	uint32 PackRgb(const FColor& Color)
	{
		return (static_cast<uint32>(Color.R) << 16) | (static_cast<uint32>(Color.G) << 8) | Color.B;
	}

	/**
	 * Reports one environment actor by what became of it. Nothing carried and nothing tuned is Kept (Mantle's own
	 * matches), something carried is Approximated, and a tuned actor nothing of which carried is Dropped. Notes says what
	 * did and did not travel, in plain words, after the reason Mantle's own stands in.
	 */
	void ReportLook(FMantleCoverage& Coverage, const TCHAR* Category, const FString& Label, const bool bCarried,
		const FString& Carried, const TArray<FString>& NotCarried, const TCHAR* MantleUses, const FString& Extra = FString())
	{
		TArray<FString> Sentences;
		if (!Extra.IsEmpty())
		{
			Sentences.Add(Extra);
		}
		if (bCarried && !Carried.IsEmpty())
		{
			Sentences.Add(FString::Printf(TEXT("carried: %s"), *Carried));
		}
		if (NotCarried.Num() > 0)
		{
			Sentences.Add(FString::Printf(TEXT("not carried: %s"), *JoinNames(NotCarried)));
		}
		const EOutcome Outcome = bCarried ? EOutcome::Approximated
			: NotCarried.Num() > 0 ? EOutcome::Dropped : EOutcome::Kept;
		FString Detail;
		if (Outcome == EOutcome::Kept)
		{
			Detail = FString::Printf(TEXT("%s; this one is at its defaults, so nothing differs."), MantleUses);
		}
		else
		{
			Detail = FString::Printf(TEXT("%s; %s."), MantleUses, *FString::Join(Sentences, TEXT("; ")));
		}
		Coverage.Add(Category, Outcome, Label, Detail);
	}

	/** The material the cloud draws with, loading it when the editor has not. */
	UMaterialInterface* CloudMaterialOf(const UVolumetricCloudComponent* Cloud)
	{
		if (!Cloud)
		{
			return nullptr;
		}
		if (UMaterialInterface* Loaded = Cloud->GetMaterial())
		{
			return Loaded;
		}
		return Cloud->Material.IsNull() ? nullptr : Cloud->Material.LoadSynchronous();
	}

	/** The place's saturation from a post-process saturation vector: the global grade's master over its colour channels. */
	double SaturationOf(const FVector4& Grade)
	{
		return Grade.W * (Grade.X + Grade.Y + Grade.Z) / 3.0;
	}

	/** Takes a setting off the list of those still unaccounted for: it was carried, or says why it was not. */
	void TakeOverride(TArray<FString>& Overridden, const TCHAR* Raw)
	{
		Overridden.Remove(Raw);
	}

	TArray<FString> DisplayNames(const TArray<FString>& Raw)
	{
		TArray<FString> Out;
		for (const FString& Name : Raw)
		{
			Out.Add(FName::NameToDisplayString(Name, /*bIsBool*/ false));
		}
		return Out;
	}

	/** The level's first sun: its entity's colour takes the light's temperature, and its angle becomes the place's clock. */
	void MapSun(const TMap<const AActor*, FString>& IdByActor, FMantlePlace& Place, FMantleCoverage& Coverage, const bool bReadsEnvironment)
	{
		FMantleEntity* Sun = nullptr;
		for (FMantleEntity& Entity : Place.Entities)
		{
			if (Entity.Type == EMantleEntityType::DirectionalLight)
			{
				Sun = &Entity;
				break;
			}
		}
		if (!Sun)
		{
			return;
		}
		const AActor* SunActor = nullptr;
		for (const TPair<const AActor*, FString>& Pair : IdByActor)
		{
			if (Pair.Value == Sun->Id)
			{
				SunActor = Pair.Key;
				break;
			}
		}
		const ADirectionalLight* Light = Cast<ADirectionalLight>(SunActor);
		const UDirectionalLightComponent* Component = Light ? Cast<UDirectionalLightComponent>(Light->GetLightComponent()) : nullptr;

		TArray<FString> Carried = { TEXT("intensity"), TEXT("colour") };
		FMantleDirectionalLightProps* Props = Sun->Props.TryGet<FMantleDirectionalLightProps>();
		if (Props && Component && Component->bUseTemperature)
		{
			// The place has no temperature for a directional light, so it goes into the colour it is multiplied with.
			const FLinearColor Tint = FLinearColor::MakeFromColorTemperature(Component->Temperature);
			Props->Color = FLinearColor(
				FMath::Clamp(Props->Color.R * Tint.R, 0.0f, 1.0f),
				FMath::Clamp(Props->Color.G * Tint.G, 0.0f, 1.0f),
				FMath::Clamp(Props->Color.B * Tint.B, 0.0f, 1.0f),
				Props->Color.A);
			Carried.Add(FString::Printf(TEXT("temperature (%.0f K, baked into the colour)"), Component->Temperature));
		}

		FMantleEnvironmentSettings& Environment = Place.Settings.Environment;
		FString Angle;
		if (!Light || !Sun->Parent.IsEmpty())
		{
			Angle = TEXT("the sun's pitch is not carried: Mantle's sky clock sets it");
		}
		else if (Environment.ClockTime.IsSet() || Environment.TimeScale.IsSet())
		{
			Angle = TEXT("the sun's angle comes from the clock Mantle Place Settings sets, not from this light's pitch");
		}
		else
		{
			FMantleSunClock Clock;
			if (!FMantleEnvironmentExport::SolveSunClock(-Light->GetActorForwardVector(), Light->GetActorRotation().Yaw, Clock))
			{
				Angle = TEXT("the sun is at or below the horizon, so its pitch is not carried: Mantle's sky clock sets it");
			}
			else if (!bReadsEnvironment)
			{
				Angle = FString::Printf(TEXT("the sun's angle is not carried: %s"), FMantleTargetFeatures::NeedsNewerClient);
			}
			else
			{
				Environment.ClockTime = FMath::RoundToDouble(Clock.Hour * 1000.0) / 1000.0;
				Environment.TimeScale = 0.0;
				// The client reads the sun's yaw as its heading, which turns the clock's sun to the level's side of the sky.
				FRotator Rotation = Sun->Transform.Rotator();
				Rotation.Yaw = Clock.HeadingYaw;
				Sun->Transform.SetRotation(Rotation.Quaternion());
				Angle = FString::Printf(
					TEXT("the sun's angle (%.0f degrees up) is carried as the place clock, stopped at %s with the heading turned to %.0f degrees, so the day and night cycle is off; "
						"set Clock Time in Mantle Place Settings to run it from an hour you choose"),
					Clock.ElevationDegrees, *HourText(Clock.Hour), Clock.HeadingYaw);
				if (Clock.bElevationClamped)
				{
					Angle += FString::Printf(TEXT(" (the clock's sun peaks at %.0f degrees, lower than this one)"), MantleEnvironmentDefaults::PeakElevationDegrees);
				}
			}
		}
		Coverage.Add(Cat::Lights, EOutcome::Approximated, Sun->Name,
			FString::Printf(TEXT("Mantle drives the sun's angle; its %s are kept, and %s."), *FString::Join(Carried, TEXT(", ")), *Angle));
	}

	/** The volume with the highest priority wins, an unbound one before a bounded one. */
	bool Outranks(const APostProcessVolume* Candidate, const APostProcessVolume* Current)
	{
		if (!Current)
		{
			return true;
		}
		if (Candidate->Priority != Current->Priority)
		{
			return Candidate->Priority > Current->Priority;
		}
		return Candidate->bUnbound && !Current->bUnbound;
	}

	/** The look of the level's post-process volume as the place's saturation, bloom, colour tint and exposure. */
	void MapPostProcess(const TArray<const APostProcessVolume*>& Volumes, FMantleEnvironmentSettings& Environment, FMantleCoverage& Coverage,
		const bool bReadsEnvironment, const bool bHasEnvironment, const FMantleExposureProject& Project)
	{
		const APostProcessVolume* Primary = nullptr;
		for (const APostProcessVolume* Volume : Volumes)
		{
			if (Volume->bEnabled && Outranks(Volume, Primary))
			{
				Primary = Volume;
			}
		}

		for (const APostProcessVolume* Volume : Volumes)
		{
			if (Volume == Primary)
			{
				continue;
			}
			if (!Volume->bEnabled)
			{
				Coverage.Add(Cat::PostProcess, EOutcome::Info, LabelOf(Volume), TEXT("is disabled in the level, so it changes nothing."));
			}
			else
			{
				Coverage.Add(Cat::PostProcess, EOutcome::Dropped, LabelOf(Volume),
					FString::Printf(TEXT("another volume (%s) outranks it; the place carries one volume's look."), *LabelOf(Primary)));
			}
		}
		if (!Primary)
		{
			// With no volume the editor shows the project's default exposure, which is an auto one unless the project turned it off.
			FMantleExposureSettings Exposure;
			TArray<FString> ExposureCarried;
			TArray<FString> ExposureNotes;
			if (bHasEnvironment && bReadsEnvironment && !Environment.Exposure.IsSet()
				&& FMantleEnvironmentExport::ExposureFor(nullptr, Project, Exposure, ExposureCarried, ExposureNotes))
			{
				Environment.Exposure = Exposure;
				Coverage.Add(Cat::PostProcess, EOutcome::Approximated, TEXT("Exposure"),
					FString::Printf(TEXT("the level has no post-process volume, so the editor's default exposure is carried as the place's; carried: %s."),
						*FString::Join(ExposureCarried, TEXT(", "))));
			}
			return;
		}

		const FPostProcessSettings& Settings = Primary->Settings;
		TArray<FString> Overridden = FMantleEnvironmentExport::OverriddenSettings(Settings);
		FMantleExposureSettings Exposure;
		TArray<FString> ExposureCarried;
		TArray<FString> ExposureNotes;
		const bool bExposure = FMantleEnvironmentExport::ExposureFor(&Settings, Project, Exposure, ExposureCarried, ExposureNotes);
		if (Overridden.Num() == 0 && !bExposure)
		{
			Coverage.Add(Cat::PostProcess, EOutcome::Info, LabelOf(Primary), TEXT("overrides no setting, so it changes nothing."));
			return;
		}

		TArray<FString> Carried;
		TArray<FString> Notes;
		auto Explicit = [&Notes](const TCHAR* What)
		{
			Notes.Add(FString::Printf(TEXT("%s (Mantle Place Settings sets its own, which wins)"), What));
		};

		// The exposure settings ExposureFor accounts for, whatever it says of them.
		for (const TCHAR* Raw : { TEXT("AutoExposureMethod"), TEXT("AutoExposureBias"), TEXT("AutoExposureMinBrightness"), TEXT("AutoExposureMaxBrightness") })
		{
			TakeOverride(Overridden, Raw);
		}
		Notes.Append(ExposureNotes);
		if (bExposure)
		{
			if (Environment.Exposure.IsSet())
			{
				Explicit(TEXT("Exposure"));
			}
			else if (!bReadsEnvironment)
			{
				Notes.Add(FString::Printf(TEXT("Exposure (%s)"), FMantleTargetFeatures::NeedsNewerClient));
			}
			else
			{
				Environment.Exposure = Exposure;
				Carried.Append(ExposureCarried);
			}
		}

		if (Settings.bOverride_ColorSaturation)
		{
			const double Saturation = FMath::Clamp(SaturationOf(Settings.ColorSaturation), 0.0, MantleEnvironmentDefaults::MaxScriptSaturation);
			TakeOverride(Overridden, TEXT("ColorSaturation"));
			if (Environment.Saturation.IsSet())
			{
				Explicit(TEXT("Saturation"));
			}
			else if (!bReadsEnvironment)
			{
				Notes.Add(FString::Printf(TEXT("Saturation (%s)"), FMantleTargetFeatures::NeedsNewerClient));
			}
			else
			{
				if (!FMath::IsNearlyEqual(Saturation, 1.0, 0.005))
				{
					Environment.Saturation = FMath::RoundToDouble(Saturation * 1000.0) / 1000.0;
					Carried.Add(FString::Printf(TEXT("saturation (%.2f)"), Saturation));
				}
			}
		}
		if (Settings.bOverride_BloomIntensity)
		{
			TakeOverride(Overridden, TEXT("BloomIntensity"));
			if (Environment.Bloom.IsSet())
			{
				Explicit(TEXT("Bloom"));
			}
			else if (!bReadsEnvironment)
			{
				Notes.Add(FString::Printf(TEXT("Bloom (%s)"), FMantleTargetFeatures::NeedsNewerClient));
			}
			else
			{
				Environment.Bloom = FMath::Clamp(static_cast<double>(Settings.BloomIntensity), 0.0, MantleEnvironmentDefaults::MaxScriptBloom);
				Carried.Add(FString::Printf(TEXT("bloom (%.2f)"), *Environment.Bloom));
			}
		}
		if (Settings.bOverride_WhiteTemp)
		{
			TakeOverride(Overridden, TEXT("WhiteTemp"));
			if (Environment.ColorTint.IsSet())
			{
				Explicit(TEXT("Colour Tint"));
			}
			else if (!bReadsEnvironment)
			{
				Notes.Add(FString::Printf(TEXT("White Temp (%s)"), FMantleTargetFeatures::NeedsNewerClient));
			}
			else
			{
				const uint32 Tint = FMantleEnvironmentExport::TintForWhiteBalance(Settings.WhiteTemp);
				if (Tint != 0xFFFFFF)
				{
					Environment.ColorTint = Tint;
					Carried.Add(FString::Printf(TEXT("white balance (%.0f K, as a colour tint)"), Settings.WhiteTemp));
				}
			}
		}

		TArray<FString> NotCarried = Notes;
		NotCarried.Append(DisplayNames(Overridden));
		FString Extra = Primary->bUnbound ? FString() : FString(TEXT("a bounded volume, so what is carried applies everywhere"));
		ReportLook(Coverage, Cat::PostProcess, LabelOf(Primary), Carried.Num() > 0, FString::Join(Carried, TEXT(", ")), NotCarried,
			TEXT("Mantle's own post-process is used (the place carries only a volume's saturation, bloom, white balance and exposure)"), Extra);
	}

	/**
	 * The level's first height fog as the place's fog: each of its density, height falloff, inscattering colour, start distance
	 * and directional inscattering colour that differs from Mantle's own fog's (the component's default). The engine draws
	 * only the first fog, so a later one is reported as not used. What the place has no key for is listed as not carried.
	 */
	void MapFog(const TArray<const AExponentialHeightFog*>& Fogs, FMantleEnvironmentSettings& Environment, FMantleCoverage& Coverage,
		const bool bReadsEnvironment)
	{
		// Mantle spawns the plain actor class, so its fog is the actor's archetype's component.
		const AExponentialHeightFog* DefaultFog = AExponentialHeightFog::StaticClass()->GetDefaultObject<AExponentialHeightFog>();
		const UExponentialHeightFogComponent* Default = DefaultFog ? DefaultFog->GetComponent() : GetDefault<UExponentialHeightFogComponent>();
		for (int32 Index = 0; Index < Fogs.Num(); ++Index)
		{
			const AExponentialHeightFog* Fog = Fogs[Index];
			const UExponentialHeightFogComponent* Component = Fog->GetComponent();
			if (Index > 0)
			{
				ReportLook(Coverage, Cat::Fog, LabelOf(Fog), false, FString(), FMantleEnvironmentExport::TunedProperties(Component),
					TEXT("Mantle supplies its own fog"),
					FString::Printf(TEXT("the engine draws only the level's first fog (%s), so the place carries that one's look"), *LabelOf(Fogs[0])));
				continue;
			}

			const bool bSetByPlace = Environment.Fog.IsSet();
			FMantleFogSettings Mapped;
			TArray<FString> Carried;
			TArray<FString> Notes;
			TArray<FName> CarriedProperties;
			if (Component && !bSetByPlace && bReadsEnvironment)
			{
				using namespace MantleEnvironmentDefaults;
				if (DiffersFromDefault(Component->FogDensity, Default->FogDensity))
				{
					const double Density = FMath::Clamp<double>(Component->FogDensity, 0.0, MaxFogDensity);
					Mapped.Density = Rounded(Density, 100000.0);
					Carried.Add(FString::Printf(TEXT("density (%g)"), *Mapped.Density));
					CarriedProperties.Add(FName(TEXT("FogDensity")));
					if (Density != static_cast<double>(Component->FogDensity))
					{
						Notes.Add(FString::Printf(TEXT("Fog Density (%g is past the place's largest, %g)"), Component->FogDensity, MaxFogDensity));
					}
				}
				if (DiffersFromDefault(Component->FogHeightFalloff, Default->FogHeightFalloff))
				{
					Mapped.HeightFalloff = Rounded(FMath::Clamp<double>(Component->FogHeightFalloff, MinFogHeightFalloff, MaxFogHeightFalloff), 100000.0);
					Carried.Add(FString::Printf(TEXT("height falloff (%g)"), *Mapped.HeightFalloff));
					CarriedProperties.Add(FName(TEXT("FogHeightFalloff")));
				}
				if (ColorDiffersFromDefault(Component->FogInscatteringLuminance, Default->FogInscatteringLuminance))
				{
					bool bClamped = false;
					Mapped.InscatteringColor = ClampedLuminance(Component->FogInscatteringLuminance, bClamped);
					Carried.Add(TEXT("inscattering colour"));
					CarriedProperties.Add(FName(TEXT("FogInscatteringLuminance")));
					if (bClamped)
					{
						Notes.Add(TEXT("Fog Inscattering Luminance (past the place's range, so clamped to it)"));
					}
				}
				if (DiffersFromDefault(Component->StartDistance, Default->StartDistance))
				{
					Mapped.StartDistance = Rounded(FMath::Clamp<double>(Component->StartDistance, 0.0, MaxFogStartDistance), 100.0);
					Carried.Add(FString::Printf(TEXT("start distance (%g cm)"), *Mapped.StartDistance));
					CarriedProperties.Add(FName(TEXT("StartDistance")));
				}
				if (ColorDiffersFromDefault(Component->DirectionalInscatteringLuminance, Default->DirectionalInscatteringLuminance))
				{
					bool bClamped = false;
					Mapped.DirectionalInscatteringColor = ClampedLuminance(Component->DirectionalInscatteringLuminance, bClamped);
					Carried.Add(TEXT("directional inscattering colour"));
					CarriedProperties.Add(FName(TEXT("DirectionalInscatteringLuminance")));
					if (bClamped)
					{
						Notes.Add(TEXT("Directional Inscattering Color (past the place's range, so clamped to it)"));
					}
				}
			}

			TArray<FString> NotCarried = Notes;
			if (bSetByPlace)
			{
				NotCarried.Add(TEXT("Fog (Mantle Place Settings sets its own, which wins)"));
			}
			else if (!bReadsEnvironment)
			{
				NotCarried.Add(FString::Printf(TEXT("Fog (%s)"), FMantleTargetFeatures::NeedsNewerClient));
			}
			// Where the fog sits in the level is the actor's, and Mantle's own fog sits at the origin.
			if (Fog->GetActorLocation().Z != 0.0)
			{
				NotCarried.Add(FString::Printf(TEXT("the fog's height in the level (%.0f cm up; the place's fog is at ground level)"), Fog->GetActorLocation().Z));
			}
			NotCarried.Append(FMantleEnvironmentExport::TunedProperties(Component, CarriedProperties));

			const bool bCarried = !Mapped.IsEmpty();
			if (bCarried)
			{
				Environment.Fog = Mapped;
			}
			ReportLook(Coverage, Cat::Fog, LabelOf(Fog), bCarried, FString::Join(Carried, TEXT(", ")), NotCarried,
				TEXT("Mantle's own fog is used for the rest (the place carries a fog's density, height falloff, colours and start distance)"));
		}
	}
}

bool FMantleEnvironmentExport::IsReported(const AActor* Actor)
{
	if (!Actor || !IsValid(Actor) || Actor->HasAnyFlags(RF_Transient) || Actor->ActorHasTag(FName(FMantleEnvironment::EnvironmentActorTag)))
	{
		return false;
	}
	return Actor->IsA(ASkyAtmosphere::StaticClass())
		|| Actor->IsA(ASkyLight::StaticClass())
		|| Actor->IsA(AVolumetricCloud::StaticClass())
		|| Actor->IsA(AExponentialHeightFog::StaticClass())
		|| Actor->IsA(APostProcessVolume::StaticClass());
}

bool FMantleEnvironmentExport::SolveSunClock(const FVector& TowardSun, const double AuthoredYaw, FMantleSunClock& Out)
{
	using namespace MantleEnvironmentExportPrivate;
	const FVector Direction = TowardSun.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return false;
	}
	const double Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Direction.Z, -1.0, 1.0)));
	if (Elevation < MinSunElevationDegrees)
	{
		return false;
	}

	const double Peak = MantleEnvironmentDefaults::PeakElevationDegrees;
	const double Used = FMath::Min(Elevation, Peak);
	// The clock's sun is at sin(elevation) = sin(arc) * sin(peak), the arc running 0 at sunrise to 180 degrees at sunset
	// over 12 hours.
	const double SinRatio = FMath::Clamp(FMath::Sin(FMath::DegreesToRadians(Used)) / FMath::Sin(FMath::DegreesToRadians(Peak)), 0.0, 1.0);
	const double HoursAfterSunrise = FMath::Asin(SinRatio) / UE_DOUBLE_PI * 12.0;
	const double Sunrise = MantleEnvironmentDefaults::SunriseHour;
	const double Candidates[2] = { Sunrise + HoursAfterSunrise, Sunrise + 12.0 - HoursAfterSunrise };

	const bool bHasSide = FVector2D(Direction.X, Direction.Y).SizeSquared() > 1.0e-8;
	const double TargetAzimuth = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	bool bChosen = false;
	for (const double Hour : Candidates)
	{
		double Heading = AuthoredYaw;
		if (bHasSide)
		{
			// The whole sun path turns about the vertical with the heading, so the heading is the difference of azimuths.
			const FVector AtZero = FMantleSkyClock::SunDirection(Hour, 0.0);
			Heading = FMath::UnwindDegrees(TargetAzimuth - FMath::RadiansToDegrees(FMath::Atan2(AtZero.Y, AtZero.X)));
		}
		// The morning is tried first and the afternoon replaces it only when its heading is clearly nearer the light's own
		// yaw (by over a thousandth of a degree). For a light whose yaw is the sun's azimuth the two are exactly as near,
		// so it is the morning, never a rounding error's choice.
		if (!bChosen || FMath::Abs(FMath::UnwindDegrees(Heading - AuthoredYaw)) < FMath::Abs(FMath::UnwindDegrees(Out.HeadingYaw - AuthoredYaw)) - 1.0e-3)
		{
			Out.Hour = FMath::RoundToDouble(Hour * 1000.0) / 1000.0;
			Out.HeadingYaw = Heading;
			bChosen = true;
		}
	}
	Out.bElevationClamped = Elevation > Peak;
	Out.ElevationDegrees = Elevation;
	return true;
}

FMantleExposureProject FMantleExposureProject::Current()
{
	FMantleExposureProject Project;
	if (const IConsoleVariable* Auto = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AutoExposure")))
	{
		Project.bAuto = Auto->GetInt() != 0;
	}
	if (const IConsoleVariable* Extended = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange")))
	{
		Project.bExtendedRange = Extended->GetInt() != 0;
	}
	Project.EngineBias = UMantleEnvironmentSubsystem::GetEngineExposureBias();
	return Project;
}

bool FMantleEnvironmentExport::ExposureFor(const FPostProcessSettings* Settings, const FMantleExposureProject& Project, FMantleExposureSettings& Out,
	TArray<FString>& Carried, TArray<FString>& Notes)
{
	using namespace MantleEnvironmentExportPrivate;
	using namespace MantleEnvironmentDefaults;
	Out = FMantleExposureSettings();

	bool bAuto = Project.bAuto;
	FString Metering;
	// Manual metering exposes by the camera: compensation / (0.18 x 2^EV100), the EV100 from the volume's f-stop, shutter
	// speed and ISO when Apply Physical Camera Exposure is on, and its compensation calibrated to a white point of 1, not
	// middle grey (PostProcessEyeAdaptation.cpp). A place's fixed exposure is the compensation alone (white point 1, middle
	// grey), so the same picture is the compensation plus log2(1 / 0.18) less the camera's EV100.
	TOptional<double> ManualStops;
	if (Settings && Settings->bOverride_AutoExposureMethod)
	{
		bAuto = Settings->AutoExposureMethod != EAutoExposureMethod::AEM_Manual;
		if (!bAuto)
		{
			static const FPostProcessSettings Defaults;
			const bool bPhysical = Settings->bOverride_AutoExposureApplyPhysicalCameraExposure
				? Settings->AutoExposureApplyPhysicalCameraExposure != 0
				: Defaults.AutoExposureApplyPhysicalCameraExposure != 0;
			const double Fstop = Settings->bOverride_DepthOfFieldFstop ? Settings->DepthOfFieldFstop : Defaults.DepthOfFieldFstop;
			const double Shutter = Settings->bOverride_CameraShutterSpeed ? Settings->CameraShutterSpeed : Defaults.CameraShutterSpeed;
			const double Iso = Settings->bOverride_CameraISO ? Settings->CameraISO : Defaults.CameraISO;
			const double CameraEV100 = bPhysical ? FMath::Log2(FMath::Square(Fstop) * Shutter * 100.0 / FMath::Max(1.0, Iso)) : 0.0;
			ManualStops = FMath::Log2(1.0 / 0.18) - CameraEV100;
			Metering = bPhysical
				? FString::Printf(TEXT("manual metering at the camera's EV100 %.1f (f/%g, 1/%g s, ISO %g) folded into the bias"), CameraEV100, Fstop, Shutter, Iso)
				: FString(TEXT("manual metering folded into the bias"));
		}
		else if (Settings->AutoExposureMethod == EAutoExposureMethod::AEM_Basic)
		{
			Metering = TEXT("basic metering as histogram");
		}
	}
	Out.Mode = bAuto ? EMantleExposureMode::Auto : EMantleExposureMode::Fixed;

	TArray<FString> Parts;
	Parts.Add(bAuto ? TEXT("auto") : TEXT("fixed"));
	if (!Metering.IsEmpty())
	{
		Parts.Add(Metering);
	}
	if (Settings && (Settings->bOverride_AutoExposureBias || ManualStops.IsSet()))
	{
		// A volume's compensation replaces the project's, and the place's bias is relative to the project's own.
		const double Compensation = Settings->bOverride_AutoExposureBias ? static_cast<double>(Settings->AutoExposureBias) : Project.EngineBias;
		const double Relative = Compensation + ManualStops.Get(0.0) - Project.EngineBias;
		Out.Bias = Rounded(FMath::Clamp(Relative, -MaxExposureBias, MaxExposureBias));
		if (Out.Bias != Rounded(Relative))
		{
			Notes.Add(FString::Printf(TEXT("Exposure Compensation (%g stops over the game's is past the place's range of %g, so clamped to it)"), Relative, MaxExposureBias));
		}
		if (Out.Bias != 0.0)
		{
			Parts.Add(FString::Printf(TEXT("bias %+g stops"), Out.Bias));
		}
	}
	if (bAuto && Settings && (Settings->bOverride_AutoExposureMinBrightness || Settings->bOverride_AutoExposureMaxBrightness))
	{
		if (Project.bExtendedRange)
		{
			Notes.Add(TEXT("Min and Max Brightness (the project counts them in EV100, which the place does not carry; Mantle's own range is used)"));
		}
		else
		{
			if (Settings->bOverride_AutoExposureMinBrightness)
			{
				Out.MinBrightness = Rounded(FMath::Clamp<double>(Settings->AutoExposureMinBrightness, MinExposureBrightness, MaxExposureBrightness));
				Parts.Add(FString::Printf(TEXT("min brightness %g"), *Out.MinBrightness));
			}
			if (Settings->bOverride_AutoExposureMaxBrightness)
			{
				Out.MaxBrightness = Rounded(FMath::Clamp<double>(Settings->AutoExposureMaxBrightness, MinExposureBrightness, MaxExposureBrightness));
				Parts.Add(FString::Printf(TEXT("max brightness %g"), *Out.MaxBrightness));
			}
			if (Out.MinBrightness.IsSet() && Out.MaxBrightness.IsSet() && *Out.MinBrightness > *Out.MaxBrightness)
			{
				Out.MaxBrightness = Out.MinBrightness;
			}
		}
	}

	if (!bAuto && Out.Bias == 0.0)
	{
		return false;
	}
	Carried.Add(FString::Printf(TEXT("exposure (%s)"), *FString::Join(Parts, TEXT(", "))));
	return true;
}

EMantleWeatherState FMantleEnvironmentExport::WeatherForCloudCoverage(const double CloudGlobalCoverage)
{
	using namespace MantleEnvironmentDefaults;
	const double Cover = FMath::Clamp((CloudGlobalCoverage - CloudCoverageAtNoCover) / (CloudCoverageAtFullCover - CloudCoverageAtNoCover), 0.0, 1.0);
	const EMantleWeatherState States[3] = { EMantleWeatherState::Clear, EMantleWeatherState::PartlyCloudy, EMantleWeatherState::Overcast };
	EMantleWeatherState Nearest = States[0];
	double NearestDistance = TNumericLimits<double>::Max();
	for (const EMantleWeatherState State : States)
	{
		const double Distance = FMath::Abs(FMantleWeatherSequence::ValuesFor(State).CloudCover - Cover);
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = State;
		}
	}
	return Nearest;
}

uint32 FMantleEnvironmentExport::TintForWhiteBalance(const double Kelvin)
{
	using namespace MantleEnvironmentExportPrivate;
	if (!FMath::IsFinite(Kelvin) || FMath::Abs(Kelvin - NeutralWhiteKelvin) < 1.0)
	{
		return 0xFFFFFF;
	}
	// The picture is corrected for a light of Kelvin: a light bluer than the neutral one is divided out, which warms it.
	const FLinearColor Neutral = FLinearColor::MakeFromColorTemperature(static_cast<float>(NeutralWhiteKelvin));
	const FLinearColor Light = FLinearColor::MakeFromColorTemperature(static_cast<float>(FMath::Clamp(Kelvin, 1500.0, 15000.0)));
	FLinearColor Gain(
		Neutral.R / FMath::Max(Light.R, 0.01f),
		Neutral.G / FMath::Max(Light.G, 0.01f),
		Neutral.B / FMath::Max(Light.B, 0.01f),
		1.0f);
	const float Brightest = FMath::Max3(Gain.R, Gain.G, Gain.B);
	if (Brightest <= 0.0f)
	{
		return 0xFFFFFF;
	}
	Gain.R /= Brightest;
	Gain.G /= Brightest;
	Gain.B /= Brightest;
	return PackRgb(Gain.ToFColor(/*bSRGB*/ true));
}

TArray<FString> FMantleEnvironmentExport::TunedProperties(const UObject* Object, const TArray<FName>& IgnoredNames)
{
	TArray<FString> Out;
	if (!Object)
	{
		return Out;
	}
	const UObject* Archetype = Object->GetArchetype();
	if (!Archetype || Archetype == Object || Archetype->GetClass() != Object->GetClass())
	{
		return Out;
	}
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
		{
			continue;
		}
		// Where a component is, whether it shows and how it moves are the actor's, not the look's.
		const UClass* Owner = Property->GetOwnerClass();
		if (Owner && USceneComponent::StaticClass()->IsChildOf(Owner))
		{
			continue;
		}
		if (IgnoredNames.Contains(Property->GetFName()))
		{
			continue;
		}
		bool bSame = true;
		for (int32 Index = 0; Index < Property->ArrayDim && bSame; ++Index)
		{
			bSame = Property->Identical_InContainer(Object, Archetype, Index, PPF_None);
		}
		if (!bSame)
		{
			Out.Add(Property->GetDisplayNameText().ToString());
		}
	}
	return Out;
}

TArray<FString> FMantleEnvironmentExport::OverriddenSettings(const FPostProcessSettings& Settings)
{
	static const FString Prefix(TEXT("bOverride_"));
	TArray<FString> Out;
	for (TFieldIterator<FBoolProperty> It(FPostProcessSettings::StaticStruct()); It; ++It)
	{
		const FString Name = It->GetName();
		if (Name.StartsWith(Prefix) && It->GetPropertyValue_InContainer(&Settings))
		{
			Out.Add(Name.Mid(Prefix.Len()));
		}
	}
	return Out;
}

void FMantleEnvironmentExport::Apply(UWorld* World, const TMap<const AActor*, FString>& IdByActor, FMantlePlace& Place, FMantleCoverage& Coverage)
{
	using namespace MantleEnvironmentExportPrivate;
	const bool bReadsEnvironment = FMantleTargetFeatures::Current().Reads(MantleClientFeatures::EnvironmentDefaults);
	FMantleEnvironmentSettings& Environment = Place.Settings.Environment;

	TArray<const ASkyAtmosphere*> Atmospheres;
	TArray<const ASkyLight*> SkyLights;
	TArray<const AVolumetricCloud*> Clouds;
	TArray<const AExponentialHeightFog*> Fogs;
	TArray<const APostProcessVolume*> Volumes;
	if (World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const AActor* Actor = *It;
			if (!IsReported(Actor))
			{
				continue;
			}
			if (const ASkyAtmosphere* Atmosphere = Cast<ASkyAtmosphere>(Actor))
			{
				Atmospheres.Add(Atmosphere);
			}
			else if (const ASkyLight* SkyLight = Cast<ASkyLight>(Actor))
			{
				SkyLights.Add(SkyLight);
			}
			else if (const AVolumetricCloud* Cloud = Cast<AVolumetricCloud>(Actor))
			{
				Clouds.Add(Cloud);
			}
			else if (const AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(Actor))
			{
				Fogs.Add(Fog);
			}
			else if (const APostProcessVolume* Volume = Cast<APostProcessVolume>(Actor))
			{
				Volumes.Add(Volume);
			}
		}
	}

	// A level with an environment of its own (a sun, sky, fog or post-process volume): its exposure is the place's to carry too.
	const bool bHasSun = Place.Entities.ContainsByPredicate([](const FMantleEntity& Entity) { return Entity.Type == EMantleEntityType::DirectionalLight; });
	const bool bHasEnvironment = bHasSun || Atmospheres.Num() > 0 || SkyLights.Num() > 0 || Clouds.Num() > 0 || Fogs.Num() > 0 || Volumes.Num() > 0;

	MapSun(IdByActor, Place, Coverage, bReadsEnvironment);
	MapPostProcess(Volumes, Environment, Coverage, bReadsEnvironment, bHasEnvironment, FMantleExposureProject::Current());

	// The weather. A level with a sky of its own (an atmosphere, clouds, a sky light or a sun) has one look, so the place's
	// weather is held at what its clouds show (Clear, PartlyCloudy or Overcast), or Clear when it has none, never left to
	// change on its own: the natural weather would rain on a level that never did. The creator chooses another in Mantle Place Settings.
	// Level's Sky (the default there) holds as above; choosing Natural in Mantle Place Settings lets the weather change on its own.
	const bool bOwnSky = Clouds.Num() > 0 || Atmospheres.Num() > 0 || SkyLights.Num() > 0 || bHasSun;
	TOptional<EMantleWeatherState> Held;
	FString HeldReason;
	const bool bWeatherSetByPlace = Environment.Weather.IsSet();
	const AMantlePlaceSettingsActor* PlaceSettings = World ? AMantlePlaceSettingsActor::Find(World) : nullptr;
	const bool bNaturalChosen = PlaceSettings && PlaceSettings->Weather == EMantlePlaceWeather::Natural;
	double CloudCoverage = DefaultCloudCoverage;
	if (Clouds.Num() > 0)
	{
		const UVolumetricCloudComponent* CloudComponent = Clouds[0]->FindComponentByClass<UVolumetricCloudComponent>();
		if (const UMaterialInterface* Material = CloudMaterialOf(CloudComponent))
		{
			float Value = 0.0f;
			if (Material->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(FMantleEnvironment::CloudCoverageParameter)), Value))
			{
				CloudCoverage = Value;
			}
		}
		const EMantleWeatherState State = FMantleEnvironmentExport::WeatherForCloudCoverage(CloudCoverage);
		Held = State;
		HeldReason = FString::Printf(TEXT("cloud coverage (%.2f) as the place's weather, held at %s (choose Natural or another Weather in Mantle Place Settings for a different one)"),
			CloudCoverage, MantleWeatherStateName(State));
	}
	else if (bOwnSky)
	{
		Held = EMantleWeatherState::Clear;
	}
	if (bNaturalChosen)
	{
		Held.Reset();
		HeldReason.Reset();
	}

	bool bWeatherWritten = false;
	if (Held.IsSet() && !bWeatherSetByPlace && bReadsEnvironment)
	{
		Environment.Weather = *Held;
		bWeatherWritten = true;
	}
	if (Clouds.Num() == 0 && Held.IsSet())
	{
		const TCHAR* Detail = bWeatherSetByPlace ? TEXT("the level has no volumetric cloud, but Mantle Place Settings holds the weather, which wins.")
			: !bReadsEnvironment ? TEXT("the level has no volumetric cloud, so the weather would be held at Clear: needs a newer Mantle client.")
			: TEXT("the level has no volumetric cloud, so the place's weather is held at Clear instead of changing on its own; choose Natural or another Weather in Mantle Place Settings for a different one.");
		Coverage.Add(Cat::Sky, bWeatherWritten ? EOutcome::Approximated : EOutcome::Dropped, TEXT("Weather"), Detail);
	}

	for (const ASkyAtmosphere* Atmosphere : Atmospheres)
	{
		ReportLook(Coverage, Cat::Sky, LabelOf(Atmosphere), false, FString(), TunedProperties(Atmosphere->GetComponent()),
			TEXT("Mantle supplies its own sky atmosphere (the place has no setting for it)"));
	}
	for (const ASkyLight* SkyLight : SkyLights)
	{
		ReportLook(Coverage, Cat::Sky, LabelOf(SkyLight), false, FString(),
			TunedProperties(SkyLight->GetLightComponent(), { FName(TEXT("bRealTimeCapture")) }), TEXT("Mantle supplies its own sky light (the place has no setting for it)"));
	}
	for (const AVolumetricCloud* Cloud : Clouds)
	{
		const bool bThisCarries = bWeatherWritten && Cloud == Clouds[0];
		TArray<FString> NotCarried = TunedProperties(Cloud->FindComponentByClass<UVolumetricCloudComponent>(), { FName(TEXT("Material")) });
		if (!bThisCarries && Held.IsSet() && Cloud == Clouds[0])
		{
			NotCarried.Insert(bWeatherSetByPlace ? FString(TEXT("Cloud Coverage (Mantle Place Settings holds the weather, which wins)"))
				: FString::Printf(TEXT("Cloud Coverage (%s)"), FMantleTargetFeatures::NeedsNewerClient), 0);
		}
		if (bNaturalChosen && !bWeatherSetByPlace && Cloud == Clouds[0])
		{
			NotCarried.Insert(FString(TEXT("Cloud Coverage (Mantle Place Settings chooses Natural weather, which changes on its own)")), 0);
		}
		ReportLook(Coverage, Cat::Sky, LabelOf(Cloud), bThisCarries, HeldReason, NotCarried,
			TEXT("Mantle supplies its own clouds (the place carries only their coverage, as the weather)"));
	}
	MapFog(Fogs, Environment, Coverage, bReadsEnvironment);
}
