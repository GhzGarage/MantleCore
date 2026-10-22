// Copyright Joshua Eger. All Rights Reserved.
#include "MantleAvatarConverter.h"
#include "MantleAvatarStripFixer.h"
#include "MantleAvatarContract.h"
#include "MantleEditorActions.h"
#include "MantleGarmentOutfit.h"
#include "MantleMetaHumanBuild.h"
#include "MantleReferenceBodies.h"
#include "MantleRuntime.h"
#include "MantleSettings.h"
#include "MantleUnriggedBody.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Engine/StaticMesh.h"
#include "IAssetTools.h"
#include "MetaHumanCharacter.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "MantleAssetCleanup.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SkinnedAssetCompiler.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

const FName FMantleAvatarConverter::ConvertedFromKey(TEXT("MantleConvertedFrom"));
const FName FMantleAvatarConverter::RouteKey(TEXT("MantleAvatarRoute"));
const FName FMantleAvatarConverter::BuiltKey(TEXT("MantleConvertBuilt"));

bool FMantleAvatarConverter::IsConvertOutput(const UObject* Asset)
{
	UPackage* Package = Asset ? Asset->GetPackage() : nullptr;
	if (!Package)
	{
		return false;
	}
	const FString* From = Package->GetMetaData().FindValue(Asset, ConvertedFromKey);
	const FString* Route = Package->GetMetaData().FindValue(Asset, RouteKey);
	// Built: only a MetaHuman handed on carries BuiltKey (a Convert that stopped part-way left none).
	const FString* Built = Package->GetMetaData().FindValue(Asset, BuiltKey);
	return From && !From->IsEmpty() && Route && Route->Equals(LexToString(EMantleAvatarRoute::Convert), ESearchCase::CaseSensitive)
		&& Built && !Built->IsEmpty();
}

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleAvatarConverterPrivate
{
	const TCHAR* const AvatarSlot = TEXT("avatar");

	/** Where a MetaHuman Creator character outside the project's content (an Epic preset) converts to. */
	const TCHAR* const MetaHumanSourceOutputFolder = TEXT("/Game/Converted");

	/** The As-is route only while UMantleSettings::bAllowAsIsAvatars is on (off by default). */
	bool ConverterRouteAllowed(const EMantleAvatarRoute Route, FString& OutError)
	{
		if (Route == EMantleAvatarRoute::AsIs && !UMantleSettings::AllowsAsIsAvatars())
		{
			OutError = TEXT("Avatars convert to a MetaHuman: uploading a character as-is isn't available.");
			return false;
		}
		return true;
	}

	/** The object path for "/Game/A/B" or "/Game/A/B.B". */
	FString ConverterObjectPath(const FString& Path)
	{
		FString ObjectPath = Path.TrimStartAndEnd();
		if (!ObjectPath.IsEmpty() && !FPackageName::GetShortName(ObjectPath).Contains(TEXT(".")))
		{
			ObjectPath += TEXT(".") + FPackageName::GetShortName(ObjectPath);
		}
		return ObjectPath;
	}

	/** An asset's path; a packaged reference mesh is identified by its editor-only provenance id. */
	FString ConverterAssetPath(const UObject* Object)
	{
		return FMantleReferenceBodies::Describe(Object);
	}

	/** The Convert route's own steps (pipeline steps 01 to 05 and the hand-off): one that fails stops the run. */
	bool ConverterIsConvertStep(const FName FixerId)
	{
		return FixerId == MantleAvatarFixerIds::Strip || FixerId == MantleAvatarFixerIds::MHUnrigged || FixerId == MantleAvatarFixerIds::MHPose
			|| FixerId == MantleAvatarFixerIds::MHConform
			|| FixerId == MantleAvatarFixerIds::MHSource || FixerId == MantleAvatarFixerIds::MHBuild;
	}

	/** The progress line while a step runs (the panel's Convert dialog shows it). */
	FText ConverterStepText(const FName FixerId)
	{
		if (FixerId == MantleAvatarFixerIds::RigMap)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepRigMap", "Mapping the rig onto a supported skeleton...");
		}
		if (FixerId == MantleAvatarFixerIds::AsIsHeight)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepAsIsHeight", "Setting the avatar to the default height...");
		}
		if (FixerId == MantleAvatarFixerIds::Repose)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepRepose", "Posing the body in its skeleton's A-pose...");
		}
		if (FixerId == MantleAvatarFixerIds::Strip)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepStrip", "Cutting the body down to bare skin...");
		}
		if (FixerId == MantleAvatarFixerIds::MHUnrigged)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepMHUnrigged", "Fitting the model's arms and height for a MetaHuman body...");
		}
		if (FixerId == MantleAvatarFixerIds::MHPose)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepMHPose", "Posing the body into MetaHuman's A-pose...");
		}
		if (FixerId == MantleAvatarFixerIds::MHConform)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepMHConform", "Fitting a MetaHuman to the body...");
		}
		if (FixerId == MantleAvatarFixerIds::MHSource)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepMHSource", "Building the MetaHuman Creator character at the default height with Epic's tools (the longest step)...");
		}
		if (FixerId == MantleAvatarFixerIds::MHBuild)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepMHBuild", "Building the MetaHuman with Epic's tools (the longest step)...");
		}
		if (FixerId == MantleAvatarFixerIds::RetargetPose)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepRetargetPose", "Lining the limbs up with the animation's driver...");
		}
		if (FixerId == MantleAvatarFixerIds::Assembly)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepAssembly", "Picking the face, clothing and hair...");
		}
		if (FixerId == MantleAvatarFixerIds::Refit)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepRefit", "Refitting clothing and hair onto the body...");
		}
		if (FixerId == MantleAvatarFixerIds::Triangles)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepTriangles", "Bringing the meshes under their triangle limits...");
		}
		if (FixerId == MantleAvatarFixerIds::Atlas)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepAtlas", "Combining materials into atlases...");
		}
		if (FixerId == MantleAvatarFixerIds::Bake)
		{
			return NSLOCTEXT("MantleAvatarConverter", "StepBake", "Baking the materials into plain textures...");
		}
		return NSLOCTEXT("MantleAvatarConverter", "StepOther", "Converting...");
	}

	bool IsAssemblyContext(const FMantleConvertContext& Context)
	{
		// A body Convert built into a MetaHuman is handed on with its build as the assembly folder
		// (FMantleMetaHumanBuild::HandOff); it is still its source's conversion, keyed by the source.
		return !Context.AssemblyFolder.IsEmpty() && Context.Parts.Num() > 0 && Context.MetaHuman.Num() == 0
			&& Context.Parts[0].Slot.Equals(AvatarSlot, ESearchCase::CaseSensitive);
	}

	TSharedRef<FJsonObject> ConverterFixJson(const FMantleConvertFix& Fix, const FString& Key)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("key"), Key);
		Object->SetStringField(TEXT("fixer"), Fix.FixerId.ToString());
		Object->SetStringField(TEXT("kind"), Fix.Kind.IsNone() ? FString() : Fix.Kind.ToString());
		Object->SetNumberField(TEXT("part"), Fix.PartIndex);
		Object->SetStringField(TEXT("problem"), Fix.Problem);
		Object->SetStringField(TEXT("action"), Fix.Action);
		Object->SetBoolField(TEXT("canFix"), Fix.bCanFix);
		Object->SetBoolField(TEXT("enabled"), Fix.bEnabled);
		Object->SetBoolField(TEXT("needsConfirm"), Fix.bNeedsConfirm);
		return Object;
	}

	TArray<TSharedPtr<FJsonValue>> ConverterOutcomesJson(TConstArrayView<FMantleConvertOutcome> Outcomes)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const FMantleConvertOutcome& Outcome : Outcomes)
		{
			TSharedRef<FJsonObject> Object = ConverterFixJson(Outcome.Fix, Outcome.Key);
			Object->SetStringField(TEXT("reason"), Outcome.Reason);
			Out.Add(MakeShared<FJsonValueObject>(Object));
		}
		return Out;
	}

	const TCHAR* ConverterSeverityName(const EMantleCosmeticSeverity Severity)
	{
		switch (Severity)
		{
		case EMantleCosmeticSeverity::Error:   return TEXT("error");
		case EMantleCosmeticSeverity::Warning: return TEXT("warning");
		case EMantleCosmeticSeverity::Info:    return TEXT("info");
		}
		return TEXT("unknown");
	}

	FString ConverterWriteJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	FString ConverterRefusal(const FString& Code, const FString& Message)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetBoolField(TEXT("ok"), false);
		Object->SetStringField(TEXT("refused"), Code);
		Object->SetStringField(TEXT("message"), Message);
		return ConverterWriteJson(Object);
	}
}

