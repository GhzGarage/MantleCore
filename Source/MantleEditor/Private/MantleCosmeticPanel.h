// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MantleAvatarConverter.h"
#include "MantleCosmeticExport.h"
#include "MantleCosmeticPackage.h"
#include "MantleCosmeticPreview.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"

class FMantleCosmeticPreviewSession;
class FMantleCosmeticUploadSession;
class SMantleCosmeticPreviewViewport;
class UAnimSequence;
class SBox;
class SVerticalBox;
struct FAssetData;
template <typename OptionType> class SComboBox;

/**
 * What the Cosmetics panel's Convert section shows, apart from Slate. Nothing here asks Epic: the
 * build checks the sign-in when Convert runs (FMantleMetaHumanBuild::Preflight), and never opens a login window.
 */
struct FMantleConvertSection
{
	/**
	 * The routes the section offers: Convert alone while UMantleSettings::bAllowAsIsAvatars is off (the default: no route
	 * picker), Convert then As-is while it is on.
	 */
	static TArray<EMantleAvatarRoute> Routes();

	/** Whether the section shows the route picker: more than one route (Routes). */
	static bool ShowsRoutePicker();

	/** The route Convert... analyses on: Picked while the section offers it, else Convert. */
	static EMantleAvatarRoute EffectiveRoute(EMantleAvatarRoute Picked);

	/**
	 * The cloud note shown once, by the Convert button: FMantleMetaHumanBuild::ConsentLine while a ticked line would build
	 * with Epic's cloud (a cloud step's, IsCloudStep, fixable and ticked); empty otherwise.
	 */
	static FString CloudNote(TConstArrayView<FMantleConvertFix> Fixes);

	/** #625: the steps that build with Epic's cloud: MHBuild, and MHSource (the MetaHuman-source route). */
	static bool IsCloudStep(FName FixerId);

	/** A fix line's action as the section shows it: a cloud step's without the cloud note (shown once, by Convert), any other as it is. */
	static FString ActionText(const FMantleConvertFix& Fix);

	/** Whether Convert is on for Fixes: no fixable line that needs confirming (Strip's bare-skin line) is left unticked. */
	static bool CanConvert(TConstArrayView<FMantleConvertFix> Fixes);
};

/**
 * Tools > Mantle > Cosmetics... (A2.4 #283): publish hair, clothing, accessories, avatars and
 * emotes from the editor with one button. The creator picks what it is, the asset,
 * and a name; the plugin finds the body it was authored on, checks everything against Mantle's
 * rules with plain messages, exports, uploads with progress and retries, and follows the item
 * through screening (FMantleCosmeticExport, FMantleCosmeticUploadSession). Native Slate.
 */
class SMantleCosmeticPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMantleCosmeticPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMantleCosmeticPanel() override;

	/** The nomad tab: registered at module startup, opened from the Tools menu. */
	static const FName TabName;
	static void RegisterTab();
	static void UnregisterTab();
	static void OpenTab();

	/**
	 * Maintainer command Mantle.Dev.ConvertPanelShot <Character> <Directory> [Confirm keys]: opens the panel, picks the
	 * character under Convert a character, lists its fixes (ticking the Confirm keys, joined by +: -ExecCmds splits on commas), converts it, then
	 * chooses Use on its first clothing piece that can be an Outfit. After each step the Convert section (and, after Use,
	 * the whole panel) is rendered offscreen into <Directory>/<Character>_{list,converted,used,panel}.png. Nothing uploads.
	 */
	static void DevConvertShot(const TArray<FString>& Args);

	/**
	 * #124: maintainer command Mantle.Dev.CosmeticPanelShot <File.png> [Source=<asset>] [Class=<number>] [SaveCustom=<png>] [Angle] [Exit]: opens the panel
	 * (with Source as the item, as that class) and renders it offscreen, to show the shared Thumbnail section as the cosmetic uploader has it; SaveCustom first
	 * chooses that custom image for the item, saved as the creator's choice would be.
	 */
	static void DevPanelShot(const TArray<FString>& Args);

