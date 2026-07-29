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

/**
 * Guarded, read-only inspection of Fab's public local integration surfaces.
 *
 * These tools do not inspect authentication, query marketplace services,
 * infer ownership, download content, install plugins, or clear the cache.
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
};
