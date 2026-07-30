#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ObservabilityExtensionToolset.generated.h"

UENUM(BlueprintType)
enum class EUnrealToolsetsExtensionOperationClass : uint8
{
	Discovery,
	Read,
	Validation,
	Mutation,
	MutationCapable,
	Save
};

UENUM(BlueprintType)
enum class EUnrealToolsetsExtensionVerificationSemantics : uint8
{
	None,
	ValidationResult,
	InternalPostcondition
};

UENUM(BlueprintType)
enum class EUnrealToolsetsExtensionPersistenceSemantics : uint8
{
	None,
	DirtyNotSaved,
	CallerControlled,
	Saved
};

UENUM(BlueprintType)
enum class EUnrealToolsetsExtensionFailureCategory : uint8
{
	InvalidRequest,
	InvalidTarget,
	Precondition,
	Unsupported,
	Environment,
	Execution,
	Verification,
	Save,
	Rollback,
	Unknown
};

USTRUCT(BlueprintType)
struct FUnrealToolsetsExtensionOperationDescriptor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	FString ToolsetName;

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	FString OperationName;

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	EUnrealToolsetsExtensionOperationClass OperationClass =
		EUnrealToolsetsExtensionOperationClass::Read;

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	EUnrealToolsetsExtensionVerificationSemantics VerificationSemantics =
		EUnrealToolsetsExtensionVerificationSemantics::None;

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	EUnrealToolsetsExtensionPersistenceSemantics PersistenceSemantics =
		EUnrealToolsetsExtensionPersistenceSemantics::None;

	/** True when the operation may validate a plan without changing project state. */
	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	bool bMayDryRun = false;

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	bool bMayAttemptRollback = false;
};

USTRUCT(BlueprintType)
struct FUnrealToolsetsExtensionErrorDescriptor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	FString ErrorCode;

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	EUnrealToolsetsExtensionFailureCategory FailureCategory =
		EUnrealToolsetsExtensionFailureCategory::Unknown;

	/** True only when this exact error code guarantees that rollback was verified. */
	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	bool bVerifiedRollback = false;
};

USTRUCT(BlueprintType)
struct FUnrealToolsetsExtensionObservabilityInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	FString ExtensionName;

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	FString ExtensionVersion;

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	FString EngineVersion;

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	int32 ObservabilityContractVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	TArray<FUnrealToolsetsExtensionOperationDescriptor> Operations;

	UPROPERTY(BlueprintReadOnly, Category = "ObservabilityExtensionToolset")
	TArray<FUnrealToolsetsExtensionErrorDescriptor> Errors;
};

/**
 * Read-only, client-neutral metadata for classifying extension operations
 * without inspecting project-specific MCP arguments or response payloads.
 */
UCLASS(BlueprintType, MinimalAPI)
class UObservabilityExtensionToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Returns the versioned operation and safe-error catalog for this plugin. */
	UFUNCTION(meta = (AICallable), Category = "ObservabilityExtensionToolset")
	static FUnrealToolsetsExtensionObservabilityInfo GetExtensionObservabilityInfo();
};
