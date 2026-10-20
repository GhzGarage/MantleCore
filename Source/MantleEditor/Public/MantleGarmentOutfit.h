// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MantleGlbWriter.h"

class USkeletalMesh;
struct FMeshDescription;
struct FReferenceSkeleton;

/** One garment FMantleGarmentOutfit made into an Outfit. */
struct MANTLEEDITOR_API FMantleGarmentOutfitPiece
{
	/** The creator's garment (never modified) and the Outfit made from it; Outfit is empty when it failed (Error says why). */
	FString Garment;
	FString Outfit;
	FString Error;

	int32 Vertices = 0;

	/** Garment vertices that had no MetaHuman skin within NearCm, and the average distance from a vertex to the skin (cm). */
	int32 FarVertices = 0;
	double MeanSkinCm = 0.0;

	/** What pushing it out of the MetaHuman's skin did (FMantleRefitPushOut::ToString). */
	FString PushOut;
};

/** What FMantleGarmentOutfit::Make did. */
struct MANTLEEDITOR_API FMantleGarmentOutfitResult
{
	/** Where everything went: <body folder>/Converted/<body>_Outfit. */
	FString OutputFolder;

	/** The MetaHuman Character conformed to the body, and the copy of its merged body and head each Outfit was authored on. */
	FString Character;
	FString AuthoringBody;

	/** The A-pose step's default-height scale k (the garment is scaled by it too). */
	double InputScale = 1.0;

	/** How the posed body was moved onto the MetaHuman (cm), and how far its skin then is from the MetaHuman's on average (cm). */
	FVector3f Offset = FVector3f::ZeroVector;
	double BodyGapCm = 0.0;

	/** The conformed MetaHuman's limbs turned onto MetaHuman's A-pose: "upperarm_l 7.4, ..." (degrees). */
	FString APoseTurns;

	TArray<FMantleGarmentOutfitPiece> Pieces;
	TArray<FString> Notes;
};

/**
 * "Make Outfit from garment": a third-party character's separate clothing mesh, plus that character's bare body, as a
 * real MetaHuman Outfit (UChaosOutfitAsset) whose one authored size is a MetaHuman built from that same body. The
 * Outfit then uploads like any MetaHuman Outfit: FMantleParametricOutfit resizes it onto Epic's standard bodies.
 *   1. The body goes through Convert's own MetaHuman steps only (Strip, MHPose, MHConform; no build): a MetaHuman Character
 *      conformed to the body in MetaHuman's A-pose at the default height. A body with no USkeleton gets one first, on a copy.
 *   2. Each garment is posed the way the body was: its vertices skinned by the A-pose clip's bones (by name; a garment bone
 *      the body lacks follows its parent), scaled by the same k, and moved by the offset that puts the posed body on the
 *      MetaHuman's skin (PoseMesh, AlignTranslation).
 *   3. Its skin weights are taken from the MetaHuman's merged body and head (TransferWeights), so it is on MetaHuman's bones.
 *   4. The MetaHuman's arms and legs are turned onto MetaHuman's own A-pose (TurnToTemplate), body and garment alike, and
 *      the garment is pushed out of the skin where it sits under it (FMantleAvatarRefit::PushOutOfBody, as the refit does).
 *   5. FMantleParametricOutfitAuthoring::Create makes the Outfit, its source body that turned body (saved as a copy).
 * The creator's assets are never modified; everything is saved under the output folder.
 */
struct MANTLEEDITOR_API FMantleGarmentOutfit
{
	/**
	 * Package metadata key on every Outfit Make writes (the garment it was made from). FMantleParametricOutfit::Prepare
	 * pushes such an Outfit's resized sizes out of their body's skin too, as Make pushes the authored size.
	 */
	static const FName MadeFromGarmentKey;

	/** A garment vertex further than this from every MetaHuman skin vertex is counted as far (FarVertices). */
	static constexpr double NearCm = 10.0;

	/** The posed body must land within this of the A-pose step's own posed copy (bounds, cm), or the garment's pose is not the body's. */
	static constexpr double PoseCheckCm = 1.0;

	/** Skin vertices blended per garment vertex. */
	static constexpr int32 TransferNeighbours = 4;

