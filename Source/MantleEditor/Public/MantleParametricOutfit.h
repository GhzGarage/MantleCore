// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/SoftObjectPath.h"

class UObject;
class FMantleAvatarContract;
struct FMantleCosmeticDraft;

struct MANTLEEDITOR_API FMantleOutfitSourceSize
{
	FString Name;
	TArray<FSoftObjectPath> BodyParts;
	TArray<FString> Pieces;
	/** Native logical Outfit identities contributing pieces to this body size. */
	TArray<FGuid> OutfitGuids;
};

/** Editor authoring input only. Evaluates authored sources, retaining the existing Mantle cook and runtime fit. */
struct MANTLEEDITOR_API FMantleParametricOutfit
{
	static bool IsSource(const UObject* Source);
	static bool ListSizes(UObject* Source, TArray<FMantleOutfitSourceSize>& OutSizes, FString& OutError);

	/**
	 * Converts Draft's native source (auto-selected when a character is supplied) into transient skinned render geometry.
	 * With OutfitAuthoringCharacter, the native Collection pipeline evaluates its configured resizing graph
	 * onto that character; AuthoringBody/Face must be that character's final converted output, unless bOutfitAuthoringInMemory
	 * says the character is a standard body type made in memory (FMantleFitBodies), which needs neither.
	 * Without a character, the authored body/head must have the complete reference layout.
	 * Does not combine source sizes or preserve simulation.
	 * Without a character, KeepSlots (the Outfit's material slots) carries over to the exported mesh, and kept sections
	 * over the slot's triangle limit are reduced to 95% of it; OutNote then says so for the creator.
	 * KeepAlive must outlive OutDraft (including preview). Source assets are never modified.
	 */
	static bool Prepare(const FMantleCosmeticDraft& Draft, const FMantleAvatarContract& Contract,
		FMantleCosmeticDraft& OutDraft, TArray<TStrongObjectPtr<UObject>>& KeepAlive, FString& OutError, FString* OutNote = nullptr);

	/**
	 * Converts Draft's Outfit into a rigid accessory (a hat from a MetaHuman Outfit): one authored size's kept sections,
	 * reduced to the accessory limit, as a transient static mesh whose pivot is the head bone (where the acc_hat socket
	 * sits), fitted from the head it was authored on to the reference head (FMantleHeadFit), so each avatar's fitted
	 * hat socket puts it on that avatar's head. OutNote says what was reduced and measured. KeepAlive must outlive OutDraft.
	 */
	static bool PrepareRigid(const FMantleCosmeticDraft& Draft, const FMantleAvatarContract& Contract,
		FMantleCosmeticDraft& OutDraft, TArray<TStrongObjectPtr<UObject>>& KeepAlive, FString& OutError, FString* OutNote = nullptr);
};
