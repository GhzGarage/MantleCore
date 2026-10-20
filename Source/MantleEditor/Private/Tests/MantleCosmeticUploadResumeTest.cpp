// Copyright Joshua Eger. All Rights Reserved.
#include "MantleCosmeticPackage.h"
#include "MantleCosmeticUpload.h"
#include "MantleSha256.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// The cosmetic upload session's resume note: the item it created is remembered, and a second upload of the same files follows
// it. If that item has since been delisted the session makes a new one in the same run, instead of stopping and leaving the
// creator to upload a second time. Pure: a scripted Mantle and a fake clock, no network.

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleCosmeticUploadResumeTestPrivate
{
	FMantleApiResult Ok(const FString& Body)
	{
		FMantleApiResult Result;
		Result.bSuccess = true;
		Result.StatusCode = 200;
		Result.Body = Body;
		return Result;
	}

	FString AssetJson(const FString& Id, const FString& Hash, const FString& Kind, bool bCreated)
	{
		return FString::Printf(TEXT("{\"id\":\"%s\",\"hash\":\"%s\",\"kind\":\"%s\",\"status\":\"approved\",\"cookState\":\"passed\",")
			TEXT("\"moderationState\":\"passed\",\"created\":%s}"), *Id, *Hash, *Kind, bCreated ? TEXT("true") : TEXT("false"));
	}

	/** A scripted Mantle: files by hash (all approved), avatar items by id with a status the test can change. */
	class FFakeMantle : public IMantleUploadTransport
	{
	public:
		TMap<FString, FString> IdByHash;
		TMap<FString, FString> KindByHash;
		TMap<FString, FString> StatusByItem;
		int32 NextId = 1;
		int32 CreateCalls = 0;
		FString LastCreatedItem;

		virtual void Send(const FMantleUploadCallRequest& Request, FMantleApiCallback OnDone) override
		{
			Pending.Emplace(Answer(Request), OnDone);
		}

		void Flush()
		{
			while (Pending.Num() > 0)
			{
				TArray<TPair<FMantleApiResult, FMantleApiCallback>> Batch = MoveTemp(Pending);
				Pending.Reset();
				for (TPair<FMantleApiResult, FMantleApiCallback>& Item : Batch)
				{
					Item.Value.ExecuteIfBound(Item.Key);
				}
			}
		}

	private:
		TArray<TPair<FMantleApiResult, FMantleApiCallback>> Pending;

		FMantleApiResult Answer(const FMantleUploadCallRequest& Request)
		{
			switch (Request.Call)
			{
			case EMantleUploadCall::DiffAssets:
				{
					FString Known;
					FString Unknown;
					for (const FString& Hash : Request.Hashes)
					{
						if (const FString* Id = IdByHash.Find(Hash))
						{
							Known += (Known.IsEmpty() ? TEXT("") : TEXT(",")) + AssetJson(*Id, Hash, KindByHash.FindRef(Hash), false);
						}
						else
						{
							Unknown += FString::Printf(TEXT("%s\"%s\""), Unknown.IsEmpty() ? TEXT("") : TEXT(","), *Hash);
						}
					}
					return Ok(FString::Printf(TEXT("{\"known\":[%s],\"unknown\":[%s]}"), *Known, *Unknown));
				}
			case EMantleUploadCall::UploadAsset:
				{
					const FString Hash = Request.Bytes ? FMantleSha256::AssetHash(*Request.Bytes) : FString();
					const FString Id = IdByHash.Contains(Hash) ? IdByHash[Hash] : FString::Printf(TEXT("ast_u%032x"), NextId++);
					IdByHash.Add(Hash, Id);
					KindByHash.Add(Hash, Request.Options.Kind);
					return Ok(AssetJson(Id, Hash, Request.Options.Kind, true));
				}
			case EMantleUploadCall::GetAsset:
				for (const TPair<FString, FString>& Entry : IdByHash)
				{
					if (Entry.Value == Request.Id)
					{
						return Ok(AssetJson(Entry.Value, Entry.Key, KindByHash.FindRef(Entry.Key), false));
					}
				}
				break;
			case EMantleUploadCall::CreateItem:
				{
					++CreateCalls;
					LastCreatedItem = FString::Printf(TEXT("itm_u%032x"), 0xB000 + CreateCalls);
					StatusByItem.Add(LastCreatedItem, TEXT("approved"));
					return Ok(FString::Printf(TEXT("{\"id\":\"%s\",\"status\":\"approved\",\"moderationState\":\"passed\"}"), *LastCreatedItem));
				}
			case EMantleUploadCall::GetItem:
				if (const FString* Status = StatusByItem.Find(Request.Id))
				{
					return Ok(FString::Printf(TEXT("{\"id\":\"%s\",\"status\":\"%s\",\"moderationState\":\"passed\"}"), *Request.Id, **Status));
				}
				break;
			default:
				return Ok(TEXT("{}"));
			}
			FMantleApiResult Missing;
			Missing.StatusCode = 404;
			Missing.Code = TEXT("not_found");
			return Missing;
		}
	};

	/** A private hat made of a thumbnail, a mesh and one texture. */
	FMantleCosmeticPackage MakeHatPackage()
	{
		FMantleCosmeticPackage Package;
		Package.Class = EMantleCosmeticClass::Accessory;
		Package.Slot = TEXT("hat");
		Package.Name = TEXT("Test hat");
		Package.Description = TEXT("A hat.");
		Package.Visibility = TEXT("private");
		Package.ThumbnailKey = TEXT("thumbnail");
		Package.MeshKey = TEXT("mesh");
		Package.Materials.AddDefaulted_GetRef().AlbedoKey = TEXT("albedo");
		const auto AddAsset = [&Package](const TCHAR* Key, const TCHAR* Kind, uint8 Seed)
		{
			FMantleCosmeticUploadAsset& Asset = Package.Assets.AddDefaulted_GetRef();
			Asset.Key = Key;
			Asset.Label = Key;
			Asset.Options.Kind = Kind;
			Asset.Bytes.Init(Seed, 64);
			Asset.Hash = FMantleSha256::AssetHash(Asset.Bytes);
		};
		AddAsset(TEXT("thumbnail"), TEXT("texture"), 1);
		AddAsset(TEXT("mesh"), TEXT("mesh"), 2);
		AddAsset(TEXT("albedo"), TEXT("texture"), 3);
		return Package;
	}

	/** Runs one upload of the package to its end on a fake clock. */
	TSharedRef<FMantleCosmeticUploadSession> RunUpload(const FMantleCosmeticPackage& Package, const TSharedRef<FFakeMantle>& Mantle, const FString& ResumeFile)
	{
		double Now = 1000.0;
		const TSharedRef<FMantleCosmeticUploadSession> Session = MakeShared<FMantleCosmeticUploadSession>(Package, Mantle, ResumeFile, [&Now] { return Now; });
		Session->Start();
		for (int32 Step = 0; Step < 2000 && !Session->IsFinished(); ++Step)
		{
			Mantle->Flush();
			Session->Tick();
			Now += 0.5;
		}
		return Session;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMantleCosmeticUploadDelistedResumeTest,
	"Mantle.Cosmetic.UploadResume.DelistedItemIsReplacedInTheSameRun",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMantleCosmeticUploadDelistedResumeTest::RunTest(const FString& Parameters)
{
	using namespace MantleCosmeticUploadResumeTestPrivate;
	const FString ResumeFile = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("Mantle") / TEXT("Tests") / (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".json")));
	const TSharedRef<FFakeMantle> Mantle = MakeShared<FFakeMantle>();
	const FMantleCosmeticPackage Package = MakeHatPackage();

	// The first upload makes the item and remembers it.
	const TSharedRef<FMantleCosmeticUploadSession> First = RunUpload(Package, Mantle, ResumeFile);
	if (!TestTrue(*FString::Printf(TEXT("the first upload succeeds (%s)"), *First->GetFailure()), First->Succeeded()))
	{
		return false;
	}
	const FString FirstItem = First->GetItemId();
	TestEqual(TEXT("one item is made"), Mantle->CreateCalls, 1);

	// An upload of the same files while the item is live follows it and makes nothing.
	const TSharedRef<FMantleCosmeticUploadSession> Again = RunUpload(Package, Mantle, ResumeFile);
	TestTrue(TEXT("a repeat while live succeeds"), Again->Succeeded());
	TestEqual(TEXT("and follows the same item"), Again->GetItemId(), FirstItem);
	TestEqual(TEXT("and makes no new item"), Mantle->CreateCalls, 1);

	// The item is delisted; the same files uploaded once more publish a new item, in that one run.
	Mantle->StatusByItem.Add(FirstItem, TEXT("delisted"));
	const TSharedRef<FMantleCosmeticUploadSession> Third = RunUpload(Package, Mantle, ResumeFile);
	TestTrue(*FString::Printf(TEXT("the upload after the delist succeeds (%s)"), *Third->GetFailure()), Third->Succeeded());
	TestEqual(TEXT("exactly one new item is made"), Mantle->CreateCalls, 2);
	TestNotEqual(TEXT("it is not the delisted one"), Third->GetItemId(), FirstItem);
	TestEqual(TEXT("it is the one Mantle just made"), Third->GetItemId(), Mantle->LastCreatedItem);

	// The note now names the new item, so the next repeat follows it.
	FString Note;
	FFileHelper::LoadFileToString(Note, *ResumeFile);
	TestTrue(TEXT("the note names the new item"), Note.Contains(Third->GetItemId()));

	IFileManager::Get().Delete(*ResumeFile, false, false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