private:
	void SetClass(EMantleCosmeticClass Class);
	void SetSource(UObject* Source);
	void SetSlot(const FString& Slot);
	void SetAuthoringBody(USkeletalMesh* Body);
	void RebuildSourcePicker();
	void RefreshSlots();
	void RefreshDetails();
	void RefreshOutfitSizes();
	TSharedRef<SWidget> MakeParametricOutfitSection();
	FReply OnCreateParametricOutfit();
	void RefreshOutfitTargets();
	/** Native Outfit authoring inputs must come from the chosen Creator character's same converted build. */
	void RefreshOutfitAuthoringDefaults();
	bool IsOutfitAuthoringCandidate(const FAssetData& Asset, bool bFace) const;

	/** Avatars: one row group per accessory socket and tool socket (hand_rSocket, no scale row), its values the body's own socket (FMantleCosmeticExport::DefaultSocketOverrides). */
	void RefreshSockets();

	/**
	 * #483, garments from a skeletal mesh: one checkbox per material section of the mesh, ticked for the chosen slot
	 * (FMantleOutfitSections::InitialKeep; skin rows disabled). Re-read with the source, re-ticked with the slot.
	 */
	void RefreshOutfitSections();

	/** Draft.KeepSlots from the ticks: unset when every section is ticked (the whole mesh), else the ticked slots. */
	void ApplyOutfitKeep();
	void RefreshClothingColors();
	/** #756: the found colour areas (tick box, name, highlight swatch) above the advanced hand-made section. */
	void AddFoundClothingAreas();
	/** #756: the creator's choice for one found area, made on first change. */
	FMantleClothingColorChoice& ClothingChoice(const FString& MaterialName, const FString& RegionId);
	/** #756: the preview colours: the highlight per found, ticked area (when on), then the hand-picked preview colours. */
	TArray<FMantleClothingColor> ClothingPreviewColorsWithHighlight() const;
	void PickClothingColor(int32 DeclarationIndex, int32 RegionIndex, bool bPreview);

	/**
	 * #594, avatars: the Jaw bone (optional) picker's rows for the chosen body (FMantleEditorActions::ApplyCosmeticJawDefaults):
	 * None, then its bones below head, the default selected. Shown only on a body whose skeleton has the jaw slot.
	 */
	void RefreshJaw();

	/**
	 * #595, avatars: the Secondary motion section's rows for the chosen body (FMantleEditorActions::ApplyCosmeticSecondaryDefaults):
	 * one per slot chain, its start bone (None, then the body's extra bones) and its preset, the suggestion selected. Shown
	 * only on a body whose skeleton has the slots.
	 */
	void RefreshSecondary();

	/** #595: Draft.SecondaryMotion's mapping for a slot chain, or null when the chain is not mapped. */
	FMantleSecondaryMapping* FindSecondaryMapping(const FString& ChainId);

	TSharedRef<SWidget> MakeSourcePicker();
	TSharedRef<SWidget> MakeSection(const FText& Title, const TSharedRef<SWidget>& Content, TAttribute<EVisibility> SectionVisibility = EVisibility::Visible);
	TSharedRef<SWidget> MakeAttachRow(const FText& Label, FVector3f& Value, float Min, float Max);

	/**
	 * A label and three numeric boxes (X, Y, Z) whose sliders span Min..Max; Target is read each time
	 * (null shows empty boxes). bClamp holds typed values inside Min..Max too; without it a value
	 * past them stays as typed, for the check to refuse with the number.
	 */
	TSharedRef<SWidget> MakeVectorRow(const TSharedRef<SWidget>& LabelWidget, TFunction<FVector3f*()> Target, float Min, float Max,
		TAttribute<bool> Enabled, bool bClamp);

	bool IsAllowedSource(const FAssetData& Asset) const;
	/** #584: a skeletal mesh on the emote's skeleton (every mesh while no animation is chosen). */
	bool IsEmoteBodyCandidate(const FAssetData& Asset) const;

	FReply OnCheck();
	FReply OnUpload();
	FReply OnCancel();
	EActiveTimerReturnType TickUpload(double InCurrentTime, float InDeltaTime);

	/** Exports and checks the draft (with a progress dialog); fills Package and the issue list. */
	bool BuildPackage();
	void ShowIssues(const TArray<FMantleCosmeticIssue>& Issues, const TArray<FString>& Notes);
	/** #51: under the checks, the hair head-fit words with one-click Looks good / Adjust (nothing for hair that was not fitted). */
	void ShowHairHeadFit();

	FText GetFilesText() const;
	FText GetStatusText() const;
	FText GetSignInText() const;
	bool IsRunning() const;

	/**
	 * The preview, in a viewport (SMantleCosmeticPreviewViewport). 3D clothing: the garment fitted the way players'
	 * clients fit it (FMantleCosmeticPreviewSession) on the picked body, Mantle's neutral reference body, the
	 * body it was made on, or a catalog avatar by id (#283). Avatars: the body posed by its retargeter (OnPreviewAvatar).
	 */
	TSharedRef<SWidget> MakePreviewSection();

	/** #124: the Thumbnail section every uploader shows, on this draft's choice. */
	TSharedRef<SWidget> MakeThumbnailSection();
	/** Reads the saved thumbnail choice for the draft's source and class (Automatic when none) and redraws the section. */
	void LoadThumbnailChoice();
	/** The creator changed the choice: kept on the draft and saved for this source; the package is built again. */
	void ThumbnailChoiceChanged(const FMantleThumbnailChoice& Choice);
	/** The thumbnail section, to redraw it after a check. */
	TSharedPtr<class SMantleThumbnailSection> ThumbnailSection;
	FReply OnPreview();
	FText GetPreviewStatusText() const;
	void ApplyPreviewAnimation();

	/**
	 * #283: the avatar preview: the creator's avatar body (and its face) posed by its skeleton's IK retargeter from the
	 * UEFN mannequin's idle, as players' clients pose avatars (FMantleAvatarPreviewPlan); the reference pose, with the
	 * reason, when this project lacks Mantle's retarget content.
	 */
	FReply OnPreviewAvatar();
	FReply OnPreviewAccessory();

	/**
	 * #736, overlays (scars, tattoos): the images section (a PNG file in place of a texture asset, the scar normal map, and the
	 * body and face the preview shows), the file picker, and the preview: the mask draped on the converted MetaHuman body or
	 * face with private dynamic instances of the shared skin materials, exactly the bytes that would upload.
	 */
	TSharedRef<SWidget> MakeOverlaySection();
	void BrowseOverlayPng(bool bNormal);
	/** Asks where to save the overlay painting guide for the selected slot, then writes it. */
	void SaveOverlayGuide();
	FString OverlayGuideStatus;
	bool bOverlayGuideFailed = false;
	FReply OnPreviewOverlay();
	void RefreshAccessoryPreview();
	void SaveAccessoryPlacement();
	FString AccessorySettingsFile() const;
	FReply OnAlignAccessoryAxes();
	/** Saved (including disabled), explicitly edited, or initialized from a resolved neutral avatar anchor. */
	bool bAccessoryPlacementInitialized = false;

	/** #283: keeps PreviewPick for the rest of the editor session: the panel reopens on it. */
	void RememberPreviewPick() const;

	/**
	 * #610, the Convert section: a character (a skeletal mesh, or a MetaHuman Creator character's Blueprint or body)
	 * analysed into its fix list (FMantleAvatarConverter::Analyze) with a checkbox per fix, converted into new assets
	 * under its Converted folder (Convert), and each converted part offered as a ready draft (LoadConvertedPart). #625: the
	 * route picker only while As-is uploads are allowed (FMantleConvertSection::ShowsRoutePicker), else Convert alone; the
	 * cloud note by the Convert button; Convert runs behind a progress dialog (it blocks, as Epic's cloud calls do).
	 */
	TSharedRef<SWidget> MakeConvertSection();
	FReply OnAnalyzeConvert();
	FReply OnRunConvert();
	void RefreshConvertRows();

	/** Forgets the fix list and what Convert did (another character or route was picked). */
	void ResetConvert();

	/** #610: the draft for converted part PartIndex: its class, slot, mesh, face, section, authoring body and name, then its check's issues. */
	void LoadConvertedPart(int32 PartIndex);

	/**
	 * A third-party avatar's separate clothing piece (FMantleConvertContext::Garments) as the panel's draft: a MetaHuman
	 * Outfit made on the MetaHuman Convert made of the avatar's body (FMantleGarmentOutfit::MakeFromWork). The first Use
	 * makes every piece that can be one, on one copy of that MetaHuman; later ones only load theirs.
	 */
	void UseConvertGarment(int32 GarmentIndex);

	/** What Convert... analyses: picked in the section, else the draft's asset. */
	UObject* GetConvertSelection() const;

	TWeakObjectPtr<UObject> ConvertSource;
	/** #625: the route picked while the section offers more than Convert (FMantleConvertSection::EffectiveRoute). */
	EMantleAvatarRoute ConvertRoute = EMantleAvatarRoute::Convert;
	TSharedPtr<FMantleConvertContext> ConvertContext;
	TArray<FMantleConvertFix> ConvertFixes;
	TOptional<FMantleConvertResult> ConvertResult;
	FString ConvertStatus;
	bool bConvertFailed = false;
	TSharedPtr<SVerticalBox> ConvertBox;

	FMantleCosmeticDraft Draft;
	TOptional<FMantleCosmeticPackage> Package;
	TSharedPtr<FMantleCosmeticUploadSession> Session;
	FString ContractError;

	TArray<TSharedPtr<EMantleCosmeticClass>> ClassOptions;
	TArray<TSharedPtr<FString>> SlotOptions;
	TArray<TSharedPtr<FString>> CardsOptions;
	TArray<TSharedPtr<FString>> SectionOptions;
	TArray<TSharedPtr<FString>> OutfitSizeOptions;
	FString OutfitSizeError;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> OutfitSizeCombo;
	/** Native source adaptation creates transient body/head/garment assets; hold them while the viewport uses them. */
	TArray<TStrongObjectPtr<UObject>> OutfitPreviewKeepAlive;
	/** The original combined source body/head, before conversion, used to author an ordinary garment. */
	TWeakObjectPtr<USkeletalMesh> ParametricSourceBodyHead;
	TArray<TStrongObjectPtr<UObject>> AuthoredOutfitKeepAlive;
	FString OutfitAuthoringStatus;
	TSharedPtr<SVerticalBox> OutfitTargetsBox;
	/** Hair from a groom's cards: the Hair colour field as typed (#RRGGBB); Draft.HairColor is set when it parses. */
	FString HairColorText;
	bool bCanGenerateHairCards = false;
	FString HairGenerationStatus;
	TArray<TSharedPtr<FString>> RoleOptions;
	/** Avatars' Publish as: "" (a new item). */
	TArray<TSharedPtr<FString>> PublishAsOptions;
	/** #594, avatars' Jaw bone (optional): "" (None) then the body's bones below head; shown while bJawSlotOffered. */
	TArray<TSharedPtr<FString>> JawOptions;
	bool bJawSlotOffered = false;
	/** #595, avatars' Secondary motion: "" (None) then the body's extra bones, and the preset ids; shown while bSecondaryOffered. */
	TArray<TSharedPtr<FString>> SecondaryBoneOptions;
	TArray<TSharedPtr<FString>> SecondaryPresetOptions;
	bool bSecondaryOffered = false;
	TSharedPtr<SVerticalBox> SecondaryBox;

	TSharedPtr<SComboBox<TSharedPtr<FString>>> SlotCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> JawCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> CardsCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> SectionCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> PublishAsCombo;
	TSharedPtr<SBox> SourcePickerBox;
	TSharedPtr<SVerticalBox> RolesBox;
	TSharedPtr<SVerticalBox> SocketsBox;
	TSharedPtr<SVerticalBox> IssuesBox;

	/** #483: the garment mesh's sections (material slots at LOD 0), their slot indices, pieces and ticks, by row. */
	TArray<FMantleOutfitSection> OutfitSections;
	TArray<int32> OutfitSlots;
	TArray<EMantleOutfitPiece> OutfitPieces;
	TArray<bool> OutfitKeep;
	TSharedPtr<SVerticalBox> OutfitBox;
	TSharedPtr<SVerticalBox> ClothingColorsBox;
	TArray<FMantleClothingColor> ClothingPreviewColors;
	TArray<FString> ClothingMaterialNames;
	/** #756: the last preview build's colour area detection; bClothingAreasKnown once a preview has looked. */
	TArray<FMantleClothingColorDetection> DetectedClothingColors;
	bool bClothingAreasKnown = false;
	bool bHighlightClothingAreas = true;
	bool bClothingColorsAdvancedOpen = false;

	TSharedPtr<FMantleCosmeticPreviewSession> PreviewSession;
	TSharedPtr<SMantleCosmeticPreviewViewport> PreviewViewport;
	/** #283: who the garment is previewed on, and the picker's rows (FMantlePreviewPick::Options). */
	FMantlePreviewPick PreviewPick;
	TArray<TSharedPtr<FMantlePreviewPick>> PreviewPickOptions;
	bool bPreviewAnimate = false;
	TWeakObjectPtr<UAnimSequence> PreviewAnimation;
	/** Why the preview could not read the draft (no garment, no body, no CPU data); empty when it could. */
	FString PreviewInputError;
	/** #736: the converted body and face an overlay previews on (unset: Mantle's reference body and face), and the preview's line. */
	TWeakObjectPtr<USkeletalMesh> OverlayPreviewBody;
	TWeakObjectPtr<USkeletalMesh> OverlayPreviewFace;
	FString OverlayPreviewStatus;
	bool bOverlayPreviewFailed = false;
	/** #283: the avatar preview's line for the creator, and whether it says what went wrong. */
	FString AvatarPreviewStatus;
	bool bAvatarPreviewFailed = false;
};
