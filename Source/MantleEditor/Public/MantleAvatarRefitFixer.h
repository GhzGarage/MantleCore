// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MantleAvatarConvertFixer.h"
#include "MantleGarmentFitter.h"
#include "MantleNailSeating.h"

class FMantleAvatarContract;
class USkeletalMesh;
struct FMeshDescription;

/** #610 Refit: how a garment's shape is carried from the body it was made on to the body it is refitted onto. */
enum class EMantleRefitCorrespondence : uint8
{
	/** The garment's source body is the authoring body: no warp, only the re-bind. */
	Identity,
	/** The two bodies share a vertex topology at some LOD pair (same vertex count, near-identical UVs): vertex i of one is vertex i of the other. */
	Dense,
	/** Otherwise: the shared MetaHuman body skeleton's joints, and points on its bone segments, as landmarks. */
	Joints,
};

MANTLEEDITOR_API const TCHAR* LexToString(EMantleRefitCorrespondence Path);

/** What refitting one garment or hair mesh onto an authoring body does (FMantleAvatarRefit::Plan). */
struct MANTLEEDITOR_API FMantleRefitPlan
{
	/** The garment's raw reference bones, parents before children. */
	TArray<FName> Names;
	TArray<int32> Parents;

	/**
	 * Component space, one per garment bone: the garment's own reference pose; the pose on its source body (shared
	 * bones at the source body's reference pose, the rest keeping their local transforms), which the vertices are
	 * first re-bound to; and the same on the authoring body, the refitted mesh's reference pose.
	 */
	TArray<FTransform> GarmentComponent;
	TArray<FTransform> SourceComponent;
	TArray<FTransform> TargetComponent;

	/** TargetComponent in bone space: what the refitted mesh's reference skeleton gets. */
	TArray<FTransform> TargetLocal;

	/** Garment bones the authoring body has, which take its pose. */
	int32 NumShared = 0;

	EMantleRefitCorrespondence Path = EMantleRefitCorrespondence::Identity;

	/** Dense: the LOD of the source body and of the authoring body that share the topology. */
	int32 SourceLOD = INDEX_NONE;
	int32 TargetLOD = INDEX_NONE;

	/** RBF landmarks the field was made from (0 on the identity). */
	int32 NumLandmarks = 0;

	/** Source body space to authoring body space (FMantleRBFInterpolation, the runtime fitter's RBF); bIdentity on the identity. */
	FMantleWarpField Field;

	/** The largest distance a garment vertex moves (every LOD, re-bind and warp together), cm, and on which LOD. */
	double MaxMoveCm = 0.0;
	int32 MaxMoveLOD = INDEX_NONE;

	/** P through the field (P itself on the identity). */
	FVector3f Warp(const FVector3f& P) const;
};

/**
 * #610, the converter's Refit step: a garment (or hair) made on a body without Mantle's reference topology, or bound
 * to another body's reference pose, carried onto an authoring body that has the topology (a reference MetaHuman body
 * in the project), so the upload accepts it and every client can warp it from that body onto its wearer.
 *
 * Two steps on every LOD's mesh description, one new mesh:
 *   1. the re-bind (FMantleAvatarRepose::RebindMeshDescription): from the garment's own reference pose to the source
 *      body's (a no-op when the garment was bound to its body's pose, as a MetaHuman Creator assembly's are);
 *   2. the warp: every vertex through an RBF field (FMantleRBFInterpolation, the one the client fits garments with)
 *      that takes the source body's surface onto the authoring body's, the dense vertex correspondence when the two
 *      share a topology, else the shared skeleton's joints. Normals, tangents and morph deltas are carried the way the
 *      runtime fitter carries them (the field applied to a 1 cm offset along them, tangents re-orthogonalised, the
 *      binormal sign kept); UVs, skin weights, sections and triangles are untouched.
 * The refitted mesh's reference pose is the authoring body's for every bone the body has; the garment's own extra
 * bones keep their local transforms. Posed in the authoring body's reference pose, the garment sits where the warp put it.
 */
class MANTLEEDITOR_API FMantleAvatarRefit
{
public:
	/** No vertex moves further: a garment that would is probably not made for a body like the authoring body. */
	static constexpr double MaxMoveCm = 25.0;

	/** Dense landmarks picked (farthest-point sampling) from the source body's vertices. */
	static constexpr int32 DensePoints = 500;

