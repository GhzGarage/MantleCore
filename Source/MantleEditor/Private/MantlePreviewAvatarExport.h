// Copyright Joshua Eger. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * The avatar half of an offline preview bundle (#563 stage 2). The creator's chosen preview avatar is a catalog avatar
 * item id (UMantleSettings::PreviewAvatarItemId, set in Project Settings or with the console command
 * Mantle.Preview.Avatar <item id>). The export copies what the client needs to dress the preview player without the
 * network: the item as POST /avatar/items/batch serves it (without its URLs) and its cooked files, in the folder layout
 * the client's MantleBuiltinBundle reads, plus a body-only avatar descriptor naming it. The editor may use the network
 * here (the catalog lookup and the downloads); the preview itself never does.
 */
namespace MantlePreviewAvatarExport
{
	/**
	 * Writes BundleDir/Avatar/descriptor.json and BundleDir/Avatar/Builtin/<item id>/ (manifest.json, item.json,
	 * assets/<hex>) for the chosen preview avatar. On success OutDescriptorRelPath is "Avatar/descriptor.json" and
	 * OutBuiltinRelPath "Avatar/Builtin". Returns false with OutError empty when no avatar is chosen, and with OutError
	 * set when the chosen one cannot be written (not an item id, not signed in, not in the catalog, not an avatar, an
	 * asset that does not download). A failure leaves no Avatar folder behind. Blocks until the downloads are done.
	 * bUseOwnAvatarWhenUnset (a test with weapons): with no avatar chosen, the signed-in creator's own saved avatar is
	 * used, and OutError says what to do when there is none to use.
	 */
	bool Write(const FString& BundleDir, FString& OutDescriptorRelPath, FString& OutBuiltinRelPath, FString& OutError,
		bool bUseOwnAvatarWhenUnset = false);
}
