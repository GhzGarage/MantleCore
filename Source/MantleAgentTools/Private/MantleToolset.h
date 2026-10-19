// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "ToolsetRegistry/ToolsetDefinition.h"

#include "MantleToolset.generated.h"

class UToolCallAsyncResultString;

/**
 * Mantle: check, preview, export, publish and follow a Mantle place from the level open in the
 * Unreal editor, and check and upload cosmetics (clothing, hair, avatars) from skeletal meshes.
 * Place tools work on the editor's current level. Pass every parameter a tool lists, with ""
 * for one that may be empty: a call that leaves them out is refused before the tool runs.
 * Every tool returns one JSON object as text.
 * A tool that could not do its job says so in that object ("ok": false, with "refused" a short
 * code and "message" a sentence for the creator). Tools that talk to Mantle need the creator to
 * be signed in (Tools > Mantle > Sign in with creator token...); no tool accepts or returns a
 * token, so never ask the creator for one.
 */
UCLASS()
class UMantleToolset final : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Check this place: runs Mantle's validator on the open level, exactly as Tools > Mantle >
	 * Check this place does, and returns every issue. Read-only.
	 * @return JSON: {"ok", "level", "ready" (true when nothing blocks publishing), "errors",
	 * "warnings", "issues": [{"severity": "error"|"warning", "actor" (outliner label, empty for
	 * place-wide issues), "message" (what is wrong), "remedy" (what to do about it)}]}.
	 */
	UFUNCTION(meta = (AICallable))
	static FString CheckPlace();

	/**
	 * Export preview: exports the open level to Mantle's place format in memory and summarises
	 * it. Nothing is written or uploaded; meshes and materials from the project carry provisional
	 * ids until a publish uploads them. Read-only.
	 * @return JSON: {"ok", "level", "exported" (false when the level cannot be expressed; see
	 * "errors"), "placeJsonBytes", "entities": {"total", "byType": {type: count}}, "assets":
	 * {"total", "bytes", "byKind": {kind: count}}, "scripts", "scriptApi": {"major", "minor",
	 * "analysed"}, "errors": [{"path", "message"}]}.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ExportPreview();

	/**
	 * Publish the open level to Mantle and make it live for players, exactly as Tools > Mantle >
	 * Publish does: check, upload what Mantle does not have yet, wait for processing, publish. It
	 * runs in the background under the Mantle Publish panel, and this call completes when it ends.
	 * This is a public action: players can join the result.
	 * This should ONLY be called after getting explicit direction or permission from the user,
	 * for this specific publish. Refused, with nothing sent, when the creator is not signed in, no
	 * level is open, or a publish is already running. Vehicles in the level become PRIVATE library
	 * items stored in the place (an agent cannot make the rights declaration a public one needs).
	 * @param Notes Revision notes shown with this version. Always pass it; "" for none.
	 * @return JSON: {"ok", "placeId", "revision", "live", "waitingForReview", "cancelled",
	 * "message", "issues": [{"severity", "actor", "message", "remedy"}]}, or a refusal.
	 */
	UFUNCTION(meta = (AICallable))
	static UToolCallAsyncResultString* PublishPlace(const FString& Notes = TEXT(""));

	/**
	 * List the places the signed-in creator owns on Mantle (the first page). Read-only.
	 * @return JSON: {"ok", "places": [the API's place objects], "more" (true when there are more
	 * pages)}, or a refusal when not signed in or Mantle could not be reached.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ListMyPlaces();

	/**
	 * Upload status of the running or last publish in this editor, per asset, as the Mantle
	 * Publish panel shows it. No network. Read-only.
	 * @param AssetName Part of an Unreal asset's name or path, or an ast_ id, to report only
	 * matching assets. Always pass it; "" reports every asset.
	 * @return JSON: {"ok", "publish": null when none has run, else {"phase" (idle, checking,
	 * converting, uploading, crediting, scripts, exporting, library, creatingPlace, saving,
	 * publishing, done, failed or cancelled; "library" is a go-live turning uploaded vehicles into
	 * library items), "running", "goLive", "progress", "status", "placeId", "revision"}, "assets": [{"name", "state" (queued,
	 * uploading, processing, approved or refused), "assetId", "bytes", "alreadyOnMantle",
	 * "inReview", "code", "message", "cookError"}]}.
	 */
	UFUNCTION(meta = (AICallable))
	static FString UploadStatus(const FString& AssetName = TEXT(""));

	/**
	 * Explain an issue: runs Check this place on the open level and returns, in full, every issue
	 * whose message, actor or remedy contains Query (issues have no codes; a phrase from a
	 * CheckPlace message is the usual query). Read-only.
	 * @param Query Part of an issue's message, its actor's label, or its remedy.
	 * @return JSON: {"ok", "query", "matches": [{"severity", "actor", "message", "remedy"}]}.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ExplainIssue(const FString& Query);

	/**
	 * The script API version the open level's scripts need, as a publish records it in the place
	 * (the lowest minor that covers every script), and each script's own need. A script the scan
	 * cannot follow makes the place record the editor's newest minor; it is listed with why. Read-only.
	 * @return JSON: {"ok", "scripts", "recorded": {"major", "minor"} (both 0 without scripts),
	 * "editorMinor", "analysed", "neededFor", "perScript": [{"path", "minor", "neededFor",
	 * "analysed", "problem", "remedy"}]}.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ScriptApiRequirement();

	/**
	 * Preview the place as players receive it, exactly as Tools > Mantle > Check what players
	 * receive does: exports the open level, reads the file back and rebuilds it in a scratch world
	 * (meshes and materials already on Mantle load as players get them when signed in), reports,
	 * then tears the rebuilt world down. Nothing is uploaded or changed. Refused when no level is
	 * open or a preview is already running.
	 * @return JSON: {"ok", "level", "built", "sourceEntities", "previewActors", "notUploaded",
	 * "leftOut": [lines], "summary", "issues": [{"severity", "actor", "message", "remedy"}]}.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewPlace();

	/**
	 * Measure an avatar's head the way an avatar upload fits its hat socket (acc_hat): the cranium (the body's and
	 * face's vertices on the head above its joint) as a box in the hat socket's frame, and the socket override an
	 * upload would send against the plugin's reference head. Read-only.
	 * @param BodyPath The avatar's body skeletal mesh.
	 * @param FacePath Its face skeletal mesh; "" for a body that carries its own head. Always pass it.
	 * @return JSON: {"ok", "vertices", "min", "max", "size" ([x, y, z] cm), "reference" (bool), "location", "scale"
	 * (the fitted socket, when there is a reference)}, or a refusal (not_found, not_measured).
	 */
	UFUNCTION(meta = (AICallable))
	static FString MeasureHead(const FString& BodyPath, const FString& FacePath = TEXT(""));

	/**
	 * Check a cosmetic: the Cosmetics panel's "Check only" on a skeletal mesh or a MetaHuman Outfit
	 * (an imported .mhpkg outfit, checked as clothing), with the panel's defaults (the authoring body
	 * it finds, generated LODs, thumbnail). Exports in memory; nothing is uploaded. Read-only.
	 * @param AssetPath The skeletal mesh or Outfit, e.g. /Game/Clothing/SKM_Jacket or /Game/Outfits/Survivor/OA_SurvivorOutfit.
	 * @param Slot The item's slot (a garment slot such as top, a hair slot such as hair, avatar, or, for a MetaHuman
	 * Outfit hat or other rigid piece, an accessory slot such as acc_hat: it uploads fitted to the reference head);
	 * "" takes the panel's default, the first 3D clothing slot. Always pass it.
	 * @param Name The item's name; "" takes the asset's name. Always pass it.
	 * @param Sections The sections to upload, as the panel's section ticks: material or material slot names separated
	 * by commas (e.g. "MI_Coat,MI_Shirt" for the top of a mesh holding a whole outfit); "" keeps the panel's default.
	 * Always pass it.
	 * A MetaHuman Outfit uploads as clothing with garment variants made automatically: it is resized onto Epic's standard
	 * body types (by default the medium-height underweight, normal and overweight bodies of one sex, named like
	 * "f_med_nrw"), made in memory, so nothing needs converting and no Epic sign-in is needed. The sex follows FitSex;
	 * with "" it is chosen from the Outfit's own sizes. A note in "notes" says which and why.
	 * @param FitSex For a MetaHuman Outfit: the style of body the clothing was made on, as the panel's "Made for":
	 * "female" (women's bodies, f_...), "male" (men's bodies, m_...), or "" / "auto" to let Mantle choose from the
	 * Outfit's sizes. Ask the creator when you do not know; ignored for anything but clothing from an Outfit.
	 * @return JSON: {"ok", "asset", "class", "slot", "name", "canUpload" (no errors), "issues":
	 * [{"severity": "error"|"warning"|"info", "message", "hint"}], "notes": [lines], "files":
	 * [{"key", "label", "bytes"}]}, or a refusal (not_found, not_skeletal_mesh, unknown_slot,
	 * outfit_not_clothing, accessory_needs_outfit, unknown_section, unknown_fit_sex).
	 */
	UFUNCTION(meta = (AICallable))
	static FString CheckCosmetic(const FString& AssetPath, const FString& Slot = TEXT(""), const FString& Name = TEXT(""), const FString& Sections = TEXT(""),
		const FString& FitSex = TEXT(""));

	/**
	 * Upload a cosmetic to Mantle as a new item, exactly as the Cosmetics panel's Upload to Mantle
	 * does: checks it (as CheckCosmetic), uploads the files Mantle does not have, waits for them to
	 * be processed, creates the item and follows it through screening. This call completes when
	 * that ends. This is a public action: other players can see and wear the item.
	 * This should ONLY be called after getting explicit direction or permission from the user,
	 * for this specific upload. A MetaHuman Outfit uploads with garment variants fitted to Epic's standard body types,
	 * made automatically (as CheckCosmetic says); no converted MetaHuman is needed. Refused, with nothing sent, when the creator is not signed in, the
	 * check finds errors, or an upload started here is still running.
	 * @param AssetPath The skeletal mesh or Outfit, as for CheckCosmetic.
	 * @param Slot The item's slot, as for CheckCosmetic.
	 * @param Name The item's name players see; "" takes the asset's name. Always pass it.
	 * @param Sections The sections to upload, as for CheckCosmetic. Always pass it.
	 * @param ItemId "" creates a new item. An id (itm_...) publishes a new version into that existing item instead,
	 * which the signed-in creator must own; the slot must match the item's, and the new version is screened again.
	 * @param FitSex For a MetaHuman Outfit, the body it was made on, as for CheckCosmetic: "female", "male", or "" / "auto".
	 * @return JSON: {"ok", "itemId", "itemStatus", "moderationState", "mayClipInMotion",
	 * "message", "files": [{"label", "state", "assetId", "alreadyOnMantle", "message"}]}, or a
	 * refusal (with "issues" when the check found errors).
	 */
	UFUNCTION(meta = (AICallable))
	static UToolCallAsyncResultString* UploadCosmetic(const FString& AssetPath, const FString& Slot, const FString& Name = TEXT(""), const FString& Sections = TEXT(""), const FString& ItemId = TEXT(""),
		const FString& FitSex = TEXT(""));

	/**
	 * Convert a character so it can be uploaded, as the Cosmetics panel's Convert does: lists what
	 * the plugin would fix (the fix list), and with Apply writes the fixes as NEW assets in a
	 * Converted folder next to the character (the creator's own assets are never changed; an
	 * earlier conversion of the same character there is replaced), then runs the upload checks on
	 * each converted part. Nothing is uploaded. Without Apply it is read-only.
	 * @param AssetPath A skeletal mesh (/Game/Characters/SKM_Hero), a MetaHuman Creator character's
	 * Blueprint or body (/Game/CharacterAssemblies/Vampire/BP_Vampire), or such a character's folder
	 * (/Game/CharacterAssemblies/Vampire): a character yields its avatar, clothing and hair as parts.
	 * @param Slot For a single skeletal mesh, the slot it converts for (avatar, top, bottom,
	 * footwear, hair, eyebrows, eyelashes, facial_hair); "" takes its slot in its character, else
	 * avatar. Ignored for a whole character. Always pass it.
	 * @param bApply False lists the fixes only; true converts. Always pass it.
	 * @param TurnOff Comma-separated fix keys (from "fixes"[].key, e.g. "Triangles.0.Mesh") to leave
	 * alone; every other fix keeps its default ("enabled"). "" for none. Always pass it.
	 * @param Confirm Comma-separated keys of the fixes that need confirming ("needsConfirm", e.g.
	 * "Strip.0.BareSkin": the kept parts are bare skin) that you confirm. Convert does nothing while
	 * one is unconfirmed. Only confirm what the person asking told you is true. "" for none.
	 * @return JSON: {"ok", "source", "assembly", "outputFolder", "route", "parts": [{"index", "slot", "class",
	 * "name", "mesh", "original", "face", "section", "skeleton", "authoringBody", "authoringFace",
	 * "included", "clothAsset"}], "fixes": [{"key", "fixer", "kind", "part", "problem", "action",
	 * "canFix", "enabled", "needsConfirm"}], "notes", "unknownTurnOff", "unknownConfirm", "applied", and with Apply "result":
	 * {"removedPrevious", "applied", "failed", "leftAlone" (fixes with a "reason"), "created" (asset
	 * paths), "checks": [{"part", "canUpload", "standingHeightCm", "issues": [{"severity",
	 * "message", "hint"}]}], "blocked"}}, or a refusal (not_convertible, no_contract).
	 */
	UFUNCTION(meta = (AICallable))
	static FString ConvertCosmetic(const FString& AssetPath, const FString& Slot, bool bApply, const FString& TurnOff = TEXT(""), const FString& Confirm = TEXT(""));
};