	/** The joint landmarks are capped at this many (farthest-point sampling). */
	static constexpr int32 MaxJointLandmarks = 400;

	/** How far two bodies' UVs may be apart, per vertex, to count as the same topology (four texels of a 2048 map). */
	static constexpr float DenseUVTolerance = 1.0f / 512.0f;

	/** Bones every refit needs in the garment and the authoring body: the MetaHuman body skeleton's spine to head. */
	static const TArray<FName>& RequiredBones();

	/**
	 * #740: slots whose pieces sit on one part of the body (shaped nails on the hands) and may carry a skeleton that stops
	 * there. They skip the RequiredBones rule; every weighted bone must still exist on the MetaHuman skeleton, so they are
	 * bound to it all the same. Case-sensitive: API slot ids are lower case.
	 */
	static bool IsOverlaySlot(const FString& Slot);

	/**
	 * The skeleton rule: every RequiredBones in GarmentBones and in BodyBones, and every bone in Weighted (the
	 * garment's bones that carry weight) in BodyBones, so the refitted garment is bound to the authoring body's
	 * skeleton. False with a sentence naming the first missing bone. Pure (tested). bOverlay (IsOverlaySlot) skips
	 * the RequiredBones in the garment (the body still needs them) and keeps the Weighted rule.
	 */
	static bool CheckBones(TConstArrayView<FName> GarmentBones, TConstArrayView<FName> Weighted, TConstArrayView<FName> BodyBones, FString& OutError,
		bool bOverlay = false);

	/** The bones LOD 0 of Mesh gives weight to (its imported model), in reference skeleton order; every bone when it has none. */
	static TArray<FName> WeightedBones(const USkeletalMesh& Mesh);

	/**
	 * The upload's bind check (FMantleCosmeticExport::Build, cook.checkGarmentBindPose): the first bone of Garment's LOD 0
	 * that carries weight whose reference pose (component space) is outside the contract's bind tolerance of Body's.
	 * True, with the bone, distance and angle, when one is; false when every weighted bone Body has is within it.
	 */
	static bool FindBindOffset(const USkeletalMesh& Garment, const USkeletalMesh& Body, const FMantleAvatarContract& Contract,
		FName& OutBone, double& OutCm, double& OutDegrees);

	/**
	 * The first LOD pair of Source and Target with the same vertex count and UVs within DenseUVTolerance (render
	 * vertices, as the fitter reads them), and their positions. False when none shares the topology. #620: a LOD whose
	 * reference topology is its mesh description's (FMantleCosmeticExport::HasReferenceTopology: a catalog reference body,
	 * a body written in its order) is read from its description, in the reference order, never in the build's render order.
	 */
	static bool FindDenseCorrespondence(const USkeletalMesh& Source, const USkeletalMesh& Target, int32& OutSourceLOD, int32& OutTargetLOD,
		TArray<FVector3f>* OutSourcePositions = nullptr, TArray<FVector3f>* OutTargetPositions = nullptr);

	/**
	 * The RBF field taking every From onto its To (FMantleRBFInterpolation::GenerateWeights over all of From, then
	 * GenerateMeshSamples). False with why when there are fewer than four landmarks or the counts differ.
	 */
	static bool MakeField(TConstArrayView<FVector3f> From, TConstArrayView<FVector3f> To, FMantleWarpField& OutField, FString& OutError);

	/**
	 * The refit of Garment onto Target: the poses, the field and the largest vertex move (read from the garment's
	 * imported LOD models). SourceBody is the body the garment was made on (the character's own); null when unknown,
	 * and then the garment's own reference pose stands for it (joint landmarks only). bAllowDense false forces the
	 * joint landmarks (tests). False with why when the skeleton rule fails or the field cannot be made; a move over
	 * MaxMoveCm is not a failure here (Out.MaxMoveCm says it). bOverlay: the piece is an IsOverlaySlot one (CheckBones).
	 */
	static bool Plan(const USkeletalMesh& Garment, const USkeletalMesh* SourceBody, const USkeletalMesh& Target, bool bAllowDense,
		FMantleRefitPlan& Out, FString& OutError, bool bOverlay = false);

	/**
	 * One LOD's mesh description refitted by InPlan (the class comment's two steps). OutMaxMoveCm gets its largest vertex
	 * move. False with why when it has no skin weights.
	 */
	static bool RefitMeshDescription(FMeshDescription& Mesh, const FMantleRefitPlan& InPlan, double& OutMaxMoveCm, FString& OutError);

