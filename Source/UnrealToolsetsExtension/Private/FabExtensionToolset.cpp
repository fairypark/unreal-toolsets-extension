#include "FabExtensionToolset.h"

#include "Algo/Unique.h"
#include "FabWorkflowFactoryRegistry.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Utilities/FabAssetsCache.h"

DEFINE_LOG_CATEGORY_STATIC(LogFabExtensionToolset, Log, All);

namespace FabExtensionToolsetPrivate
{
	void RaiseError(const TCHAR* Code, const FString& Detail)
	{
		UE_LOG(
			LogFabExtensionToolset,
			Error,
			TEXT("[%s] %s"),
			Code,
			*Detail);
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("[%s] %s"), Code, *Detail));
	}

	bool IsPlainAssetId(const FString& AssetId)
	{
		return !AssetId.IsEmpty()
			&& AssetId == FPaths::GetCleanFilename(AssetId)
			&& !AssetId.Contains(TEXT("/"))
			&& !AssetId.Contains(TEXT("\\"))
			&& AssetId != TEXT(".")
			&& AssetId != TEXT("..");
	}

	FString GetEntryPath(const FString& AssetId)
	{
		return FPaths::Combine(
			FFabAssetsCache::GetCacheLocation(),
			AssetId + TEXT(".zip"));
	}

	FFabExtensionCacheEntry BuildEntry(const FString& AssetId)
	{
		FFabExtensionCacheEntry Entry;
		Entry.AssetId = AssetId;
		Entry.CachedFilePath = GetEntryPath(AssetId);

		const FFileStatData Stats =
			IFileManager::Get().GetStatData(*Entry.CachedFilePath);
		if (Stats.bIsValid && !Stats.bIsDirectory)
		{
			Entry.FileSizeBytes = Stats.FileSize;
			Entry.ModifiedAt = Stats.ModificationTime;
		}
		return Entry;
	}

	TArray<FFabExtensionCacheEntry> GetEntries()
	{
		TArray<FString> AssetIds = FFabAssetsCache::GetCachedAssets();
		AssetIds.Sort();
		AssetIds.SetNum(Algo::Unique(AssetIds));

		TArray<FFabExtensionCacheEntry> Entries;
		Entries.Reserve(AssetIds.Num());
		for (const FString& AssetId : AssetIds)
		{
			FFabExtensionCacheEntry Entry = BuildEntry(AssetId);
			if (Entry.FileSizeBytes >= 0
				&& IFileManager::Get().FileExists(*Entry.CachedFilePath))
			{
				Entries.Add(MoveTemp(Entry));
			}
		}
		return Entries;
	}
}

FFabExtensionIntegrationInfo
UFabExtensionToolset::GetFabIntegrationInfo()
{
	FFabExtensionIntegrationInfo Result;
	Result.bRunningCommandlet = IsRunningCommandlet();

	const TSharedPtr<IPlugin> FabPlugin =
		IPluginManager::Get().FindPlugin(TEXT("Fab"));
	Result.bPluginInstalled = FabPlugin.IsValid();
	if (FabPlugin.IsValid())
	{
		Result.bPluginEnabled = FabPlugin->IsEnabled();
		Result.VersionName = FabPlugin->GetDescriptor().VersionName;
		Result.Version = FabPlugin->GetDescriptor().Version;
	}
	Result.bModuleLoaded =
		FModuleManager::Get().IsModuleLoaded(TEXT("Fab"));
	return Result;
}