bool FMantleConvertResult::HasBlockers() const
{
	for (const FMantleConvertPartCheck& Check : Checks)
	{
		if (!Check.bCanUpload)
		{
			return true;
		}
	}
	return false;
}

const FMantleAssemblyMesh* FMantleAssemblyScan::Find(const USkeletalMesh* Mesh) const
{
	if (!Mesh)
	{
		return nullptr;
	}
	for (const TArray<FMantleAssemblyMesh>* List : { &Garments, &Hair })
	{
		for (const FMantleAssemblyMesh& Entry : *List)
		{
			if (Entry.Mesh.Get() == Mesh)
			{
				return &Entry;
			}
		}
	}
	return nullptr;
}

void FMantleAvatarConverter::RegisterBuiltInFixers()
{
	FMantleAvatarFixers::Register(MakeShared<FMantleAvatarAssemblyFixer>());
	FMantleAvatarFixers::Register(MakeShared<FMantleAvatarTrianglesFixer>());
}

FString FMantleAvatarConverter::OutputFolderFor(const FString& SourcePackagePath)
{
	FString Folder = SourcePackagePath;
	while (Folder.EndsWith(TEXT("/")))
	{
		Folder.LeftChopInline(1);
	}
	// A converted asset converted again stays in its Converted folder: never Converted/Converted.
	if (Folder.EndsWith(TEXT("/Converted"), ESearchCase::CaseSensitive))
	{
		return Folder;
	}
	return Folder + TEXT("/Converted");
}

FString FMantleAvatarConverter::SourceKey(const FMantleConvertContext& Context)
{
	// An assembly is one source however it was picked (its Blueprint, its body or its folder).
	if (MantleAvatarConverterPrivate::IsAssemblyContext(Context))
	{
		return Context.AssemblyFolder;
	}
	return Context.Source.IsValid() ? Context.Source->GetPathName() : Context.AssemblyFolder;
}

bool FMantleAvatarConverter::ClassOfSlot(const FString& Slot, const FMantleAvatarContract& Contract, EMantleCosmeticClass& OutClass)
{
	if (Slot.Equals(MantleAvatarConverterPrivate::AvatarSlot, ESearchCase::CaseSensitive))
	{
		OutClass = EMantleCosmeticClass::Avatar;
		return true;
	}
	if (Contract.HairSlots.ContainsByPredicate([&Slot](const FString& Hair) { return Hair.Equals(Slot, ESearchCase::CaseSensitive); }))
	{
		OutClass = EMantleCosmeticClass::Hair;
		return true;
	}
	const FMantleContractSlot* Entry = Contract.FindSlot(Slot);
	if (Entry && Entry->Id.Equals(Slot, ESearchCase::CaseSensitive) && Entry->ItemClass.Equals(TEXT("skinned_garment"), ESearchCase::CaseSensitive))
	{
		OutClass = EMantleCosmeticClass::Garment;
		return true;
	}
	return false;
}

