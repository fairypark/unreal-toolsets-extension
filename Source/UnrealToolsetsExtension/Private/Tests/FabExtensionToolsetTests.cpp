#if WITH_DEV_AUTOMATION_TESTS

#include "FabExtensionToolset.h"

#include "DataStorage/Features.h"
#include "Elements/Columns/TypedElementWebColumns.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Modules/ModuleManager.h"
#include "Misc/Parse.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFabExtensionReadOnlyTest,
	"AI.UnrealToolsetsExtension.Fab.ReadOnlyInspection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFabExtensionReadOnlyTest::RunTest(const FString& Parameters)
{
	const FFabExtensionIntegrationInfo Integration =
		UFabExtensionToolset::GetFabIntegrationInfo();
	TestTrue(TEXT("Fab plugin is installed"), Integration.bPluginInstalled);
	TestTrue(TEXT("Fab plugin is enabled"), Integration.bPluginEnabled);
	TestTrue(TEXT("Fab module is loaded"), Integration.bModuleLoaded);
	TestFalse(TEXT("Fab version is reported"), Integration.VersionName.IsEmpty());

	const FFabExtensionCacheSummary Summary =
		UFabExtensionToolset::GetFabCacheSummary();
	TestFalse(TEXT("Fab cache location is reported"), Summary.CacheDirectory.IsEmpty());
	TestTrue(TEXT("Fab cache size is non-negative"), Summary.TotalSizeBytes >= 0);
	TestTrue(TEXT("Fab cache entry count is non-negative"), Summary.EntryCount >= 0);

	FFabExtensionCacheListRequest LimitedRequest;
	LimitedRequest.MaxEntries = 1;
	const TArray<FFabExtensionCacheEntry> LimitedEntries =
		UFabExtensionToolset::ListFabCachedEntries(LimitedRequest);
	TestTrue(TEXT("List respects MaxEntries"), LimitedEntries.Num() <= 1);

	FFabExtensionCacheListRequest EmptyRequest;
	EmptyRequest.AssetIdPrefix =
		TEXT("UnrealToolsetsExtension_NoMatchingFabEntry_7CC60973");
	const TArray<FFabExtensionCacheEntry> EmptyEntries =
		UFabExtensionToolset::ListFabCachedEntries(EmptyRequest);
	TestEqual(TEXT("No matching cache entries is meaningful"), EmptyEntries.Num(), 0);

	FFabExtensionCacheListRequest FutureRequest;
	FutureRequest.ModifiedAfterUnixSeconds =
		FDateTime::UtcNow().ToUnixTimestamp() + 60;
	const TArray<FFabExtensionCacheEntry> FutureEntries =
		UFabExtensionToolset::ListFabCachedEntries(FutureRequest);
	TestEqual(
		TEXT("Future modification bound returns no completed entries"),
		FutureEntries.Num(),
		0);

	const FFabExtensionCacheEntryStatus MissingStatus =
		UFabExtensionToolset::GetFabCacheEntryStatus(
			TEXT("UnrealToolsetsExtensionMissingFabEntry7CC60973"));
	TestFalse(TEXT("Missing cache entry is reported"), MissingStatus.bExists);

	const FFabExtensionImportTypeSupport UnknownType =
		UFabExtensionToolset::GetFabImportTypeSupport(
			TEXT("UnrealToolsetsExtension.UnknownFabType"));
	TestFalse(TEXT("Unknown import type is not registered"), UnknownType.bRegistered);

	const FFabExtensionLibraryCapability LibraryCapability =
		UFabExtensionToolset::GetFabOwnedLibrarySearchCapability();
	TestEqual(TEXT("Installed Fab version is reported"), LibraryCapability.FabVersion, Integration.Version);
	TestTrue(
		TEXT("Fab prototype declares its private reflected schema dependency"),
		LibraryCapability.bUsesPrivateReflectedSchema);
	TestTrue(
		TEXT("Fab prototype declares its active-login precondition"),
		LibraryCapability.bRequiresActiveFabLoginSession);
	TestFalse(
		TEXT("Fab prototype does not claim download initiation"),
		LibraryCapability.bCanInitiateDownloads);
	TestTrue(
		TEXT("Fab prototype requires the Fab window to start a download"),
		LibraryCapability.bRequiresManualFabWindowDownloadStart);
	TestTrue(
		TEXT("Fab prototype can observe completed ZIP cache entries"),
		LibraryCapability.bCanObserveCompletedZipCacheEntries);

	if (LibraryCapability.bAvailable)
	{
		const FFabExtensionLibrarySnapshotInfo Snapshot =
			UFabExtensionToolset::GetFabOwnedLibrarySnapshotInfo();
		TestTrue(TEXT("Snapshot count is non-negative"), Snapshot.EntryCount >= 0);
		TestFalse(
			TEXT("Prototype never claims snapshot completeness"),
			Snapshot.bCompletenessKnown);

		FFabExtensionOwnedAssetSearchRequest SearchRequest;
		SearchRequest.Query =
			TEXT("UnrealToolsetsExtension_NoOwnedFabMatch_7CC60973");
		const FFabExtensionOwnedAssetSearchResult Search =
			UFabExtensionToolset::SearchFabOwnedAssets(SearchRequest);
		TestEqual(
			TEXT("Impossible owned-library query has no matches"),
			Search.Assets.Num(),
			0);
		TestEqual(
			TEXT("Search reports the observed snapshot count"),
			Search.SnapshotEntryCount,
			Snapshot.EntryCount);
		TestFalse(
			TEXT("Search never claims snapshot completeness"),
			Search.bSnapshotCompletenessKnown);
	}
	return true;
}

