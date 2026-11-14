#include "FabExtensionToolset.h"

#include "Algo/Unique.h"
#include "DataStorage/Features.h"
#include "Elements/Columns/TypedElementWebColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "FabWorkflowFactoryRegistry.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Utilities/FabAssetsCache.h"

DEFINE_LOG_CATEGORY_STATIC(LogFabExtensionToolset, Log, All);

namespace FabExtensionToolsetPrivate
{
	using namespace UE::Editor::DataStorage;
	using namespace UE::Editor::DataStorage::Queries;

	constexpr int32 SupportedFabPluginVersion = 14;
	const TCHAR* FabSyncCommandName =
		TEXT("Fab.TEDS.MyFolderIntegration");
	const TCHAR* FabObjectColumnPath =
		TEXT("/Script/Fab.FabObjectColumn");
	const TCHAR* FabObjectNameColumnPath =
		TEXT("/Script/Fab.FabObjectNameColumn");

	struct FFabLibrarySchema
	{
		const UScriptStruct* ObjectColumn = nullptr;
		const UScriptStruct* NameColumn = nullptr;

		bool IsValid() const
		{
			if (ObjectColumn == nullptr || NameColumn == nullptr)
			{
				return false;
			}

			return FindFProperty<FNameProperty>(
					NameColumn,
					TEXT("Name")) != nullptr
				&& FindFProperty<FStrProperty>(
					ObjectColumn,
					TEXT("Description")) != nullptr
				&& FindFProperty<FStructProperty>(
					ObjectColumn,
					TEXT("AssetId")) != nullptr
				&& FindFProperty<FStructProperty>(
					ObjectColumn,
					TEXT("AssetNamespace")) != nullptr
				&& FindFProperty<FNameProperty>(
					ObjectColumn,
					TEXT("ListingType")) != nullptr
				&& FindFProperty<FStrProperty>(
					ObjectColumn,
					TEXT("Seller")) != nullptr
				&& FindFProperty<FNameProperty>(
					ObjectColumn,
					TEXT("Source")) != nullptr;
		}
	};

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

	FFabLibrarySchema GetFabLibrarySchema()
	{
		FFabLibrarySchema Schema;
		Schema.ObjectColumn = FindObject<UScriptStruct>(
			nullptr,
			FabObjectColumnPath);
		Schema.NameColumn = FindObject<UScriptStruct>(
			nullptr,
			FabObjectNameColumnPath);
		return Schema;
	}

	IConsoleCommand* GetFabSyncCommand()
	{
		if (IConsoleObject* Object =
				IConsoleManager::Get().FindConsoleObject(
					FabSyncCommandName,
					false))
		{
			return Object->IsEnabled() ? Object->AsCommand() : nullptr;
		}
		return nullptr;
	}

	// Optional official module: inspect only already-loaded reflection. Never load the
	// module, create its default object, dispatch a function, or initialize a session.
	void InspectOfficialFabTools(FFabExtensionLibraryCapability& Result)
	{
		Result.bOfficialToolsetModuleLoaded =
			FModuleManager::Get().IsModuleLoaded(TEXT("FabToolset"));
		const UClass* OfficialClass = FindObject<UClass>(
			nullptr, TEXT("/Script/FabToolset.FabToolset"));
		Result.bOfficialToolsetClassPresent = OfficialClass != nullptr;
		if (!Result.bOfficialToolsetModuleLoaded || OfficialClass == nullptr
			|| !OfficialClass->IsChildOf(UToolsetDefinition::StaticClass()))
		{
			return;
		}

#if WITH_METADATA
		const FName FunctionNames[] = {
			TEXT("SearchFabMyLibrary"),
			TEXT("SearchFabListings"),
			TEXT("GetFabListingDetails")
		};
		for (const FName FunctionName : FunctionNames)
		{
			const UFunction* Function = OfficialClass->FindFunctionByName(FunctionName);
			if (Function == nullptr || !Function->HasAnyFunctionFlags(FUNC_Static)
				|| !Function->HasMetaData(TEXT("AICallable"))
				|| Function->HasMetaData(TEXT("AIIgnore")))
			{
				continue;
			}
			// ToolsetRegistry qualifies C++ toolsets as Module.Class.Function.
			const FString QualifiedName =
				TEXT("FabToolset.") + OfficialClass->GetName() + TEXT(".") + Function->GetName();
			Result.OfficialReflectedToolNames.Add(QualifiedName);
			if (FunctionName == FName(TEXT("SearchFabMyLibrary")))
			{
				Result.RecommendedOwnedLibraryTool = QualifiedName;
			}
		}
#endif
	}

