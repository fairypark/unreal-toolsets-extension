#if WITH_DEV_AUTOMATION_TESTS

#include "FabExtensionToolset.h"

#include "Misc/AutomationTest.h"

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

	const FFabExtensionCacheEntryStatus MissingStatus =
		UFabExtensionToolset::GetFabCacheEntryStatus(
			TEXT("UnrealToolsetsExtensionMissingFabEntry7CC60973"));
	TestFalse(TEXT("Missing cache entry is reported"), MissingStatus.bExists);

	const FFabExtensionImportTypeSupport UnknownType =
		UFabExtensionToolset::GetFabImportTypeSupport(
			TEXT("UnrealToolsetsExtension.UnknownFabType"));
	TestFalse(TEXT("Unknown import type is not registered"), UnknownType.bRegistered);
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
	return true;
}

#endif
