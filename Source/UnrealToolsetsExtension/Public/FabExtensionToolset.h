#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "FabExtensionToolset.generated.h"

USTRUCT(BlueprintType)
struct FFabExtensionIntegrationInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bPluginInstalled = false;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bPluginEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bModuleLoaded = false;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString VersionName;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	int32 Version = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bRunningCommandlet = false;
};

USTRUCT(BlueprintType)
struct FFabExtensionCacheEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString AssetId;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString CachedFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	int64 FileSizeBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FDateTime ModifiedAt;
};

USTRUCT(BlueprintType)
struct FFabExtensionCacheSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString CacheDirectory;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bDirectoryExists = false;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	int64 TotalSizeBytes = 0;

	/** Number of top-level ZIP entries reported by Fab's public cache API. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	int32 EntryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bHasEntries = false;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FDateTime OldestModifiedAt;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FDateTime NewestModifiedAt;
};

USTRUCT(BlueprintType)
struct FFabExtensionCacheListRequest
{
	GENERATED_BODY()

	/** Optional case-insensitive prefix applied to the cache entry asset id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FabExtensionToolset")
	FString AssetIdPrefix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FabExtensionToolset", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 MaxEntries = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FabExtensionToolset")
	bool bSortNewestFirst = true;

	/**
	 * Optional strict lower bound for cache modification time. Zero disables the
	 * filter. Capture the current Unix time before clicking Download in Fab, then
	 * poll with that value to observe newly completed top-level ZIP entries.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FabExtensionToolset", meta = (ClampMin = "0"))
	int64 ModifiedAfterUnixSeconds = 0;
};

USTRUCT(BlueprintType)
struct FFabExtensionCacheEntryStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString AssetId;

	/** False is a meaningful not-cached result. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bExists = false;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FFabExtensionCacheEntry Entry;
};

USTRUCT(BlueprintType)
struct FFabExtensionImportTypeSupport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString AssetType;

	/** Whether Fab's public workflow factory registry currently has this exact type. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bRegistered = false;
};

UENUM(BlueprintType)
enum class EFabExtensionLibraryCapabilityStatus : uint8
{
	Available,
	RunningCommandlet,
	FabPluginUnavailable,
	UnsupportedFabPluginVersion,
	DataStorageUnavailable,
	PrivateSchemaUnavailable,
	SyncCommandUnavailable
};

USTRUCT(BlueprintType)
struct FFabExtensionLibraryCapability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	EFabExtensionLibraryCapabilityStatus Status =
		EFabExtensionLibraryCapabilityStatus::FabPluginUnavailable;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString FabVersionName;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	int32 FabVersion = 0;

	/** This prototype depends on a reflected Fab schema that is not a public compatibility contract. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bUsesPrivateReflectedSchema = true;

	/** The current Editor process must already have an active Fab login session. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bRequiresActiveFabLoginSession = true;

	/** This extension cannot initiate an entitled Fab download. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bCanInitiateDownloads = false;

	/** A user must start acquisition through Epic's Fab window. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bRequiresManualFabWindowDownloadStart = true;

	/** Completed top-level ZIP cache entries can be inspected and time-filtered. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bCanObserveCompletedZipCacheEntries = true;
};

USTRUCT(BlueprintType)
struct FFabExtensionLibrarySyncReceipt
{
	GENERATED_BODY()

	/** Maximum page size requested from Fab's internal My Library synchronization command. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	int32 RequestedBatchSize = 0;

	/** Rows visible immediately before the asynchronous synchronization command was submitted. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	int32 EntriesBeforeRequest = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FDateTime RequestedAtUtc;
};

USTRUCT(BlueprintType)
struct FFabExtensionLibrarySnapshotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	int32 EntryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bHasEntries = false;

	/**
	 * Always false for this prototype because Fab's private synchronization command
	 * does not expose a public completion signal.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bCompletenessKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FDateTime ObservedAtUtc;
};

USTRUCT(BlueprintType)
struct FFabExtensionOwnedAsset
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString AssetId;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString AssetNamespace;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString ListingType;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString Seller;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString Source;

	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	FString ListingUrl;
};

USTRUCT(BlueprintType)
struct FFabExtensionOwnedAssetSearchRequest
{
	GENERATED_BODY()

	/** Optional case-insensitive text matched against title, asset id, seller, and optionally description. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FabExtensionToolset")
	FString Query;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FabExtensionToolset")
	FString SellerFilter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FabExtensionToolset")
	FString ListingTypeFilter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FabExtensionToolset")
	FString SourceFilter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FabExtensionToolset")
	bool bIncludeDescriptionMatches = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FabExtensionToolset", meta = (ClampMin = "1", ClampMax = "500"))
	int32 MaxResults = 100;
};

USTRUCT(BlueprintType)
struct FFabExtensionOwnedAssetSearchResult
{
	GENERATED_BODY()

	/** Total rows in the currently visible, potentially partial Fab TEDS snapshot. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	int32 SnapshotEntryCount = 0;

	/** False because the private Fab synchronization command has no public completion signal. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	bool bSnapshotCompletenessKnown = false;

	/** Empty when the current snapshot has no matching owned products. */
	UPROPERTY(BlueprintReadOnly, Category = "FabExtensionToolset")
	TArray<FFabExtensionOwnedAsset> Assets;
};