bool FMantleAvatarConverter::MakeContext(UObject* Selection, const FMantleConvertOptions& Options, const FMantleAvatarContract& Contract,
	FMantleConvertContext& OutContext, FString& OutError)
{
	using namespace MantleAvatarConverterPrivate;
	OutContext = FMantleConvertContext();
	if (!ConverterRouteAllowed(Options.Route, OutError))
	{
		return false;
	}
	OutContext.Route = Options.Route;
	if (!Selection)
	{
		OutError = TEXT("Choose a skeletal mesh, a static mesh, or a MetaHuman Creator character (its Blueprint or its body), to convert.");
		return false;
	}

	// A MetaHuman Creator character asset (an Epic preset or the creator's own): the MetaHuman-source route builds a copy
	// of it at the default height (FMantleMetaHumanSourceFixer). One avatar part with no mesh until the build hands it on.
	if (const UMetaHumanCharacter* Character = Cast<UMetaHumanCharacter>(Selection))
	{
		if (OutContext.Route != EMantleAvatarRoute::Convert)
		{
			OutError = FString::Printf(TEXT("%s is a MetaHuman Creator character: it builds on the Convert route only."), *Character->GetName());
			return false;
		}
		// Next to a character in the project; an Epic preset (plugin content, never written to) into /Game/Converted.
		const FString SourceFolder = FPackageName::GetLongPackagePath(Character->GetOutermost()->GetName());
		OutContext.Source = Selection;
		OutContext.OutputFolder = !Options.OutputFolder.IsEmpty() ? Options.OutputFolder
			: SourceFolder.StartsWith(TEXT("/Game/")) ? OutputFolderFor(SourceFolder) : FString(MetaHumanSourceOutputFolder);
		FMantleConvertPart& Part = OutContext.Parts.AddDefaulted_GetRef();
		Part.Slot = AvatarSlot;
		Part.Name = FMantleAvatarAssemblyFixer::DisplayName(Character->GetName());
		UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT context character=%s slot=%s output=%s"), *Character->GetPathName(), *Part.Slot, *OutContext.OutputFolder);
		return true;
	}

	// A static mesh has no skeleton: the Convert route fits it to a MetaHuman body (FMantleUnriggedBody; the MHUnrigged step
	// takes Strip's and the A-pose step's place). One avatar part with no skeletal mesh until the build hands it on.
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Selection))
	{
		if (OutContext.Route != EMantleAvatarRoute::Convert)
		{
			OutError = FString::Printf(TEXT("%s has no skeleton: it can only be fitted to a MetaHuman body, on the Convert route."), *StaticMesh->GetName());
			return false;
		}
		if (!Options.Slot.TrimStartAndEnd().IsEmpty() && !Options.Slot.TrimStartAndEnd().Equals(AvatarSlot, ESearchCase::CaseSensitive))
		{
			OutError = FString::Printf(TEXT("%s has no skeleton, so it can only become an avatar (a MetaHuman body), not \"%s\"."), *StaticMesh->GetName(),
				*Options.Slot.TrimStartAndEnd());
			return false;
		}
		OutContext.Source = Selection;
		OutContext.OutputFolder = !Options.OutputFolder.IsEmpty() ? Options.OutputFolder
			: OutputFolderFor(FPackageName::GetLongPackagePath(StaticMesh->GetOutermost()->GetName()));
		FMantleConvertPart& Part = OutContext.Parts.AddDefaulted_GetRef();
		Part.Slot = AvatarSlot;
		Part.StaticSource = StaticMesh;
		Part.bUnrigged = true;
		Part.Name = FMantleAvatarAssemblyFixer::DisplayName(StaticMesh->GetName());
		OutContext.Notes.Add(FMantleUnriggedBody::PlainMessage());
		UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT context static mesh=%s slot=%s output=%s"), *StaticMesh->GetPathName(), *Part.Slot, *OutContext.OutputFolder);
		return true;
	}

	// A MetaHuman Creator assembly: its Blueprint or its body converts the whole character.
	const FString Folder = FMantleAvatarAssemblyFixer::AssemblyFolderOf(Selection);
	FMantleAssemblyScan Scan;
	FString ScanError;
	const bool bAssembly = !Folder.IsEmpty() && FMantleAvatarAssemblyFixer::Scan(Folder, Scan, ScanError);
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(Selection);
	if (bAssembly && (!Mesh || Mesh == Scan.Body.Get()))
	{
		return MakeAssemblyContext(Scan, Selection, Options, OutContext, OutError);
	}
	if (!Mesh)
	{
		if (Folder.IsEmpty())
		{
			// Creator wording; the technical detail is in the log. Static meshes took the unrigged route above.
			UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT %s is a %s: the converter takes a skeletal mesh, a static mesh, or a MetaHuman Creator character's Blueprint."),
				*Selection->GetName(), *Selection->GetClass()->GetName());
			OutError = FString::Printf(TEXT("%s isn't a character we can convert. Pick a skeletal mesh, a static mesh, or a MetaHuman Creator character's Blueprint."), *Selection->GetName());
			return false;
		}
		OutError = FString::Printf(TEXT("%s is not a MetaHuman Creator character's Blueprint: %s"), *Selection->GetName(), *ScanError);
		return false;
	}

	// One mesh, for the slot asked (or the one it has in its assembly, else an avatar).
	FString Slot = Options.Slot.TrimStartAndEnd();
	const FMantleAssemblyMesh* InAssembly = bAssembly ? Scan.Find(Mesh) : nullptr;
	if (Slot.IsEmpty())
	{
		Slot = InAssembly ? InAssembly->Slot : FString(AvatarSlot);
	}
	EMantleCosmeticClass Class = EMantleCosmeticClass::Avatar;
	if (!ClassOfSlot(Slot, Contract, Class))
	{
		OutError = FString::Printf(TEXT("\"%s\" is not a slot the converter makes: it converts avatars, clothing (top, bottom, footwear) and hair (hair, eyebrows, eyelashes, facial_hair)."), *Slot);
		return false;
	}
	OutContext.Source = Selection;
	OutContext.AssemblyFolder = bAssembly ? Scan.Folder : FString();
	OutContext.OutputFolder = !Options.OutputFolder.IsEmpty() ? Options.OutputFolder
		: OutputFolderFor(FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName()));
	FMantleConvertPart& Part = OutContext.Parts.AddDefaulted_GetRef();
	Part.Slot = Slot;
	Part.Mesh = Mesh;
	Part.Original = Mesh;
	Part.Name = FMantleAvatarAssemblyFixer::DisplayName(Mesh->GetName());
	// A skeletal mesh with one bone has no skeleton to speak of: it is fitted to a MetaHuman body as a static mesh is.
	if (!InAssembly && Class == EMantleCosmeticClass::Avatar && OutContext.Route == EMantleAvatarRoute::Convert && Mesh->GetRefSkeleton().GetRawBoneNum() == 1)
	{
		Part.bUnrigged = true;
		OutContext.Notes.Add(FMantleUnriggedBody::PlainMessage());
	}
	if (InAssembly && Class != EMantleCosmeticClass::Avatar)
	{
		Part.SkeletonId = TEXT("metahuman");
		Part.ClothAsset = InAssembly->ClothAsset;
		// Picked on its own, a cloth piece is what the creator asked for: included, its simulation dropped.
		if (!Part.ClothAsset.IsEmpty())
		{
			OutContext.Notes.Add(FString::Printf(TEXT("%s is simulated cloth (%s): it converts as a plain garment, the simulation dropped."),
				*Mesh->GetName(), *Part.ClothAsset));
		}
	}
	// A third-party avatar's body picked on its own: its separate clothing beside it, which can become MetaHuman Outfits on
	// the MetaHuman Convert makes of this body.
	if (!InAssembly && Class == EMantleCosmeticClass::Avatar && OutContext.Route == EMantleAvatarRoute::Convert && !Part.bUnrigged
		&& Options.OnlyFixers.Num() == 0)
	{
		TArray<FString> NotOffered;
		FMantleGarmentOutfit::FindGarments(*Mesh, OutContext.Garments, NotOffered);
		// #55: the clothing in the character's own sections, cut into pieces when the Outfits are made.
		FMantleGarmentOutfit::FindSectionGarments(*Mesh, OutContext.Garments);
		if (NotOffered.Num() > 0)
		{
			OutContext.Notes.Add(FString::Printf(TEXT("Only tops, bottoms and footwear become Outfits; not offered: %s."), *FString::Join(NotOffered, TEXT(", "))));
		}
	}
	UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT context mesh=%s slot=%s assembly=%s output=%s garments=%d"), *Mesh->GetPathName(), *Slot,
		*OutContext.AssemblyFolder, *OutContext.OutputFolder, OutContext.Garments.Num());
	return true;
}

bool FMantleAvatarConverter::MakeContextForPath(const FString& Path, const FMantleConvertOptions& Options, const FMantleAvatarContract& Contract,
	FMantleConvertContext& OutContext, FString& OutError)
{
	using namespace MantleAvatarConverterPrivate;
	const FString Trimmed = Path.TrimStartAndEnd();
	const FString ObjectPath = ConverterObjectPath(Trimmed);
	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	UObject* Asset = nullptr;
	if (!PackageName.IsEmpty() && FPackageName::IsValidLongPackageName(PackageName))
	{
		// In memory first (a new, unsaved asset), then on disk: a wrong path is a refusal, not a load warning.
		Asset = FindObject<UObject>(nullptr, *ObjectPath);
		if (!Asset && FPackageName::DoesPackageExist(PackageName))
		{
			Asset = LoadObject<UObject>(nullptr, *ObjectPath);
		}
	}
	if (Asset)
	{
		return MakeContext(Asset, Options, Contract, OutContext, OutError);
	}
	// A folder: an assembly's.
	FString Folder = Trimmed;
	while (Folder.EndsWith(TEXT("/")))
	{
		Folder.LeftChopInline(1);
	}
	FMantleAssemblyScan Scan;
	FString ScanError;
	if (FPackageName::IsValidLongPackageName(Folder) && FMantleAvatarAssemblyFixer::Scan(Folder, Scan, ScanError))
	{
		return MakeAssemblyContext(Scan, nullptr, Options, OutContext, OutError);
	}
	OutError = FString::Printf(TEXT("There is no skeletal mesh, Blueprint or MetaHuman Creator character at \"%s\". Give an asset's path as the Content Browser shows it (/Game/Characters/SKM_Hero), or a character's folder (/Game/CharacterAssemblies/Vampire)."),
		*Trimmed);
	return false;
}