	/**
	 * Pure (tested): Mesh's vertices (bind pose, Mesh.Joints' reference pose) skinned into Posed (one component-space
	 * transform per joint of Mesh.Joints), then scaled by Scale about the origin; normals and tangents turned with them.
	 * Joints and weights are left as they were. False with why when Posed does not match the joints or a weight names no joint.
	 */
	static bool PoseMesh(FMantleGlbMesh& Mesh, TConstArrayView<FTransform> Posed, double Scale, FString& OutError);

	/**
	 * Pure (tested): the translation that moves From onto To (iterated nearest points, pairs further than 20 cm left out),
	 * and OutMeanCm, the average distance from a point of From, so moved, to its nearest point of To.
	 */
	static FVector3f AlignTranslation(TConstArrayView<FVector3f> From, TConstArrayView<FVector3f> To, double& OutMeanCm);

	/**
	 * Pure (tested): Garment takes Body's joints, and each of its vertices the blend of the skin weights of its
	 * TransferNeighbours nearest Body vertices (by inverse distance), the heaviest 8 kept and renormalised. OutFar counts
	 * vertices with no Body vertex within NearCm; OutMeanCm is the average distance to the nearest one. False with why when
	 * Body has no skin.
	 */
	static bool TransferWeights(FMantleGlbMesh& Garment, const FMantleGlbMesh& Body, int32& OutFar, double& OutMeanCm, FString& OutError);

	/**
	 * Steps 1 to 4 for Body and each of Garments. TurnOff and Confirm are Convert's fix keys (comma-separated), as the
	 * toolset's ConvertCosmetic takes them; "keep the face, choose a body" never applies here (the garment needs its own body).
	 * OutConvertJson is the converter's fix list and result (FMantleAvatarConverter::ToJson). False with OutRefusal ("code"
	 * and a sentence) when the body could not be made a MetaHuman; a garment that fails is reported in its piece.
	 */
	static bool Make(USkeletalMesh& Body, TConstArrayView<USkeletalMesh*> Garments, const FString& TurnOff, const FString& Confirm,
		FMantleGarmentOutfitResult& OutResult, FString& OutConvertJson, FString& OutRefusalCode, FString& OutRefusal);

	/**
	 * A saved copy of Source as Folder/<Name> (made unique), for meshes MetaHuman Creator makes in memory: a transient
	 * skeleton is replaced by a saved one of the same bones, and a transient material by the first saved material it is an
	 * instance of. Null with why.
	 */
	static USkeletalMesh* SaveCopy(USkeletalMesh& Source, const FString& Folder, const FString& Name, FString& OutError,
		const TFunction<bool(FMeshDescription&, FReferenceSkeleton&, FString&)>& Edit = nullptr);

	/**
	 * Pure (tested): Component (one component-space transform per raw bone of Reference) turned so each limb bone of Limbs()
	 * points as it does in Template (bones by name), root to leaf: a bone turns about its joint by the smallest rotation,
	 * and everything below it follows. The conform keeps the arm angle of the body it was given; Epic's standard bodies,
	 * and the resize that fits an Outfit onto them, stand in MetaHuman's own A-pose. OutLines: "upperarm_l 7.4", one per
	 * limb bone, degrees.
	 */
	static TArray<FTransform> TurnToTemplate(const FReferenceSkeleton& Reference, TConstArrayView<FTransform> Component,
		const FReferenceSkeleton& Template, TConstArrayView<FTransform> TemplateComponent, TArray<FString>& OutLines);

	/** The limb bones TurnToTemplate lines up, each with the child it points at, root to leaf (MetaHuman's names). */
	static const TArray<TPair<FName, FName>>& Limbs();

	/** Component-space transforms of Reference's raw bones in their reference pose. */
	static TArray<FTransform> ComponentPose(const FReferenceSkeleton& Reference);

	/** The toolset's MakeOutfitFromGarment: Make for asset paths (Garments comma-separated), as one JSON object. */
	static FString RunForAgent(const FString& BodyPath, const FString& GarmentPaths, const FString& TurnOff, const FString& Confirm);
};
