#include "ObservabilityExtensionToolset.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ObservabilityExtensionToolsetPrivate
{
	constexpr const TCHAR* PluginName = TEXT("UnrealToolsetsExtension");

	void RaiseError(const TCHAR* Code, const FString& Detail)
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("[%s] %s"), Code, *Detail));
	}

	bool ParseOperationClass(
		const FString& Value,
		EUnrealToolsetsExtensionOperationClass& Result)
	{
		if (Value == TEXT("discovery"))
		{
			Result = EUnrealToolsetsExtensionOperationClass::Discovery;
		}
		else if (Value == TEXT("read"))
		{
			Result = EUnrealToolsetsExtensionOperationClass::Read;
		}
		else if (Value == TEXT("validation"))
		{
			Result = EUnrealToolsetsExtensionOperationClass::Validation;
		}
		else if (Value == TEXT("mutation"))
		{
			Result = EUnrealToolsetsExtensionOperationClass::Mutation;
		}
		else if (Value == TEXT("mutation_capable"))
		{
			Result = EUnrealToolsetsExtensionOperationClass::MutationCapable;
		}
		else if (Value == TEXT("save"))
		{
			Result = EUnrealToolsetsExtensionOperationClass::Save;
		}
		else
		{
			return false;
		}
		return true;
	}

	bool ParseVerificationSemantics(
		const FString& Value,
		EUnrealToolsetsExtensionVerificationSemantics& Result)
	{
		if (Value == TEXT("none"))
		{
			Result = EUnrealToolsetsExtensionVerificationSemantics::None;
		}
		else if (Value == TEXT("validation_result"))
		{
			Result = EUnrealToolsetsExtensionVerificationSemantics::ValidationResult;
		}
		else if (Value == TEXT("internal_postcondition"))
		{
			Result = EUnrealToolsetsExtensionVerificationSemantics::InternalPostcondition;
		}
		else
		{
			return false;
		}
		return true;
	}

	bool ParsePersistenceSemantics(
		const FString& Value,
		EUnrealToolsetsExtensionPersistenceSemantics& Result)
	{
		if (Value == TEXT("none"))
		{
			Result = EUnrealToolsetsExtensionPersistenceSemantics::None;
		}
		else if (Value == TEXT("dirty_not_saved"))
		{
			Result = EUnrealToolsetsExtensionPersistenceSemantics::DirtyNotSaved;
		}
		else if (Value == TEXT("caller_controlled"))
		{
			Result = EUnrealToolsetsExtensionPersistenceSemantics::CallerControlled;
		}
		else if (Value == TEXT("saved"))
		{
			Result = EUnrealToolsetsExtensionPersistenceSemantics::Saved;
		}
		else
		{
			return false;
		}
		return true;
	}

	bool ParseFailureCategory(
		const FString& Value,
		EUnrealToolsetsExtensionFailureCategory& Result)
	{
		if (Value == TEXT("invalid_request"))
		{
			Result = EUnrealToolsetsExtensionFailureCategory::InvalidRequest;
		}
		else if (Value == TEXT("invalid_target"))
		{
			Result = EUnrealToolsetsExtensionFailureCategory::InvalidTarget;
		}
		else if (Value == TEXT("precondition"))
		{
			Result = EUnrealToolsetsExtensionFailureCategory::Precondition;
		}
		else if (Value == TEXT("unsupported"))
		{
			Result = EUnrealToolsetsExtensionFailureCategory::Unsupported;
		}
		else if (Value == TEXT("environment"))
		{
			Result = EUnrealToolsetsExtensionFailureCategory::Environment;
		}
		else if (Value == TEXT("execution"))
		{
			Result = EUnrealToolsetsExtensionFailureCategory::Execution;
		}
		else if (Value == TEXT("verification"))
		{
			Result = EUnrealToolsetsExtensionFailureCategory::Verification;
		}
		else if (Value == TEXT("save"))
		{
			Result = EUnrealToolsetsExtensionFailureCategory::Save;
		}
		else if (Value == TEXT("rollback"))
		{
			Result = EUnrealToolsetsExtensionFailureCategory::Rollback;
		}
		else if (Value == TEXT("unknown"))
		{
			Result = EUnrealToolsetsExtensionFailureCategory::Unknown;
		}
		else
		{
			return false;
		}
		return true;
	}

	bool LoadCatalog(
		const FString& CatalogPath,
		FUnrealToolsetsExtensionObservabilityInfo& Result,
		FString& Error)
	{
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *CatalogPath))
		{
			Error = TEXT("The packaged observability catalog could not be read.");
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			Error = TEXT("The packaged observability catalog is not valid JSON.");
			return false;
		}

		double ContractVersion = 0.0;
		const TArray<TSharedPtr<FJsonValue>>* Operations = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
		if (!Root->TryGetNumberField(TEXT("contract_version"), ContractVersion) ||
			!Root->TryGetArrayField(TEXT("operations"), Operations) ||
			!Root->TryGetArrayField(TEXT("errors"), Errors))
		{
			Error = TEXT("The observability catalog is missing required top-level fields.");
			return false;
		}

		Result.ObservabilityContractVersion = static_cast<int32>(ContractVersion);
		for (const TSharedPtr<FJsonValue>& Value : *Operations)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Entry.IsValid())
			{
				Error = TEXT("An operation catalog entry is not an object.");
				return false;
			}

			FString OperationClass;
			FString VerificationSemantics;
			FString PersistenceSemantics;
			FUnrealToolsetsExtensionOperationDescriptor Descriptor;
			if (!Entry->TryGetStringField(TEXT("toolset"), Descriptor.ToolsetName) ||
				!Entry->TryGetStringField(TEXT("operation"), Descriptor.OperationName) ||
				!Entry->TryGetStringField(TEXT("class"), OperationClass) ||
				!Entry->TryGetStringField(TEXT("verification"), VerificationSemantics) ||
				!Entry->TryGetStringField(TEXT("persistence"), PersistenceSemantics) ||
				!Entry->TryGetBoolField(TEXT("may_dry_run"), Descriptor.bMayDryRun) ||
				!Entry->TryGetBoolField(TEXT("may_attempt_rollback"), Descriptor.bMayAttemptRollback) ||
				!ParseOperationClass(OperationClass, Descriptor.OperationClass) ||
				!ParseVerificationSemantics(VerificationSemantics, Descriptor.VerificationSemantics) ||
				!ParsePersistenceSemantics(PersistenceSemantics, Descriptor.PersistenceSemantics))
			{
				Error = TEXT("An operation catalog entry is missing or has an invalid field.");
				return false;
			}
			Result.Operations.Add(MoveTemp(Descriptor));
		}

		for (const TSharedPtr<FJsonValue>& Value : *Errors)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Entry.IsValid())
			{
				Error = TEXT("An error catalog entry is not an object.");
				return false;
			}

			FString FailureCategory;
			FUnrealToolsetsExtensionErrorDescriptor Descriptor;
			if (!Entry->TryGetStringField(TEXT("code"), Descriptor.ErrorCode) ||
				!Entry->TryGetStringField(TEXT("category"), FailureCategory) ||
				!Entry->TryGetBoolField(TEXT("verified_rollback"), Descriptor.bVerifiedRollback) ||
				!ParseFailureCategory(FailureCategory, Descriptor.FailureCategory))
			{
				Error = TEXT("An error catalog entry is missing or has an invalid field.");
				return false;
			}
			Result.Errors.Add(MoveTemp(Descriptor));
		}
		return true;
	}
}

FUnrealToolsetsExtensionObservabilityInfo
UObservabilityExtensionToolset::GetExtensionObservabilityInfo()
{
	using namespace ObservabilityExtensionToolsetPrivate;

	FUnrealToolsetsExtensionObservabilityInfo Result;
	Result.ExtensionName = PluginName;
	Result.EngineVersion = FEngineVersion::Current().ToString();

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin.IsValid())
	{
		RaiseError(
			TEXT("OBSERVABILITY_EXTENSION_CATALOG_UNAVAILABLE"),
			TEXT("The UnrealToolsetsExtension plugin descriptor is unavailable."));
		return {};
	}

	Result.ExtensionVersion = Plugin->GetDescriptor().VersionName;
	const FString CatalogPath =
		FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("UsefulnessCatalog.json"));
	FString Error;
	if (!LoadCatalog(CatalogPath, Result, Error))
	{
		RaiseError(TEXT("OBSERVABILITY_EXTENSION_CATALOG_INVALID"), Error);
		return {};
	}
	return Result;
}