bool FMantleAvatarConverter::MakeAssemblyContext(const FMantleAssemblyScan& Scan, UObject* Source, const FMantleConvertOptions& Options,
	FMantleConvertContext& OutContext, FString& OutError)
{
	using namespace MantleAvatarConverterPrivate;
	OutContext = FMantleConvertContext();
	if (!ConverterRouteAllowed(Options.Route, OutError))
	{
		return false;
	}
	OutContext.Route = Options.Route;
	USkeletalMesh* Body = Scan.Body.Get();
	if (!Body)
	{
		OutError = FString::Printf(TEXT("%s has no body to convert."), *Scan.Folder);
		return false;
	}
	OutContext.Source = Source;
	OutContext.AssemblyFolder = Scan.Folder;
	OutContext.OutputFolder = !Options.OutputFolder.IsEmpty() ? Options.OutputFolder : OutputFolderFor(Scan.Folder);

	// An assembled MetaHuman (meshes, no MetaHuman Creator character) on the Convert route: MHSource makes a character from
	// its face and body and builds it, as it builds a character source. One avatar part with no mesh until the build hands it on;
	// its clothing and hair are not carried through the rebuild.
	if (OutContext.Route == EMantleAvatarRoute::Convert)
	{
		USkeletalMesh* Face = Scan.Face.Get();
		// Only a real MetaHuman (Epic's face and body skeletons): any other humanoid assembly converts as before.
		const bool bMetaHuman = Face && FMantleMetaHumanBuild::IsMetaHumanAssembly(Body->GetSkeleton() ? Body->GetSkeleton()->GetPathName() : FString(),
			Face->GetSkeleton() ? Face->GetSkeleton()->GetPathName() : FString());
		if (!Face && Body->GetSkeleton() && FMantleMetaHumanBuild::IsMetaHumanAssembly(Body->GetSkeleton()->GetPathName(), TEXT("Face_Archetype_Skeleton")))
		{
			OutContext.Notes.Add(FMantleMetaHumanBuild::AssemblyNotBuiltMessage(Scan.Name, TEXT("no face mesh was found next to its body")));
		}
		else if (bMetaHuman)
		{
			OutContext.Source = Source ? Source : Body;
			OutContext.AssemblyFolder.Reset();
			OutContext.AssemblyBodyToFit = Body;
			OutContext.AssemblyFaceToFit = Face;
			FMantleConvertPart& ToBuild = OutContext.Parts.AddDefaulted_GetRef();
			ToBuild.Slot = AvatarSlot;
			ToBuild.Name = Scan.Name;
			TArray<FString> Others;
			for (const TArray<FMantleAssemblyMesh>* List : { &Scan.Garments, &Scan.Hair })
			{
				for (const FMantleAssemblyMesh& Entry : *List)
				{
					if (const USkeletalMesh* Mesh = Entry.Mesh.Get())
					{
						Others.AddUnique(Mesh->GetName());
					}
				}
			}
			Others.Append(Scan.LeftOut);
			if (Others.Num() > 0)
			{
				OutContext.Notes.Add(FString::Printf(TEXT("%s is rebuilt from its face and body; not carried through: %s"), *Scan.Name, *FString::Join(Others, TEXT(", "))));
			}
			UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT assembly=%s body=%s face=%s built as a MetaHuman Creator character, output=%s"), *Scan.Folder,
				*Body->GetPathName(), *Face->GetPathName(), *OutContext.OutputFolder);
			return true;
		}
	}

	FMantleConvertPart& Avatar = OutContext.Parts.AddDefaulted_GetRef();
	Avatar.Slot = AvatarSlot;
	Avatar.Mesh = Body;
	Avatar.Original = Body;
	Avatar.SkeletonId = TEXT("metahuman");
	Avatar.Name = Scan.Name;

	// A one-piece outfit that names several slots uploads once per slot: say which in its name.
	TMap<const USkeletalMesh*, int32> SlotsPerMesh;
	for (const FMantleAssemblyMesh& Garment : Scan.Garments)
	{
		SlotsPerMesh.FindOrAdd(Garment.Mesh.Get())++;
	}
	for (const FMantleAssemblyMesh& Garment : Scan.Garments)
	{
		USkeletalMesh* Mesh = Garment.Mesh.Get();
		if (!Mesh)
		{
			continue;
		}
		FMantleConvertPart& Part = OutContext.Parts.AddDefaulted_GetRef();
		Part.Slot = Garment.Slot;
		Part.Mesh = Mesh;
		Part.Original = Mesh;
		Part.SkeletonId = TEXT("metahuman");
		Part.Name = FMantleAvatarAssemblyFixer::DisplayName(Mesh->GetName());
		if (SlotsPerMesh.FindRef(Mesh) > 1)
		{
			Part.Name += TEXT(" ") + Garment.Slot;
		}
		// A cloth-simulated piece waits for the creator (the Assembly fixer offers it, off).
		Part.ClothAsset = Garment.ClothAsset;
		Part.bIncluded = Garment.ClothAsset.IsEmpty();
	}
	for (const FMantleAssemblyMesh& Hair : Scan.Hair)
	{
		USkeletalMesh* Mesh = Hair.Mesh.Get();
		if (!Mesh)
		{
			continue;
		}
		FMantleConvertPart& Part = OutContext.Parts.AddDefaulted_GetRef();
		Part.Slot = Hair.Slot;
		Part.Mesh = Mesh;
		Part.Original = Mesh;
		Part.Name = FMantleAvatarAssemblyFixer::DisplayName(Mesh->GetName());
	}
	for (const FString& Line : Scan.LeftOut)
	{
		OutContext.Notes.Add(FString::Printf(TEXT("Left out: %s"), *Line));
	}
	UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT assembly=%s body=%s face=%s garments=%d hair=%d output=%s"), *Scan.Folder, *Body->GetPathName(),
		Scan.Face.IsValid() ? *Scan.Face->GetPathName() : TEXT("none"), Scan.Garments.Num(), Scan.Hair.Num(), *OutContext.OutputFolder);
	return true;
}

TArray<FMantleConvertFix> FMantleAvatarConverter::Analyze(const FMantleConvertContext& Context)
{
	TArray<FMantleConvertFix> Fixes;
	for (const TSharedRef<IMantleAvatarFixer>& Fixer : FMantleAvatarFixers::Get())
	{
		TArray<FMantleConvertFix> Own;
		Fixer->Analyze(Context, Own);
		for (FMantleConvertFix& Fix : Own)
		{
			// A fixer names itself; say so when it forgets, so keys and the list stay truthful.
			if (Fix.FixerId.IsNone())
			{
				Fix.FixerId = Fixer->GetId();
			}
			Fixes.Add(MoveTemp(Fix));
		}
	}
	return Fixes;
}

TArray<FString> FMantleAvatarConverter::FixKeys(TConstArrayView<FMantleConvertFix> Fixes)
{
	TArray<FString> Keys;
	Keys.Reserve(Fixes.Num());
	for (int32 Index = 0; Index < Fixes.Num(); ++Index)
	{
		const FMantleConvertFix& Fix = Fixes[Index];
		FString Key = FString::Printf(TEXT("%s.%d."), *Fix.FixerId.ToString(), Fix.PartIndex);
		if (!Fix.Kind.IsNone())
		{
			Key += Fix.Kind.ToString();
		}
		else
		{
			int32 Ordinal = 0;
			for (int32 Earlier = 0; Earlier < Index; ++Earlier)
			{
				const FMantleConvertFix& Other = Fixes[Earlier];
				if (Other.FixerId == Fix.FixerId && Other.PartIndex == Fix.PartIndex && Other.Kind.IsNone())
				{
					++Ordinal;
				}
			}
			Key += FString::FromInt(Ordinal);
		}
		Keys.Add(MoveTemp(Key));
	}
	return Keys;
}

