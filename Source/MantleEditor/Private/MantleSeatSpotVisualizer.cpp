// Copyright Joshua Eger. All Rights Reserved.
#include "MantleSeatSpotVisualizer.h"
#include "MantleGizmoStyle.h"
#include "MantleSeatExport.h"
#include "MantleSeatSpot.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "UnrealEdGlobals.h"

// The seat's marker, drawn in the overlay gizmo's look (MantleGizmoStyle): a box where the hips sit, an arrow for the
// way the body faces, a green up line, and a yellow line down to the floor with the hip height beside it.

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleSeatSpotVisualizerPrivate
{
	/** The seated hips' footprint in centimetres: X is forward, Y is sideways, Z is up. */
	const FVector HipHalfExtent(20.0, 20.0, 10.0);
	constexpr double FacingLength = 60.0;
	constexpr double UpLength = 30.0;
}

void FMantleSeatSpotVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	using namespace MantleSeatSpotVisualizerPrivate;
	const UMantleSeatSpotComponent* Seat = Cast<UMantleSeatSpotComponent>(Component);
	if (!Seat)
	{
		return;
	}
	// Only the heading counts, so the marker is drawn flat: the box and the arrow follow the yaw alone.
	const FVector Hip = Seat->GetComponentLocation();
	const FQuat Yaw = FRotator(0.0, Seat->GetComponentRotation().Yaw, 0.0).Quaternion();
	const FVector Forward = Yaw.GetForwardVector(), Right = Yaw.GetRightVector(), Up = FVector::UpVector;

	DrawOrientedWireBox(PDI, Hip, Forward, Right, Up, HipHalfExtent, MantleGizmoStyle::Reach, SDPG_Foreground);
	DrawDirectionalArrow(PDI, FRotationMatrix::MakeFromX(Forward) * FTranslationMatrix(Hip), MantleGizmoStyle::Facing, FacingLength, 1.f, SDPG_Foreground);
	PDI->DrawLine(Hip, Hip + Up * UpLength, MantleGizmoStyle::Up, SDPG_Foreground, 0.f);

	double HipHeight = 0.0;
	if (FMantleSeatExport::FloorBelow(Seat, HipHeight))
	{
		// Yellow while the height is one a place can hold; red-orange would clash with the arrow, so a bad one is dashed instead.
		const FVector Floor = Hip - Up * HipHeight;
		const bool bSensible = FMantleSeatExport::HeightProblem(true, HipHeight).IsEmpty();
		PDI->DrawLine(Hip, Floor, MantleGizmoStyle::Outline, SDPG_Foreground, bSensible ? 0.f : 3.f);
		DrawWireDiamond(PDI, FTranslationMatrix(Floor), 6.f, MantleGizmoStyle::Outline, SDPG_Foreground);
	}
}

void FMantleSeatSpotVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	const UMantleSeatSpotComponent* Seat = Cast<UMantleSeatSpotComponent>(Component);
	if (!Seat || !View || !Canvas)
	{
		return;
	}
	double HipHeight = 0.0;
	const bool bFound = FMantleSeatExport::FloorBelow(Seat, HipHeight);
	const FString Text = bFound ? FString::Printf(TEXT("Hip %.0f cm"), HipHeight) : FString(TEXT("Nothing to sit on"));
	FVector2D Pixel;
	if (View->WorldToPixel(Seat->GetComponentLocation() + FVector(0.0, 0.0, 40.0), Pixel))
	{
		Canvas->DrawItem(FCanvasTextItem(Pixel, FText::FromString(Text), GEngine->GetSmallFont(), MantleGizmoStyle::Outline));
	}
}