	FFabExtensionLibraryCapability BuildFabLibraryCapability()
	{
		FFabExtensionLibraryCapability Result;
		InspectOfficialFabTools(Result);

		const TSharedPtr<IPlugin> FabPlugin =
			IPluginManager::Get().FindPlugin(TEXT("Fab"));
		if (!FabPlugin.IsValid()
			|| !FabPlugin->IsEnabled()
			|| !FModuleManager::Get().IsModuleLoaded(TEXT("Fab")))
		{
			Result.Status =
				EFabExtensionLibraryCapabilityStatus::FabPluginUnavailable;
			return Result;
		}

		Result.FabVersionName = FabPlugin->GetDescriptor().VersionName;
		Result.FabVersion = FabPlugin->GetDescriptor().Version;
		const FEngineVersion EngineVersion = FEngineVersion::Current();
		if (EngineVersion.GetMajor() != 5
			|| EngineVersion.GetMinor() != 8
			|| Result.FabVersion != SupportedFabPluginVersion)
		{
			Result.Status =
				EFabExtensionLibraryCapabilityStatus::UnsupportedFabPluginVersion;
			return Result;
		}

		if (IsRunningCommandlet())
		{
			Result.Status =
				EFabExtensionLibraryCapabilityStatus::RunningCommandlet;
			return Result;
		}

		if (GetMutableDataStorageFeature<ICoreProvider>(
				StorageFeatureName) == nullptr)
		{
			Result.Status =
				EFabExtensionLibraryCapabilityStatus::DataStorageUnavailable;
			return Result;
		}

		if (!GetFabLibrarySchema().IsValid())
		{
			Result.Status =
				EFabExtensionLibraryCapabilityStatus::PrivateSchemaUnavailable;
			return Result;
		}

		if (GetFabSyncCommand() == nullptr)
		{
			Result.Status =
				EFabExtensionLibraryCapabilityStatus::SyncCommandUnavailable;
			return Result;
		}

		Result.Status = EFabExtensionLibraryCapabilityStatus::Available;
		Result.bAvailable = true;
		return Result;
	}

	bool GetFabLibraryRows(
		ICoreProvider& Storage,
		const FFabLibrarySchema& Schema,
		TArray<RowHandle>& OutRows)
	{
		FQueryDescription Query = Select()
			.Where()
				.All(Schema.ObjectColumn)
			.Compile();
		const QueryHandle Handle =
			Storage.RegisterQuery(MoveTemp(Query));
		if (Handle == InvalidQueryHandle)
		{
			return false;
		}

		Storage.RunQuery(
			Handle,
			CreateDirectQueryCallbackBinding(
				[&OutRows](
					const IDirectQueryContext& Context,
					const RowHandle*)
				{
					OutRows.Append(Context.GetRowHandles());
				}));
		Storage.UnregisterQuery(Handle);
		return true;
	}

	FString ReadStringProperty(
		const UScriptStruct& Struct,
		const void* Data,
		const FName PropertyName)
	{
		if (const FStrProperty* Property =
				FindFProperty<FStrProperty>(&Struct, PropertyName))
		{
			return Property->GetPropertyValue_InContainer(Data);
		}
		return {};
	}