FMantleConvertResult FMantleAvatarConverter::Convert(FMantleConvertContext& Context, TConstArrayView<FMantleConvertFix> Choices,
	const FMantleConvertOptions& Options, const FMantleAvatarContract& Contract)
{
	FMantleConvertResult Result;
	const FString Key = SourceKey(Context);

	// The route is checked again here: a context made while As-is was allowed runs nothing once it is off.
	FString RouteError;
	if (!MantleAvatarConverterPrivate::ConverterRouteAllowed(Context.Route, RouteError))
	{
		FMantleConvertOutcome Outcome;
		Outcome.Fix.Problem = FString::Printf(TEXT("the %s route"), LexToString(Context.Route));
		Outcome.Reason = RouteError + TEXT(" (nothing was converted)");
		UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT stopped: %s"), *RouteError);
		Result.Failed.Add(MoveTemp(Outcome));
		return Result;
	}

	TMap<FString, bool> Chosen;
	const TArray<FString> ChoiceKeys = FixKeys(Choices);
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		Chosen.Add(ChoiceKeys[Index], Choices[Index].bEnabled);
	}

	// A line the creator must confirm (bNeedsConfirm, e.g. "the kept parts are bare skin") left unticked stops the
	// whole conversion before anything is written or replaced.
	auto Unconfirmed = [&Chosen](const FMantleConvertFix& Fix, const FString& FixKey)
	{
		// With "keep the face, choose a body" ticked nothing is stripped, so there is no bare skin to confirm.
		if (Fix.Kind == FMantleAvatarStripFixer::KindBareSkin && FixKey.EndsWith(Fix.Kind.ToString()))
		{
			const FString FaceKey = FixKey.LeftChop(Fix.Kind.ToString().Len()) + FMantleAvatarStripFixer::KindFaceOnly.ToString();
			if (const bool* Face = Chosen.Find(FaceKey); Face && *Face)
			{
				return false;
			}
		}
		const bool* Choice = Chosen.Find(FixKey);
		return Fix.bNeedsConfirm && !(Choice ? *Choice : Fix.bEnabled);
	};
	{
		const TArray<FMantleConvertFix> Initial = Analyze(Context);
		const TArray<FString> InitialKeys = FixKeys(Initial);
		for (int32 Index = 0; Index < Initial.Num(); ++Index)
		{
			if (Options.OnlyFixers.Num() > 0 && !Options.OnlyFixers.Contains(Initial[Index].FixerId))
			{
				continue;
			}
			if (Unconfirmed(Initial[Index], InitialKeys[Index]))
			{
				FMantleConvertOutcome Outcome;
				Outcome.Fix = Initial[Index];
				Outcome.Key = InitialKeys[Index];
				Outcome.Reason = TEXT("not confirmed: tick it to convert (nothing was converted)");
				UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT stopped: %s is not confirmed"), *Outcome.Key);
				Result.Failed.Add(MoveTemp(Outcome));
			}
		}
		// With the face kept, the creator picks its body (masculine or feminine); Convert never guesses it.
		TArray<FMantleConvertFix> AsChosen = Initial;
		for (int32 Index = 0; Index < AsChosen.Num(); ++Index)
		{
			if (const bool* Choice = Chosen.Find(InitialKeys[Index]))
			{
				AsChosen[Index].bEnabled = *Choice;
			}
		}
		for (int32 PartIndex = 0; PartIndex < Context.Parts.Num(); ++PartIndex)
		{
			const FString Missing = FMantleAvatarStripFixer::MissingBodyChoice(AsChosen, PartIndex);
			if (!Missing.IsEmpty())
			{
				FMantleConvertOutcome Outcome;
				Outcome.Key = FString::Printf(TEXT("%s.%d.%s"), *MantleAvatarFixerIds::Strip.ToString(), PartIndex, *FMantleAvatarStripFixer::KindFeminineBody.ToString());
				Outcome.Fix.FixerId = MantleAvatarFixerIds::Strip;
				Outcome.Fix.PartIndex = PartIndex;
				Outcome.Fix.Kind = FMantleAvatarStripFixer::KindFeminineBody;
				Outcome.Fix.Problem = TEXT("the body the face goes on");
				Outcome.Reason = Missing + TEXT(" (nothing was converted)");
				UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT stopped: part %d: %s"), PartIndex, *Missing);
				Result.Failed.Add(MoveTemp(Outcome));
			}
		}
		if (Result.Failed.Num() > 0)
		{
			return Result;
		}
	}

	// Converting again replaces what the last conversion of this source wrote; nothing else in the folder is touched.
	Result.RemovedPrevious = RemovePreviousOutput(Context.OutputFolder, Key);
	if (Result.RemovedPrevious > 0)
	{
		Context.Notes.Add(FString::Printf(TEXT("Replaced %d assets an earlier conversion of this character wrote to %s."), Result.RemovedPrevious, *Context.OutputFolder));
	}

	bool bStopped = false;
	const TArray<TSharedRef<IMantleAvatarFixer>> Fixers = FMantleAvatarFixers::Get();
	// One progress frame per step and one for the checks, for the panel's dialog (no Cancel: a step can't be stopped
	// part-way).
	FScopedSlowTask Progress(static_cast<float>(Fixers.Num() + 1), NSLOCTEXT("MantleAvatarConverter", "Converting", "Converting..."));
	for (const TSharedRef<IMantleAvatarFixer>& Fixer : Fixers)
	{
		if (bStopped)
		{
			break;
		}
		Progress.EnterProgressFrame(1.0f, MantleAvatarConverterPrivate::ConverterStepText(Fixer->GetId()));
		if (Options.OnlyFixers.Num() > 0 && !Options.OnlyFixers.Contains(Fixer->GetId()))
		{
			continue;
		}
		// Each fixer sees the context as the fixers before it left it.
		TArray<FMantleConvertFix> Fixes;
		Fixer->Analyze(Context, Fixes);
		for (FMantleConvertFix& Fix : Fixes)
		{
			if (Fix.FixerId.IsNone())
			{
				Fix.FixerId = Fixer->GetId();
			}
		}
		const TArray<FString> Keys = FixKeys(Fixes);
		for (int32 Index = 0; Index < Fixes.Num(); ++Index)
		{
			FMantleConvertOutcome Outcome;
			Outcome.Fix = Fixes[Index];
			Outcome.Key = Keys[Index];
			if (Unconfirmed(Outcome.Fix, Outcome.Key))
			{
				// Met only now (an earlier fixer made the case for it): stop here.
				Outcome.Reason = TEXT("not confirmed: tick it to convert (the conversion stopped here)");
				UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT stopped: %s is not confirmed"), *Outcome.Key);
				Result.Failed.Add(MoveTemp(Outcome));
				bStopped = true;
				break;
			}
			if (const bool* Choice = Chosen.Find(Outcome.Key))
			{
				Outcome.Fix.bEnabled = *Choice;
			}
			const bool bPartIncluded = !Context.Parts.IsValidIndex(Outcome.Fix.PartIndex) || Context.Parts[Outcome.Fix.PartIndex].bIncluded;
			if (!Outcome.Fix.bEnabled || !Outcome.Fix.bCanFix || (!bPartIncluded && Outcome.Fix.FixerId != MantleAvatarFixerIds::Assembly))
			{
				Outcome.Reason = !Outcome.Fix.bEnabled ? TEXT("turned off") : !Outcome.Fix.bCanFix ? TEXT("not fixable here") : TEXT("its part is left out");
				Result.LeftAlone.Add(MoveTemp(Outcome));
				continue;
			}
			FString Error;
			if (Fixer->Apply(Outcome.Fix, Context, Error))
			{
				UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT applied %s: %s"), *Outcome.Key, *Outcome.Fix.Action);
				Result.Applied.Add(MoveTemp(Outcome));
			}
			else
			{
				UE_LOG(LogMantle, Warning, TEXT("MANTLE_CONVERT failed %s: %s"), *Outcome.Key, *Error);
				Outcome.Reason = Error;
				// A failed Convert-route step (strip, A-pose, conform, build and hand-off) stops the run: nothing after it
				// may dress up the half-made body as an upload (it is never a Convert output: IsConvertOutput).
				const bool bConvertStep = MantleAvatarConverterPrivate::ConverterIsConvertStep(Outcome.Fix.FixerId);
				Result.Failed.Add(MoveTemp(Outcome));
				if (bConvertStep)
				{
					UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT stopped: a Convert step failed"));
					bStopped = true;
					break;
				}
			}
		}
	}

	// Stamp what this conversion wrote, so the next conversion of the source finds and replaces it; then keep it.
	TSet<UPackage*> Saved;
	for (const TWeakObjectPtr<UObject>& Created : Context.CreatedAssets)
	{
		UObject* Asset = Created.Get();
		if (!Asset)
		{
			continue;
		}
		UPackage* Package = Asset->GetPackage();
		Package->GetMetaData().SetValue(Asset, ConvertedFromKey, *Key);
		// Also the route, so the upload knows a Convert output (accepted while As-is is off) from a creator's own mesh.
		Package->GetMetaData().SetValue(Asset, RouteKey, LexToString(Context.Route));
		Package->MarkPackageDirty();
		if (!Options.bSave || Saved.Contains(Package))
		{
			continue;
		}
		Saved.Add(Package);
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
		{
			Context.Notes.Add(FString::Printf(TEXT("%s could not be saved; it stays in the editor until you save it (File > Save All)."), *Package->GetName()));
			UE_LOG(LogMantle, Warning, TEXT("MANTLE_CONVERT could not save %s"), *Package->GetName());
		}
	}

	if (Options.bCheck)
	{
		Progress.EnterProgressFrame(1.0f, NSLOCTEXT("MantleAvatarConverter", "StepCheck", "Checking the parts..."));
		for (int32 Index = 0; Index < Context.Parts.Num(); ++Index)
		{
			FMantleConvertPart& Part = Context.Parts[Index];
			if (!Part.bIncluded)
			{
				continue;
			}
			FMantleCosmeticDraft Draft = MakeDraft(Part, Contract);
			if (Draft.Class == EMantleCosmeticClass::Avatar)
			{
				Draft.AvatarPublicationMode = Context.Route == EMantleAvatarRoute::AsIs ? FString(TEXT("as_is"))
					: FString(TEXT("converted"));
			}
			// The checks only: no generated LODs, rendered thumbnail or body shapes (the panel's Upload makes them).
			Draft.bGenerateLODs = false;
			Draft.bRenderThumbnail = false;
			Draft.bBodyOffsets = false;
			const FMantleCosmeticPackage Package = FMantleCosmeticExport::Build(Draft, Contract);
			FMantleConvertPartCheck& Check = Result.Checks.AddDefaulted_GetRef();
			Check.PartIndex = Index;
			Check.Issues = Package.Issues;
			Check.bCanUpload = !Package.HasErrors();
			Check.StandingHeightCm = Package.AvatarStandingHeightCm;
			if (Draft.Class == EMantleCosmeticClass::Avatar && Part.SkeletonId.IsEmpty())
			{
				FString SkeletonId;
				FString Error;
				if (FMantleCosmeticExport::AvatarSkeletonId(Part.Mesh.Get(), Contract, SkeletonId, Error))
				{
					Part.SkeletonId = SkeletonId;
				}
			}
			UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT check part=%d %s ready=%d"), Index, *DescribePart(Part, Contract), Check.bCanUpload ? 1 : 0);
			for (const FMantleCosmeticIssue& Issue : Check.Issues)
			{
				UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT check part=%d %s"), Index, *Issue.ToString());
			}
		}
	}
	return Result;
}