// Passive reflection only: no login, network, sync, download, or install calls.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFabExtensionCompatibilityTest,
	"AI.UnrealToolsetsExtension.Fab.CompatibilityDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFabExtensionCompatibilityTest::RunTest(const FString& Parameters)
{
	const FFabExtensionIntegrationInfo Integration =
		UFabExtensionToolset::GetFabIntegrationInfo();
	const FFabExtensionLibraryCapability Capability =
		UFabExtensionToolset::GetFabOwnedLibrarySearchCapability();
	const FEngineVersion EngineVersion = FEngineVersion::Current();
	const bool bLegacyVersion = EngineVersion.GetMajor() == 5
		&& EngineVersion.GetMinor() == 8 && Integration.Version == 14;
	if (!Integration.bPluginInstalled || !Integration.bPluginEnabled || !Integration.bModuleLoaded)
	{
		TestTrue(TEXT("Unavailable plugin keeps legacy status"),
			Capability.Status == EFabExtensionLibraryCapabilityStatus::FabPluginUnavailable);
		TestFalse(TEXT("Unavailable plugin cannot enable legacy path"), Capability.bAvailable);
	}
	else if (!bLegacyVersion)
	{
		TestTrue(TEXT("Other Engine/Fab versions stay blocked, including UE 5.9/Fab 22"),
			Capability.Status == EFabExtensionLibraryCapabilityStatus::UnsupportedFabPluginVersion);
		TestFalse(TEXT("Official discovery never unlocks private TEDS"), Capability.bAvailable);
	}
	else if (IsRunningCommandlet())
	{
		TestTrue(TEXT("Exact legacy version still refuses commandlets"),
			Capability.Status == EFabExtensionLibraryCapabilityStatus::RunningCommandlet);
		TestFalse(TEXT("Commandlets cannot enable legacy path"), Capability.bAvailable);
	}
	TestEqual(TEXT("Availability agrees with legacy status"), Capability.bAvailable,
		Capability.Status == EFabExtensionLibraryCapabilityStatus::Available);

	const UClass* OfficialClass = FindObject<UClass>(
		nullptr, TEXT("/Script/FabToolset.FabToolset"));
	TestEqual(TEXT("Reports optional module without loading it"),
		Capability.bOfficialToolsetModuleLoaded,
		FModuleManager::Get().IsModuleLoaded(TEXT("FabToolset")));
	TestEqual(TEXT("Reports loaded official reflection"),
		Capability.bOfficialToolsetClassPresent, OfficialClass != nullptr);
	const FName FunctionNames[] = {
		TEXT("SearchFabMyLibrary"), TEXT("SearchFabListings"), TEXT("GetFabListingDetails")
	};
	int32 ExpectedCount = 0;
	for (const FName FunctionName : FunctionNames)
	{
		bool bExpected = false;
#if WITH_METADATA
		const UFunction* Function = OfficialClass != nullptr
			? OfficialClass->FindFunctionByName(FunctionName) : nullptr;
		bExpected = Capability.bOfficialToolsetModuleLoaded
			&& OfficialClass != nullptr
			&& OfficialClass->IsChildOf(UToolsetDefinition::StaticClass())
			&& Function != nullptr && Function->HasAnyFunctionFlags(FUNC_Static)
			&& Function->HasMetaData(TEXT("AICallable"))
			&& !Function->HasMetaData(TEXT("AIIgnore"));
#endif
		const FString QualifiedName = TEXT("FabToolset.FabToolset.") + FunctionName.ToString();
		TestEqual(*FString::Printf(TEXT("Observed function %s"), *QualifiedName),
			Capability.OfficialReflectedToolNames.Contains(QualifiedName), bExpected);
		ExpectedCount += bExpected ? 1 : 0;
		if (FunctionName == FName(TEXT("SearchFabMyLibrary")))
		{
			TestEqual(TEXT("Recommendation requires observed official library function"),
				Capability.RecommendedOwnedLibraryTool,
				bExpected ? QualifiedName : FString());
		}
	}
	TestEqual(TEXT("Only read-only official search/detail functions are advertised"),
		Capability.OfficialReflectedToolNames.Num(), ExpectedCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFabExtensionValidationTest,
	"AI.UnrealToolsetsExtension.Fab.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFabExtensionValidationTest::RunTest(const FString& Parameters)
{
	AddExpectedError(
		TEXT("FAB_EXTENSION_CACHE_LIST_REQUEST_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	FFabExtensionCacheListRequest InvalidListRequest;
	InvalidListRequest.MaxEntries = 0;
	TestEqual(
		TEXT("Invalid list request returns no entries"),
		UFabExtensionToolset::ListFabCachedEntries(InvalidListRequest).Num(),
		0);

	AddExpectedError(
		TEXT("FAB_EXTENSION_CACHE_LIST_REQUEST_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	FFabExtensionCacheListRequest InvalidTimeRequest;
	InvalidTimeRequest.ModifiedAfterUnixSeconds = -1;
	TestEqual(
		TEXT("Negative cache modification bound is rejected"),
		UFabExtensionToolset::ListFabCachedEntries(InvalidTimeRequest).Num(),
		0);

	AddExpectedError(
		TEXT("FAB_EXTENSION_ASSET_ID_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	const FFabExtensionCacheEntryStatus InvalidStatus =
		UFabExtensionToolset::GetFabCacheEntryStatus(TEXT("../not-allowed"));
	TestFalse(TEXT("Invalid asset id is rejected"), InvalidStatus.bExists);

	AddExpectedError(
		TEXT("FAB_EXTENSION_IMPORT_TYPE_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	const FFabExtensionImportTypeSupport InvalidType =
		UFabExtensionToolset::GetFabImportTypeSupport(TEXT("   "));
	TestFalse(TEXT("Blank import type is rejected"), InvalidType.bRegistered);

	AddExpectedError(
		TEXT("FAB_EXTENSION_LIBRARY_SYNC_REQUEST_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	const FFabExtensionLibrarySyncReceipt InvalidSync =
		UFabExtensionToolset::RequestFabOwnedLibrarySync(0);
	TestEqual(
		TEXT("Invalid sync request returns an empty receipt"),
		InvalidSync.RequestedBatchSize,
		0);

	AddExpectedError(
		TEXT("FAB_EXTENSION_LIBRARY_SEARCH_REQUEST_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	FFabExtensionOwnedAssetSearchRequest InvalidSearchRequest;
	InvalidSearchRequest.MaxResults = 0;
	const FFabExtensionOwnedAssetSearchResult InvalidSearch =
		UFabExtensionToolset::SearchFabOwnedAssets(InvalidSearchRequest);
	TestEqual(
		TEXT("Invalid search request returns no assets"),
		InvalidSearch.Assets.Num(),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFabExtensionSyntheticOwnedLibraryTest,
	"AI.UnrealToolsetsExtension.Fab.SyntheticOwnedLibrarySearch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFabExtensionSyntheticOwnedLibraryTest::RunTest(
	const FString& Parameters)
{
	using namespace UE::Editor::DataStorage;

	const FFabExtensionLibraryCapability Capability =
		UFabExtensionToolset::GetFabOwnedLibrarySearchCapability();
	if (!Capability.bAvailable)
	{
		AddInfo(
			TEXT("Synthetic Fab TEDS test skipped because the exact-version prototype is unavailable."));
		return true;
	}

	ICoreProvider* Storage =
		GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
	UScriptStruct* ObjectColumn = FindObject<UScriptStruct>(
		nullptr,
		TEXT("/Script/Fab.FabObjectColumn"));
	UScriptStruct* NameColumn = FindObject<UScriptStruct>(
		nullptr,
		TEXT("/Script/Fab.FabObjectNameColumn"));
	if (Storage == nullptr
		|| ObjectColumn == nullptr
		|| NameColumn == nullptr)
	{
		AddError(TEXT("Required Fab TEDS test surfaces are unavailable."));
		return false;
	}

	const TArray<const UScriptStruct*> Columns = {
		NameColumn,
		ObjectColumn,
		FEditorDataStorageUrlColumn::StaticStruct()
	};
	const TableHandle Table = Storage->RegisterTable(
		Columns,
		FName(*FString::Printf(
			TEXT("UnrealToolsetsExtensionFabTest_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits))));
	if (Table == InvalidTableHandle)
	{
		AddError(TEXT("Unable to register a disposable Fab TEDS table."));
		return false;
	}

	const RowHandle Row = Storage->AddRow(Table);
	if (Row == InvalidRowHandle)
	{
		AddError(TEXT("Unable to add a disposable Fab TEDS row."));
		return false;
	}

	void* NameData = Storage->GetColumnData(Row, NameColumn);
	void* ObjectData = Storage->GetColumnData(Row, ObjectColumn);
	FEditorDataStorageUrlColumn* UrlData =
		Storage->GetColumn<FEditorDataStorageUrlColumn>(Row);
	if (NameData == nullptr || ObjectData == nullptr || UrlData == nullptr)
	{
		Storage->RemoveRow(Row);
		AddError(TEXT("Disposable Fab TEDS row columns are unavailable."));
		return false;
	}

	FindFProperty<FNameProperty>(NameColumn, TEXT("Name"))
		->SetPropertyValue_InContainer(
			NameData,
			FName(TEXT("Headless Prototype Test Asset")));
	FindFProperty<FStrProperty>(ObjectColumn, TEXT("Description"))
		->SetPropertyValue_InContainer(
			ObjectData,
			TEXT("Synthetic owned-library description"));
	FindFProperty<FNameProperty>(ObjectColumn, TEXT("ListingType"))
		->SetPropertyValue_InContainer(
			ObjectData,
			FName(TEXT("3d-model")));
	FindFProperty<FStrProperty>(ObjectColumn, TEXT("Seller"))
		->SetPropertyValue_InContainer(
			ObjectData,
			TEXT("Prototype Seller"));
	FindFProperty<FNameProperty>(ObjectColumn, TEXT("Source"))
		->SetPropertyValue_InContainer(
			ObjectData,
			FName(TEXT("fab")));

	const FGuid ExpectedAssetId(
		0x12345678,
		0x90ABCDEF,
		0x12345678,
		0x90ABCDEF);
	const FGuid ExpectedNamespace(
		0x0BADF00D,
		0x12345678,
		0x90ABCDEF,
		0x10203040);
	*FindFProperty<FStructProperty>(ObjectColumn, TEXT("AssetId"))
		->ContainerPtrToValuePtr<FGuid>(ObjectData) = ExpectedAssetId;
	*FindFProperty<FStructProperty>(ObjectColumn, TEXT("AssetNamespace"))
		->ContainerPtrToValuePtr<FGuid>(ObjectData) = ExpectedNamespace;
	UrlData->UrlString =
		TEXT("https://www.fab.com/listings/headless-prototype-test");

	FFabExtensionOwnedAssetSearchRequest SearchRequest;
	SearchRequest.Query = TEXT("Headless Prototype Test");
	SearchRequest.SellerFilter = TEXT("prototype seller");
	SearchRequest.ListingTypeFilter = TEXT("3D-MODEL");
	SearchRequest.SourceFilter = TEXT("FAB");
	SearchRequest.MaxResults = 10;
	const FFabExtensionOwnedAssetSearchResult Search =
		UFabExtensionToolset::SearchFabOwnedAssets(SearchRequest);

	TestEqual(
		TEXT("Synthetic search returns the exact owned product"),
		Search.Assets.Num(),
		1);
	if (!Search.Assets.IsEmpty())
	{
		TestEqual(
			TEXT("Synthetic title is decoded"),
			Search.Assets[0].Title,
			FString(TEXT("Headless Prototype Test Asset")));
		TestEqual(
			TEXT("Synthetic asset id is decoded"),
			Search.Assets[0].AssetId,
			ExpectedAssetId.ToString(
				EGuidFormats::DigitsWithHyphensLower));
		TestEqual(
			TEXT("Synthetic seller is decoded"),
			Search.Assets[0].Seller,
			FString(TEXT("Prototype Seller")));
		TestEqual(
			TEXT("Synthetic listing URL is decoded"),
			Search.Assets[0].ListingUrl,
			UrlData->UrlString);
	}

	Storage->RemoveRow(Row);
	return true;
}

class FWaitForFabOwnedLibraryRowsCommand final
	: public IAutomationLatentCommand
{
public:
	FWaitForFabOwnedLibraryRowsCommand(
		FAutomationTestBase* InTest,
		const double InStartedAt,
		const double InLastSyncAt)
		: Test(InTest)
		, StartedAt(InStartedAt)
		, LastSyncAt(InLastSyncAt)
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - StartedAt > 60.0)
		{
			Test->AddError(
				TEXT("Timed out waiting for a non-empty Fab owned-library snapshot."));
			return true;
		}

		const FFabExtensionLibrarySnapshotInfo Snapshot =
			UFabExtensionToolset::GetFabOwnedLibrarySnapshotInfo();
		if (Snapshot.EntryCount != LastEntryCount)
		{
			LastEntryCount = Snapshot.EntryCount;
			StableSince = Now;
		}

		if (Snapshot.EntryCount == 0)
		{
			if (Now - LastSyncAt >= 5.0)
			{
				UFabExtensionToolset::RequestFabOwnedLibrarySync(1000);
				LastSyncAt = Now;
			}
			return false;
		}

		if (Now - StableSince < 3.0)
		{
			return false;
		}

		FFabExtensionOwnedAssetSearchRequest SearchRequest;
		SearchRequest.MaxResults = 1;
		const FFabExtensionOwnedAssetSearchResult Search =
			UFabExtensionToolset::SearchFabOwnedAssets(SearchRequest);
		Test->TestEqual(
			TEXT("Live search returns one bounded owned product"),
			Search.Assets.Num(),
			1);
		Test->TestEqual(
			TEXT("Live search reports the stabilized snapshot count"),
			Search.SnapshotEntryCount,
			Snapshot.EntryCount);
		if (!Search.Assets.IsEmpty())
		{
			Test->TestFalse(
				TEXT("Live owned product has an asset id"),
				Search.Assets[0].AssetId.IsEmpty());
			Test->TestFalse(
				TEXT("Live owned product has a title"),
				Search.Assets[0].Title.IsEmpty());
		}
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
	double LastSyncAt = 0.0;
	double StableSince = 0.0;
	int32 LastEntryCount = INDEX_NONE;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFabExtensionOwnedLibraryLiveTest,
	"AI.UnrealToolsetsExtension.Fab.LiveOwnedLibrarySearch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFabExtensionOwnedLibraryLiveTest::RunTest(
	const FString& Parameters)
{
	if (!FParse::Param(
			FCommandLine::Get(),
			TEXT("RunFabOwnedLibraryLiveTest")))
	{
		AddInfo(
			TEXT("Live Fab owned-library test skipped; pass -RunFabOwnedLibraryLiveTest to enable it."));
		return true;
	}

	const FFabExtensionLibraryCapability Capability =
		UFabExtensionToolset::GetFabOwnedLibrarySearchCapability();
	if (!Capability.bAvailable)
	{
		AddError(
			FString::Printf(
				TEXT("Fab owned-library prototype unavailable with capability status %d."),
				static_cast<int32>(Capability.Status)));
		return false;
	}

	const double StartedAt = FPlatformTime::Seconds();
	UFabExtensionToolset::RequestFabOwnedLibrarySync(1000);
	ADD_LATENT_AUTOMATION_COMMAND(
		FWaitForFabOwnedLibraryRowsCommand(
			this,
			StartedAt,
			StartedAt));
	return true;
}

#endif