	FString ReadNameProperty(
		const UScriptStruct& Struct,
		const void* Data,
		const FName PropertyName)
	{
		if (const FNameProperty* Property =
				FindFProperty<FNameProperty>(&Struct, PropertyName))
		{
			return Property->GetPropertyValue_InContainer(Data).ToString();
		}
		return {};
	}

	FString ReadGuidProperty(
		const UScriptStruct& Struct,
		const void* Data,
		const FName PropertyName)
	{
		const FStructProperty* Property =
			FindFProperty<FStructProperty>(&Struct, PropertyName);
		if (Property == nullptr
			|| Property->Struct != TBaseStructure<FGuid>::Get())
		{
			return {};
		}

		const FGuid* Value =
			Property->ContainerPtrToValuePtr<FGuid>(Data);
		return Value != nullptr && Value->IsValid()
			? Value->ToString(EGuidFormats::DigitsWithHyphensLower)
			: FString();
	}

	FFabExtensionOwnedAsset BuildOwnedAsset(
		ICoreProvider& Storage,
		const FFabLibrarySchema& Schema,
		const RowHandle Row)
	{
		FFabExtensionOwnedAsset Result;
		const void* ObjectData =
			Storage.GetColumnData(Row, Schema.ObjectColumn);
		if (ObjectData == nullptr)
		{
			return Result;
		}

		if (const void* NameData =
				Storage.GetColumnData(Row, Schema.NameColumn))
		{
			Result.Title = ReadNameProperty(
				*Schema.NameColumn,
				NameData,
				TEXT("Name"));
		}

		Result.AssetId = ReadGuidProperty(
			*Schema.ObjectColumn,
			ObjectData,
			TEXT("AssetId"));
		Result.AssetNamespace = ReadGuidProperty(
			*Schema.ObjectColumn,
			ObjectData,
			TEXT("AssetNamespace"));
		Result.Description = ReadStringProperty(
			*Schema.ObjectColumn,
			ObjectData,
			TEXT("Description"));
		Result.ListingType = ReadNameProperty(
			*Schema.ObjectColumn,
			ObjectData,
			TEXT("ListingType"));
		Result.Seller = ReadStringProperty(
			*Schema.ObjectColumn,
			ObjectData,
			TEXT("Seller"));
		Result.Source = ReadNameProperty(
			*Schema.ObjectColumn,
			ObjectData,
			TEXT("Source"));

		if (const FEditorDataStorageUrlColumn* UrlColumn =
				Storage.GetColumn<FEditorDataStorageUrlColumn>(Row))
		{
			Result.ListingUrl = UrlColumn->UrlString;
		}
		return Result;
	}

	bool MatchesOptionalExactFilter(
		const FString& Value,
		const FString& Filter)
	{
		return Filter.IsEmpty()
			|| Value.Equals(Filter, ESearchCase::IgnoreCase);
	}