FMantleCosmeticDraft FMantleAvatarConverter::MakeDraft(const FMantleConvertPart& Part, const FMantleAvatarContract& Contract)
{
	EMantleCosmeticClass Class = EMantleCosmeticClass::Garment;
	ClassOfSlot(Part.Slot, Contract, Class);
	// The panel's order (FMantleEditorActions::MakeCosmeticDraft), with the part's own picks set first so the defaults keep them.
	FMantleCosmeticDraft Draft;
	Draft.Class = Class;
	Draft.Slot = Part.Slot;
	Draft.Name = Part.Name;
	Draft.Source = Part.Mesh.Get();
	if (Class == EMantleCosmeticClass::Avatar)
	{
		Draft.Face = Part.Face;
	}
	else
	{
		Draft.AuthoringBody = Part.AuthoringBody;
		Draft.AuthoringFace = Part.AuthoringFace;
	}
	Draft.SourceSection = Class == EMantleCosmeticClass::Hair ? Part.Section : INDEX_NONE;
	FMantleEditorActions::ApplyCosmeticSourceDefaults(Draft, &Contract);
	FMantleEditorActions::ApplyCosmeticSectionDefaults(Draft);
	FMantleEditorActions::ApplyCosmeticSocketDefaults(Draft, &Contract);
	TArray<FName> JawCandidates;
	FMantleEditorActions::ApplyCosmeticJawDefaults(Draft, &Contract, JawCandidates);
	TArray<FName> SecondaryCandidates;
	FMantleEditorActions::ApplyCosmeticSecondaryDefaults(Draft, &Contract, SecondaryCandidates);
	FMantleEditorActions::ApplyCosmeticOutfitDefaults(Draft);
	FMantleEditorActions::ApplyCosmeticRoleDefaults(Draft, &Contract);
	return Draft;
}

FString FMantleAvatarConverter::DescribePart(const FMantleConvertPart& Part, const FMantleAvatarContract& Contract)
{
	EMantleCosmeticClass Class = EMantleCosmeticClass::Garment;
	if (!ClassOfSlot(Part.Slot, Contract, Class))
	{
		return FString::Printf(TEXT("%s: %s"), *Part.Slot, *Part.Name);
	}
	if (Class == EMantleCosmeticClass::Avatar)
	{
		return FString::Printf(TEXT("Avatar: %s"), *Part.Name);
	}
	return FString::Printf(TEXT("%s (%s): %s"), LexToString(Class), *Part.Slot, *Part.Name);
}

