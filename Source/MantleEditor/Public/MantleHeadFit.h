// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MantleCosmeticPackage.h"

class USkeletalMesh;

/**
 * Hats fit every head (acc_hat): an avatar's cranium is measured at upload and its hat socket is moved and
 * scaled so the reference head's cranium lands on it. Hats are stored fitted to the reference head
 * (Resources/AvatarConvert/head-reference.json), so a hat on any avatar is the reference fit mapped onto
 * that avatar's head.
 *
 * The cranium: the bind-pose vertices of the body and face whose skin weights are mostly on the head bone or
 * its children, at or above the head joint, as a box in the acc_hat socket's frame (cm). Percentiles, not
 * extremes, so a few stray vertices do not move it.
 */
struct MANTLEEDITOR_API FMantleHeadFit
{
	/** Per axis: a reference-head point u maps onto this head as Scale * u + Offset (socket frame, cm). */
	struct FFit
	{
		FVector3f Scale = FVector3f::OneVector;
		FVector3f Offset = FVector3f::ZeroVector;
	};

	/**
	 * The cranium of Body (and Face, when given) in the frame SocketInHead puts the hat socket in, relative to
	 * the head bone. False with a sentence when neither mesh has a head bone or enough head vertices.
	 */
	static bool MeasureCranium(const USkeletalMesh* Body, const USkeletalMesh* Face, const FTransform& SocketInHead,
		FBox3f& OutBox, int32& OutVertices, FString& OutError);

	/** Mesh's head bone in its bind pose (component space, from the raw reference skeleton); false when it has none. */
	static bool HeadPose(const USkeletalMesh* Mesh, FTransform& OutPose);

	/** The reference head's cranium (head-reference.json); false when the file is missing or unreadable. */
	static bool ReferenceCranium(FBox3f& OutBox);

	/** The per-axis map from the reference cranium onto Measured, each scale clamped to the socket bounds (0.5 to 2). */
	static FFit FitTo(const FBox3f& Reference, const FBox3f& Measured);

	/**
	 * The acc_hat socket override that puts the reference cranium onto Measured: the socket's own rotation, its
	 * location moved and its scale set. SocketInHead is the socket relative to the head bone (identity on Mantle's
	 * skeletons).
	 */
	static FMantleCosmeticSocketOverride HatSocket(const FBox3f& Reference, const FBox3f& Measured, const FTransform& SocketInHead);

	/** The acc_hat socket relative to the head bone on Body: its own socket when it hangs off the head, else identity. */
	static FTransform HatSocketInHead(const USkeletalMesh* Body);

	/** "18.2 x 21.0 x 14.9 cm" (width, depth, height above the joint) for notes and logs. */
	static FString Describe(const FBox3f& Box);
};