	bool MatchesOwnedAsset(
		const FFabExtensionOwnedAsset& Asset,
		const FFabExtensionOwnedAssetSearchRequest& Request)
	{
		if (!MatchesOptionalExactFilter(
				Asset.Seller,
				Request.SellerFilter)
			|| !MatchesOptionalExactFilter(
				Asset.ListingType,
				Request.ListingTypeFilter)
			|| !MatchesOptionalExactFilter(
				Asset.Source,
				Request.SourceFilter))
		{
			return false;
		}

		const FString Query = Request.Query.TrimStartAndEnd();
		return Query.IsEmpty()
			|| Asset.Title.Contains(Query, ESearchCase::IgnoreCase)
			|| Asset.AssetId.Contains(Query, ESearchCase::IgnoreCase)
			|| Asset.Seller.Contains(Query, ESearchCase::IgnoreCase)
			|| (Request.bIncludeDescriptionMatches
				&& Asset.Description.Contains(
					Query,
					ESearchCase::IgnoreCase));
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
	if (Request.MaxEntries < 1
		|| Request.MaxEntries > 10000
		|| Request.ModifiedAfterUnixSeconds < 0)
	{
		RaiseError(
			TEXT("FAB_EXTENSION_CACHE_LIST_REQUEST_INVALID"),
			TEXT("MaxEntries must be in the range 1..10000 and ModifiedAfterUnixSeconds must be non-negative."));
		return {};
	}

	TArray<FFabExtensionCacheEntry> Entries = GetEntries().FilterByPredicate(
		[&Request](const FFabExtensionCacheEntry& Entry)
		{
			const bool bMatchesPrefix = Request.AssetIdPrefix.IsEmpty()
				|| Entry.AssetId.StartsWith(
					Request.AssetIdPrefix,
					ESearchCase::IgnoreCase);
			const bool bMatchesModifiedTime =
				Request.ModifiedAfterUnixSeconds == 0
				|| Entry.ModifiedAt.ToUnixTimestamp()
					> Request.ModifiedAfterUnixSeconds;
			return bMatchesPrefix && bMatchesModifiedTime;
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

FFabExtensionLibraryCapability
UFabExtensionToolset::GetFabOwnedLibrarySearchCapability()
{
	return FabExtensionToolsetPrivate::BuildFabLibraryCapability();
}

FFabExtensionLibrarySyncReceipt
UFabExtensionToolset::RequestFabOwnedLibrarySync(const int32 BatchSize)
{
	using namespace FabExtensionToolsetPrivate;
	FFabExtensionLibrarySyncReceipt Result;
	if (BatchSize < 1 || BatchSize > 1000)
	{
		RaiseError(
			TEXT("FAB_EXTENSION_LIBRARY_SYNC_REQUEST_INVALID"),
			TEXT("BatchSize must be in the range 1..1000."));
		return Result;
	}

	const FFabExtensionLibraryCapability Capability =
		BuildFabLibraryCapability();
	if (!Capability.bAvailable)
	{
		RaiseError(
			TEXT("FAB_EXTENSION_LIBRARY_UNAVAILABLE"),
			FString::Printf(
				TEXT("Owned-library search prototype is unavailable with capability status %d. Legacy snapshot operations require UE 5.8/Fab 14. On UE 5.9, inspect GetFabOwnedLibrarySearchCapability and prefer the reported official FabToolset.FabToolset.SearchFabMyLibrary if exposed by the host; reflection presence does not prove login or network success."),
				static_cast<int32>(Capability.Status)));
		return Result;
	}

	ICoreProvider* Storage =
		GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
	const FFabLibrarySchema Schema = GetFabLibrarySchema();
	TArray<RowHandle> ExistingRows;
	if (Storage == nullptr
		|| !GetFabLibraryRows(*Storage, Schema, ExistingRows))
	{
		RaiseError(
			TEXT("FAB_EXTENSION_LIBRARY_QUERY_FAILED"),
			TEXT("Unable to query the current Fab owned-library snapshot."));
		return Result;
	}

	IConsoleCommand* SyncCommand = GetFabSyncCommand();
	if (SyncCommand == nullptr)
	{
		RaiseError(
			TEXT("FAB_EXTENSION_LIBRARY_UNAVAILABLE"),
			TEXT("Fab owned-library synchronization command is unavailable."));
		return Result;
	}

	Result.RequestedBatchSize = BatchSize;
	Result.EntriesBeforeRequest = ExistingRows.Num();
	Result.RequestedAtUtc = FDateTime::UtcNow();
	SyncCommand->Execute(
		{FString::FromInt(BatchSize)},
		nullptr,
		*GLog);
	return Result;
}

FFabExtensionLibrarySnapshotInfo
UFabExtensionToolset::GetFabOwnedLibrarySnapshotInfo()
{
	using namespace FabExtensionToolsetPrivate;
	FFabExtensionLibrarySnapshotInfo Result;
	const FFabExtensionLibraryCapability Capability =
		BuildFabLibraryCapability();
	if (!Capability.bAvailable)
	{
		RaiseError(
			TEXT("FAB_EXTENSION_LIBRARY_UNAVAILABLE"),
			FString::Printf(
				TEXT("Owned-library search prototype is unavailable with capability status %d. Legacy snapshot operations require UE 5.8/Fab 14. On UE 5.9, inspect GetFabOwnedLibrarySearchCapability and prefer the reported official FabToolset.FabToolset.SearchFabMyLibrary if exposed by the host; reflection presence does not prove login or network success."),
				static_cast<int32>(Capability.Status)));
		return Result;
	}

	ICoreProvider* Storage =
		GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
	const FFabLibrarySchema Schema = GetFabLibrarySchema();
	TArray<RowHandle> Rows;
	if (Storage == nullptr
		|| !GetFabLibraryRows(*Storage, Schema, Rows))
	{
		RaiseError(
			TEXT("FAB_EXTENSION_LIBRARY_QUERY_FAILED"),
			TEXT("Unable to query the current Fab owned-library snapshot."));
		return Result;
	}

	Result.EntryCount = Rows.Num();
	Result.bHasEntries = !Rows.IsEmpty();
	Result.ObservedAtUtc = FDateTime::UtcNow();
	return Result;
}

FFabExtensionOwnedAssetSearchResult
UFabExtensionToolset::SearchFabOwnedAssets(
	const FFabExtensionOwnedAssetSearchRequest& Request)
{
	using namespace FabExtensionToolsetPrivate;
	FFabExtensionOwnedAssetSearchResult Result;
	if (Request.MaxResults < 1 || Request.MaxResults > 500)
	{
		RaiseError(
			TEXT("FAB_EXTENSION_LIBRARY_SEARCH_REQUEST_INVALID"),
			TEXT("MaxResults must be in the range 1..500."));
		return Result;
	}

	const FFabExtensionLibraryCapability Capability =
		BuildFabLibraryCapability();
	if (!Capability.bAvailable)
	{
		RaiseError(
			TEXT("FAB_EXTENSION_LIBRARY_UNAVAILABLE"),
			FString::Printf(
				TEXT("Owned-library search prototype is unavailable with capability status %d. Legacy snapshot operations require UE 5.8/Fab 14. On UE 5.9, inspect GetFabOwnedLibrarySearchCapability and prefer the reported official FabToolset.FabToolset.SearchFabMyLibrary if exposed by the host; reflection presence does not prove login or network success."),
				static_cast<int32>(Capability.Status)));
		return Result;
	}

	ICoreProvider* Storage =
		GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
	const FFabLibrarySchema Schema = GetFabLibrarySchema();
	TArray<RowHandle> Rows;
	if (Storage == nullptr
		|| !GetFabLibraryRows(*Storage, Schema, Rows))
	{
		RaiseError(
			TEXT("FAB_EXTENSION_LIBRARY_QUERY_FAILED"),
			TEXT("Unable to query the current Fab owned-library snapshot."));
		return Result;
	}

	Result.SnapshotEntryCount = Rows.Num();
	Result.Assets.Reserve(
		FMath::Min(Rows.Num(), Request.MaxResults));
	for (const RowHandle Row : Rows)
	{
		FFabExtensionOwnedAsset Asset =
			BuildOwnedAsset(*Storage, Schema, Row);
		if (MatchesOwnedAsset(Asset, Request))
		{
			Result.Assets.Add(MoveTemp(Asset));
		}
	}

	Result.Assets.Sort(
		[](const FFabExtensionOwnedAsset& Left,
			const FFabExtensionOwnedAsset& Right)
		{
			const int32 TitleComparison =
				Left.Title.Compare(
					Right.Title,
					ESearchCase::IgnoreCase);
			return TitleComparison == 0
				? Left.AssetId < Right.AssetId
				: TitleComparison < 0;
		});
	if (Result.Assets.Num() > Request.MaxResults)
	{
		Result.Assets.SetNum(Request.MaxResults);
	}
	return Result;
}