UObject* FMantleAvatarConverter::DuplicateIntoOutput(FMantleConvertContext& Context, UObject* Source, FString& OutError)
{
	if (!Source)
	{
		OutError = TEXT("there is nothing to copy");
		return nullptr;
	}
	if (Context.OutputFolder.IsEmpty() || !FPackageName::IsValidLongPackageName(Context.OutputFolder, /*bIncludeReadOnlyRoots*/ true))
	{
		OutError = FString::Printf(TEXT("\"%s\" is not a folder the converter can write to"), *Context.OutputFolder);
		return nullptr;
	}
	if (USkinnedAsset* Skinned = Cast<USkinnedAsset>(Source))
	{
		TArray<USkinnedAsset*> Compiling = { Skinned };
		FSkinnedAssetCompilingManager::Get().FinishCompilation(Compiling);
	}
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	FString PackageName;
	FString AssetName;
	AssetTools.CreateUniqueAssetName(Context.OutputFolder / Source->GetName(), FString(), PackageName, AssetName);
	UObject* Copy = AssetTools.DuplicateAsset(AssetName, FPackageName::GetLongPackagePath(PackageName), Source);
	if (!Copy)
	{
		OutError = FString::Printf(TEXT("%s could not be copied to %s"), *Source->GetName(), *Context.OutputFolder);
		return nullptr;
	}
	if (USkinnedAsset* Skinned = Cast<USkinnedAsset>(Copy))
	{
		TArray<USkinnedAsset*> Compiling = { Skinned };
		FSkinnedAssetCompilingManager::Get().FinishCompilation(Compiling);
	}
	Context.CreatedAssets.Add(Copy);
	UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT copied %s to %s"), *Source->GetPathName(), *Copy->GetPathName());
	return Copy;
}

USkeletalMesh* FMantleAvatarConverter::MakeWritable(FMantleConvertContext& Context, USkeletalMesh* Mesh, FString& OutError)
{
	if (!Mesh)
	{
		OutError = TEXT("the part has no mesh");
		return nullptr;
	}
	for (const TWeakObjectPtr<UObject>& Created : Context.CreatedAssets)
	{
		if (Created.Get() == Mesh)
		{
			return Mesh;
		}
	}
	return Cast<USkeletalMesh>(DuplicateIntoOutput(Context, Mesh, OutError));
}

USkeletalMesh* FMantleAvatarConverter::ConvertedBodyFor(const FMantleConvertContext& Context, USkeletalMesh* CharacterBody, const FMantleAvatarContract& Contract)
{
	if (!CharacterBody)
	{
		return nullptr;
	}
	for (const FMantleConvertPart& Part : Context.Parts)
	{
		USkeletalMesh* Mesh = Part.Mesh.Get();
		if (!Mesh || Mesh == CharacterBody || Part.Original.Get() != CharacterBody || !Part.Slot.Equals(MantleAvatarConverterPrivate::AvatarSlot, ESearchCase::CaseSensitive))
		{
			continue;
		}
		const bool bConverted = Context.CreatedAssets.ContainsByPredicate([Mesh](const TWeakObjectPtr<UObject>& Created) { return Created.Get() == Mesh; });
		if (bConverted && FMantleCosmeticExport::FindReferenceLOD(Mesh, Contract) != INDEX_NONE)
		{
			return Mesh;
		}
	}
	return CharacterBody;
}

int32 FMantleAvatarConverter::RemovePreviousOutput(const FString& OutputFolder, const FString& Key)
{
	if (OutputFolder.IsEmpty() || Key.IsEmpty() || !FPackageName::IsValidLongPackageName(OutputFolder, /*bIncludeReadOnlyRoots*/ true))
	{
		return 0;
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*OutputFolder));
	Filter.bRecursivePaths = true;
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	TArray<UObject*> Doomed;
	for (const FAssetData& Data : Assets)
	{
		UObject* Asset = Data.GetAsset();
		if (!Asset)
		{
			continue;
		}
		const FString* From = Asset->GetPackage()->GetMetaData().FindValue(Asset, ConvertedFromKey);
		if (From && From->Equals(Key, ESearchCase::CaseSensitive))
		{
			Doomed.Add(Asset);
		}
	}
	if (Doomed.Num() == 0)
	{
		return 0;
	}
	const int32 Deleted = FMantleAssetCleanup::MoveAside(Doomed);
	UE_LOG(LogMantle, Log, TEXT("MANTLE_CONVERT replaced the earlier output of %s: %d of %d assets under %s deleted"), *Key, Deleted, Doomed.Num(), *OutputFolder);
	return Deleted;
}