FFabExtensionCacheSummary UFabExtensionToolset::GetFabCacheSummary()
{
	using namespace FabExtensionToolsetPrivate;
	FFabExtensionCacheSummary Result;
	Result.CacheDirectory = FFabAssetsCache::GetCacheLocation();
	Result.bDirectoryExists =
		IFileManager::Get().DirectoryExists(*Result.CacheDirectory);
	Result.TotalSizeBytes = FFabAssetsCache::GetCacheSize();

	const TArray<FFabExtensionCacheEntry> Entries = GetEntries();
	Result.EntryCount = Entries.Num();
	Result.bHasEntries = !Entries.IsEmpty();
	if (Result.bHasEntries)
	{
		Result.OldestModifiedAt = Entries[0].ModifiedAt;
		Result.NewestModifiedAt = Entries[0].ModifiedAt;
		for (const FFabExtensionCacheEntry& Entry : Entries)
		{
			Result.OldestModifiedAt =
				FMath::Min(Result.OldestModifiedAt, Entry.ModifiedAt);
			Result.NewestModifiedAt =
				FMath::Max(Result.NewestModifiedAt, Entry.ModifiedAt);
		}
	}
	return Result;
}

TArray<FFabExtensionCacheEntry>
UFabExtensionToolset::ListFabCachedEntries(
	const FFabExtensionCacheListRequest& Request)
{
	using namespace FabExtensionToolsetPrivate;
	if (Request.MaxEntries < 1 || Request.MaxEntries > 10000)
	{
		RaiseError(
			TEXT("FAB_EXTENSION_CACHE_LIST_REQUEST_INVALID"),
			TEXT("MaxEntries must be in the range 1..10000."));
		return {};
	}

	TArray<FFabExtensionCacheEntry> Entries = GetEntries().FilterByPredicate(
		[&Request](const FFabExtensionCacheEntry& Entry)
		{
			return Request.AssetIdPrefix.IsEmpty()
				|| Entry.AssetId.StartsWith(
					Request.AssetIdPrefix,
					ESearchCase::IgnoreCase);
		});
	Entries.Sort(
		[&Request](
			const FFabExtensionCacheEntry& Left,
			const FFabExtensionCacheEntry& Right)
		{
			if (Left.ModifiedAt == Right.ModifiedAt)
			{
				return Left.AssetId < Right.AssetId;
			}
			return Request.bSortNewestFirst
				? Left.ModifiedAt > Right.ModifiedAt
				: Left.ModifiedAt < Right.ModifiedAt;
		});
	if (Entries.Num() > Request.MaxEntries)
	{
		Entries.SetNum(Request.MaxEntries);
	}
	return Entries;
}

FFabExtensionCacheEntryStatus
UFabExtensionToolset::GetFabCacheEntryStatus(const FString& AssetId)
{
	using namespace FabExtensionToolsetPrivate;
	FFabExtensionCacheEntryStatus Result;
	Result.AssetId = AssetId;
	if (!IsPlainAssetId(AssetId))
	{
		RaiseError(
			TEXT("FAB_EXTENSION_ASSET_ID_INVALID"),
			TEXT("AssetId must be a non-empty cache base filename without an extension or path separators."));
		return Result;
	}

	const TArray<FString> AssetIds = FFabAssetsCache::GetCachedAssets();
	const FString* Match = AssetIds.FindByPredicate(
		[&AssetId](const FString& CachedAssetId)
		{
			return CachedAssetId.Equals(
				AssetId,
				ESearchCase::CaseSensitive);
		});
	if (Match != nullptr)
	{
		Result.Entry = BuildEntry(*Match);
		Result.bExists =
			IFileManager::Get().FileExists(*Result.Entry.CachedFilePath);
	}
	return Result;
}

FFabExtensionImportTypeSupport
UFabExtensionToolset::GetFabImportTypeSupport(const FString& AssetType)
{
	using namespace FabExtensionToolsetPrivate;
	FFabExtensionImportTypeSupport Result;
	Result.AssetType = AssetType;
	if (AssetType.TrimStartAndEnd().IsEmpty())
	{
		RaiseError(
			TEXT("FAB_EXTENSION_IMPORT_TYPE_INVALID"),
			TEXT("AssetType must not be empty or whitespace."));
		return Result;
	}
	Result.bRegistered =
		FFabWorkflowFactoryRegistry::IsAssetTypeRegistered(AssetType);
	return Result;
}