	/**
	 * A NEW skeletal mesh, PackagePath/AssetName, Garment refitted by InPlan on every LOD (FMantleAvatarRepose::
	 * WriteWithPose), with InPlan.TargetLocal as its reference pose. Garment is never modified. Null with why when a LOD
	 * cannot be refitted or a vertex would move more than MaxMoveCm. OutMaxMoveCm: the largest move written.
	 * SeatBody (#740, nails): when given, each LOD is then seated on it (FMantleNailSeating), after the move check;
	 * OutSeating gets the last LOD's result.
	 */
	static USkeletalMesh* WriteRefit(USkeletalMesh* Garment, const FMantleRefitPlan& InPlan, const FString& PackagePath, const FString& AssetName,
		double& OutMaxMoveCm, FString& OutError, const FMantleSkinnedMeshLOD* SeatBody = nullptr, FMantleNailSeatingResult* OutSeating = nullptr);

	/**
	 * The fix list's words when Mantle's reference body cannot be measured or loaded and no more specific
	 * provider error is available.
	 */
	static FString NoReferenceBodyText();
};

/**
 * MantleAvatarFixerIds::Refit, after Assembly and before Triangles. Analyze proposes, for each garment and hair part,
 * a refit onto its authoring body (the part's AuthoringBody when it has the reference topology, else the character's
 * own body when it has, else Mantle's neutral reference body, built once into the project by
 * FMantleReferenceBodies::Build and found by FMantleReferenceBodies::GetClosest) when the part's weighted bones
 * are bound outside the upload's bind tolerance of that body's reference pose, or when the character's own body has
 * no reference topology (the piece was shaped for a body no client can fit from). Apply writes the refitted mesh under
 * the output folder and points the part at it and at the authoring body and its face. When the reference body is needed
 * and the project hasn't built it yet, the line offers to build it, and Apply does so (Epic's MetaHuman cloud) first.
 */
class MANTLEEDITOR_API FMantleAvatarRefitFixer final : public IMantleAvatarFixer
{
public:
	virtual FName GetId() const override { return MantleAvatarFixerIds::Refit; }
	virtual void Analyze(const FMantleConvertContext& Context, TArray<FMantleConvertFix>& OutFixes) const override;
	virtual bool Apply(const FMantleConvertFix& Fix, FMantleConvertContext& Context, FString& OutError) const override;

	/** The bodies a part's refit uses: where it was made, and where it goes. */
	struct FBodies
	{
		/** The character's own body (the assembly's), null outside an assembly. */
		USkeletalMesh* Source = nullptr;

		/**
		 * The authoring body (reference topology) and its face; null when there is none. Mantle's reference body is the
		 * project's own build of it (FMantleReferenceBodies).
		 */
		USkeletalMesh* Target = nullptr;
		USkeletalMesh* TargetFace = nullptr;

		/** The part carries no authoring body and its own character's body has no reference topology. */
		bool bSourceLacksTopology = false;

		/** No Target: why Mantle's reference body is not available (a phrase that follows "Can't: "). */
		FString NoTargetWhy;

		/** No Target only because this project hasn't built Mantle's reference body yet: Apply builds it first. */
		bool bReferenceNotBuilt = false;
	};

	/** The bodies for Part, given the character's own body (null outside an assembly): the class comment's order. */
	static FBodies ResolveBodies(const FMantleConvertPart& Part, USkeletalMesh* CharacterBody, const FMantleAvatarContract& Contract);

	/**
	 * The fix for Context.Parts[PartIndex] with Bodies already resolved (Analyze's body, for the tests): none when the
	 * part is bound to Bodies.Target within tolerance and its own body is fine; a greyed line when there is no target,
	 * the skeleton rule fails, or a vertex would move more than FMantleAvatarRefit::MaxMoveCm.
	 */
	static void AnalyzePart(const FMantleConvertContext& Context, int32 PartIndex, const FBodies& Bodies, const FMantleAvatarContract& Contract,
		TArray<FMantleConvertFix>& OutFixes);
};

/** Registers FMantleAvatarRefitFixer with FMantleAvatarFixers (MantleEditor's StartupModule). */
MANTLEEDITOR_API void RegisterMantleAvatarRefitFixer();