/**
 * Guarded inspection of Fab's public local surfaces plus an exact-version,
 * read-only My Library search prototype.
 *
 * The prototype submits Fab's private TEDS synchronization command but never
 * reads or exposes authentication, tokens, cookies, account ids, signed URLs,
 * or payment data. It does not download, import, install, purchase, or delete.
 */
UCLASS(BlueprintType, MinimalAPI)
class UFabExtensionToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	UFUNCTION(meta = (AICallable), Category = "FabExtensionToolset")
	static FFabExtensionIntegrationInfo GetFabIntegrationInfo();

	UFUNCTION(meta = (AICallable), Category = "FabExtensionToolset")
	static FFabExtensionCacheSummary GetFabCacheSummary();

	/** Returns an empty array when no cache entries match the request. */
	UFUNCTION(meta = (AICallable), Category = "FabExtensionToolset")
	static TArray<FFabExtensionCacheEntry> ListFabCachedEntries(
		const FFabExtensionCacheListRequest& Request);

	UFUNCTION(meta = (AICallable), Category = "FabExtensionToolset")
	static FFabExtensionCacheEntryStatus GetFabCacheEntryStatus(
		const FString& AssetId);

	UFUNCTION(meta = (AICallable), Category = "FabExtensionToolset")
	static FFabExtensionImportTypeSupport GetFabImportTypeSupport(
		const FString& AssetType);

	/** Reports whether the exact Fab version and reflected TEDS schema required by the prototype are present. */
	UFUNCTION(meta = (AICallable), Category = "FabExtensionToolset")
	static FFabExtensionLibraryCapability GetFabOwnedLibrarySearchCapability();

	/**
	 * Replaces Fab's ephemeral TEDS My Library snapshot asynchronously.
	 * Call GetFabOwnedLibrarySnapshotInfo until the row count stabilizes before searching.
	 */
	UFUNCTION(meta = (AICallable), Category = "FabExtensionToolset")
	static FFabExtensionLibrarySyncReceipt RequestFabOwnedLibrarySync(
		int32 BatchSize = 1000);

	UFUNCTION(meta = (AICallable), Category = "FabExtensionToolset")
	static FFabExtensionLibrarySnapshotInfo GetFabOwnedLibrarySnapshotInfo();

	/** Searches only the currently visible owned-library snapshot; it never searches public marketplace listings. */
	UFUNCTION(meta = (AICallable), Category = "FabExtensionToolset")
	static FFabExtensionOwnedAssetSearchResult SearchFabOwnedAssets(
		const FFabExtensionOwnedAssetSearchRequest& Request);
};