TSharedRef<FJsonObject> FMantleAvatarConverter::ToJson(const FMantleConvertContext& Context, TConstArrayView<FMantleConvertFix> Fixes,
	const FMantleConvertResult* Result, const FMantleAvatarContract& Contract)
{
	using namespace MantleAvatarConverterPrivate;
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("ok"), true);
	Object->SetStringField(TEXT("source"), SourceKey(Context));
	Object->SetStringField(TEXT("assembly"), Context.AssemblyFolder);
	Object->SetStringField(TEXT("outputFolder"), Context.OutputFolder);
	Object->SetStringField(TEXT("route"), LexToString(Context.Route));

	TArray<TSharedPtr<FJsonValue>> Parts;
	for (int32 Index = 0; Index < Context.Parts.Num(); ++Index)
	{
		const FMantleConvertPart& Part = Context.Parts[Index];
		EMantleCosmeticClass Class = EMantleCosmeticClass::Garment;
		const bool bKnown = ClassOfSlot(Part.Slot, Contract, Class);
		TSharedRef<FJsonObject> PartObject = MakeShared<FJsonObject>();
		PartObject->SetNumberField(TEXT("index"), Index);
		PartObject->SetStringField(TEXT("slot"), Part.Slot);
		PartObject->SetStringField(TEXT("class"), bKnown ? LexToString(Class) : TEXT(""));
		PartObject->SetStringField(TEXT("name"), Part.Name);
		PartObject->SetStringField(TEXT("mesh"), ConverterAssetPath(Part.Mesh.Get()));
		PartObject->SetStringField(TEXT("original"), ConverterAssetPath(Part.Original.Get()));
		PartObject->SetStringField(TEXT("face"), ConverterAssetPath(Part.Face.Get()));
		PartObject->SetNumberField(TEXT("section"), Part.Section);
		PartObject->SetStringField(TEXT("skeleton"), Part.SkeletonId);
		PartObject->SetStringField(TEXT("authoringBody"), ConverterAssetPath(Part.AuthoringBody.Get()));
		PartObject->SetStringField(TEXT("authoringFace"), ConverterAssetPath(Part.AuthoringFace.Get()));
		PartObject->SetBoolField(TEXT("included"), Part.bIncluded);
		PartObject->SetStringField(TEXT("clothAsset"), Part.ClothAsset);
		Parts.Add(MakeShared<FJsonValueObject>(PartObject));
	}
	Object->SetArrayField(TEXT("parts"), Parts);

	TArray<TSharedPtr<FJsonValue>> FixArray;
	const TArray<FString> Keys = FixKeys(Fixes);
	for (int32 Index = 0; Index < Fixes.Num(); ++Index)
	{
		FixArray.Add(MakeShared<FJsonValueObject>(ConverterFixJson(Fixes[Index], Keys[Index])));
	}
	Object->SetArrayField(TEXT("fixes"), FixArray);

	TArray<TSharedPtr<FJsonValue>> Notes;
	for (const FString& Note : Context.Notes)
	{
		Notes.Add(MakeShared<FJsonValueString>(Note));
	}
	Object->SetArrayField(TEXT("notes"), Notes);

	// A third-party avatar's separate clothing, which the Cosmetics panel offers as Outfits after Convert.
	TArray<TSharedPtr<FJsonValue>> Garments;
	for (const FMantleConvertGarment& Garment : Context.Garments)
	{
		TSharedRef<FJsonObject> GarmentObject = MakeShared<FJsonObject>();
		GarmentObject->SetStringField(TEXT("name"), Garment.Name);
		GarmentObject->SetStringField(TEXT("slot"), Garment.Slot);
		GarmentObject->SetStringField(TEXT("mesh"), Garment.Mesh.IsValid() ? Garment.Mesh->GetPathName() : FString());
		GarmentObject->SetStringField(TEXT("why"), Garment.Why);
		if (Garment.SourceMesh.IsValid())
		{
			GarmentObject->SetStringField(TEXT("pieceOf"), Garment.SourceMesh->GetPathName());
			TArray<TSharedPtr<FJsonValue>> Slots;
			for (const int32 Slot : Garment.Sections)
			{
				Slots.Add(MakeShared<FJsonValueNumber>(Slot));
			}
			GarmentObject->SetArrayField(TEXT("materialSlots"), Slots);
		}
		Garments.Add(MakeShared<FJsonValueObject>(GarmentObject));
	}
	Object->SetArrayField(TEXT("garments"), Garments);

	Object->SetBoolField(TEXT("applied"), Result != nullptr);
	if (Result)
	{
		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetNumberField(TEXT("removedPrevious"), Result->RemovedPrevious);
		ResultObject->SetArrayField(TEXT("applied"), ConverterOutcomesJson(Result->Applied));
		ResultObject->SetArrayField(TEXT("failed"), ConverterOutcomesJson(Result->Failed));
		ResultObject->SetArrayField(TEXT("leftAlone"), ConverterOutcomesJson(Result->LeftAlone));
		TArray<TSharedPtr<FJsonValue>> Created;
		for (const TWeakObjectPtr<UObject>& Asset : Context.CreatedAssets)
		{
			if (Asset.IsValid())
			{
				Created.Add(MakeShared<FJsonValueString>(Asset->GetPathName()));
			}
		}
		ResultObject->SetArrayField(TEXT("created"), Created);
		TArray<TSharedPtr<FJsonValue>> Checks;
		for (const FMantleConvertPartCheck& Check : Result->Checks)
		{
			TSharedRef<FJsonObject> CheckObject = MakeShared<FJsonObject>();
			CheckObject->SetNumberField(TEXT("part"), Check.PartIndex);
			CheckObject->SetBoolField(TEXT("canUpload"), Check.bCanUpload);
			if (Check.StandingHeightCm.IsSet())
			{
				CheckObject->SetNumberField(TEXT("standingHeightCm"), Check.StandingHeightCm.GetValue());
			}
			TArray<TSharedPtr<FJsonValue>> Issues;
			for (const FMantleCosmeticIssue& Issue : Check.Issues)
			{
				TSharedRef<FJsonObject> IssueObject = MakeShared<FJsonObject>();
				IssueObject->SetStringField(TEXT("severity"), ConverterSeverityName(Issue.Severity));
				IssueObject->SetStringField(TEXT("message"), Issue.Message);
				IssueObject->SetStringField(TEXT("hint"), Issue.Hint);
				Issues.Add(MakeShared<FJsonValueObject>(IssueObject));
			}
			CheckObject->SetArrayField(TEXT("issues"), Issues);
			Checks.Add(MakeShared<FJsonValueObject>(CheckObject));
		}
		ResultObject->SetArrayField(TEXT("checks"), Checks);
		ResultObject->SetBoolField(TEXT("blocked"), Result->HasBlockers());
		Object->SetObjectField(TEXT("result"), ResultObject);
	}
	return Object;
}

FString FMantleAvatarConverter::RunForAgent(const FString& Path, const FString& Slot, const bool bApply, const FString& TurnOff,
	const FString& Confirm, const FMantleConvertOptions* OptionsOverride)
{
	using namespace MantleAvatarConverterPrivate;
	FString ContractError;
	const FMantleAvatarContract* Contract = FMantleAvatarContract::Get(&ContractError);
	if (!Contract)
	{
		return ConverterRefusal(TEXT("no_contract"), ContractError.IsEmpty() ? FString(TEXT("Mantle's avatar rules (limits.json) could not be read.")) : ContractError);
	}
	FMantleConvertOptions Options = OptionsOverride ? *OptionsOverride : FMantleConvertOptions();
	Options.Slot = Slot.TrimStartAndEnd();
	FMantleConvertContext Context;
	FString Error;
	if (!MakeContextForPath(Path, Options, *Contract, Context, Error))
	{
		return ConverterRefusal(TEXT("not_convertible"), Error);
	}
	TArray<FMantleConvertFix> Fixes = Analyze(Context);
	const TArray<FString> Keys = FixKeys(Fixes);

	// The fixes the caller switches off, by key; a key the list lacks is reported, never guessed at.
	TArray<FString> Off;
	TurnOff.ParseIntoArray(Off, TEXT(","), true);
	TArray<TSharedPtr<FJsonValue>> Unknown;
	for (FString& Wanted : Off)
	{
		Wanted.TrimStartAndEndInline();
		const int32 Found = Keys.IndexOfByPredicate([&Wanted](const FString& Key) { return Key.Equals(Wanted, ESearchCase::CaseSensitive); });
		if (Found == INDEX_NONE)
		{
			Unknown.Add(MakeShared<FJsonValueString>(Wanted));
		}
		else
		{
			Fixes[Found].bEnabled = false;
		}
	}

	// The lines that need confirming the caller confirms, by key; only a line that needs confirming is ticked here, and
	// the "keep the face, choose a body" line (a choice the creator makes, off for a bare body).
	TArray<FString> Confirmed;
	Confirm.ParseIntoArray(Confirmed, TEXT(","), true);
	TArray<TSharedPtr<FJsonValue>> UnknownConfirm;
	for (FString& Wanted : Confirmed)
	{
		Wanted.TrimStartAndEndInline();
		const int32 Found = Keys.IndexOfByPredicate([&Wanted](const FString& Key) { return Key.Equals(Wanted, ESearchCase::CaseSensitive); });
		const auto CreatorChoice = [](const FName Kind)
		{
			return Kind == FMantleAvatarStripFixer::KindFaceOnly || Kind == FMantleAvatarStripFixer::KindFeminineBody
				|| Kind == FMantleAvatarStripFixer::KindMasculineBody;
		};
		if (Found == INDEX_NONE || (!Fixes[Found].bNeedsConfirm && !CreatorChoice(Fixes[Found].Kind)))
		{
			UnknownConfirm.Add(MakeShared<FJsonValueString>(Wanted));
		}
		else
		{
			Fixes[Found].bEnabled = true;
		}
	}

	TOptional<FMantleConvertResult> Result;
	if (bApply)
	{
		Result = Convert(Context, Fixes, Options, *Contract);
	}
	TSharedRef<FJsonObject> Object = ToJson(Context, Fixes, Result.GetPtrOrNull(), *Contract);
	Object->SetArrayField(TEXT("unknownTurnOff"), Unknown);
	Object->SetArrayField(TEXT("unknownConfirm"), UnknownConfirm);
	return ConverterWriteJson(Object);
}
