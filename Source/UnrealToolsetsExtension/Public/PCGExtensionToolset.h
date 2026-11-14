#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "PCGExtensionToolset.generated.h"

class UPCGComponent;
class UPCGNode;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FPCGExtensionWeightedMeshEntryRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCGExtensionToolset")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCGExtensionToolset", meta = (ClampMin = "0"))
	int32 Weight = 1;

	/** Collision profile applied to generated instances. Defaults to NoCollision for bulk environment placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCGExtensionToolset")
	FName CollisionProfileName = TEXT("NoCollision");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCGExtensionToolset", meta = (ClampMin = "0"))
	int32 CullStartDistanceCentimeters = 0;

	/** Zero disables distance culling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCGExtensionToolset", meta = (ClampMin = "0"))
	int32 CullEndDistanceCentimeters = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCGExtensionToolset")
	bool bCanEverAffectNavigation = false;
};

USTRUCT(BlueprintType)
struct FPCGExtensionWeightedMeshEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	int32 Weight = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	FName CollisionProfileName;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	int32 CullStartDistanceCentimeters = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	int32 CullEndDistanceCentimeters = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	bool bCanEverAffectNavigation = false;
};

USTRUCT(BlueprintType)
struct FPCGExtensionWeightedMeshMutation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	TSoftObjectPtr<UPCGNode> Node;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	FString ModifiedPackagePath;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	TArray<FPCGExtensionWeightedMeshEntry> Entries;
};

USTRUCT(BlueprintType)
struct FPCGExtensionInstanceComponentAudit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	FString ComponentPath;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	int32 InstanceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	FName CollisionProfileName;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	bool bCanEverAffectNavigation = false;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	int32 CullStartDistanceCentimeters = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	int32 CullEndDistanceCentimeters = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	int32 SampledInstanceCount = 0;

	/** Fraction of sampled instances whose XY position resolves to a Landscape height. */
	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	double LandscapeCoverageRatio = 0.0;

	/** Fraction of sampled instances within the requested vertical tolerance of a Landscape. */
	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	double LandscapeGroundedRatio = 0.0;
};

USTRUCT(BlueprintType)
struct FPCGExtensionGraphAuditRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCGExtensionToolset", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 MaxInstancesPerComponentToSample = 256;

	/** Maximum vertical distance from Landscape height counted as grounded, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCGExtensionToolset", meta = (ClampMin = "0.0"))
	double GroundToleranceCentimeters = 100.0;
};

USTRUCT(BlueprintType)
struct FPCGExtensionGraphAudit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	TSoftObjectPtr<UPCGComponent> Component;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	TSoftObjectPtr<AActor> OwnerActor;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	FString GraphPath;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	bool bIsGenerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	bool bWasGeneratedThisSession = false;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	int32 ManagedResourceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	int32 TotalInstanceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	TArray<FPCGExtensionInstanceComponentAudit> InstanceComponents;

	/** Diagnostic explanations, including likely reasons for an empty generated volume. */
	UPROPERTY(BlueprintReadOnly, Category = "PCGExtensionToolset")
	TArray<FString> Findings;
};

/**
 * Structured PCG operations that complement the official generic PCGToolset.
 *
 * These tools avoid ambiguous generic array mutation and inspect generated
 * managed ISM/HISM resources without regenerating or deleting PCG output.
 */
UCLASS(BlueprintType, MinimalAPI)
class UPCGExtensionToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Returns an empty array when the weighted selector contains no entries. */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "PCGExtensionToolset")
	static TArray<FPCGExtensionWeightedMeshEntry> GetWeightedMeshEntries(UPCGNode* StaticMeshSpawnerNode);

	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "PCGExtensionToolset")
	static FPCGExtensionWeightedMeshMutation ReplaceWeightedMeshEntries(
		UPCGNode* StaticMeshSpawnerNode,
		const TArray<FPCGExtensionWeightedMeshEntryRequest>& Entries);

	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "PCGExtensionToolset")
	static FPCGExtensionWeightedMeshMutation AddWeightedMeshEntry(
		UPCGNode* StaticMeshSpawnerNode,
		const FPCGExtensionWeightedMeshEntryRequest& Entry,
		int32 InsertIndex = -1);

	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "PCGExtensionToolset")
	static FPCGExtensionWeightedMeshMutation UpdateWeightedMeshEntry(
		UPCGNode* StaticMeshSpawnerNode,
		int32 Index,
		const FPCGExtensionWeightedMeshEntryRequest& Entry);

	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "PCGExtensionToolset")
	static FPCGExtensionWeightedMeshMutation RemoveWeightedMeshEntry(
		UPCGNode* StaticMeshSpawnerNode,
		int32 Index);

	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "PCGExtensionToolset")
	static FPCGExtensionWeightedMeshMutation ReorderWeightedMeshEntry(
		UPCGNode* StaticMeshSpawnerNode,
		int32 FromIndex,
		int32 ToIndex);

	/** Audits existing managed PCG output without executing or cleaning the graph. */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "PCGExtensionToolset")
	static FPCGExtensionGraphAudit AuditGraphInstance(
		UPCGComponent* Component,
		const FPCGExtensionGraphAuditRequest& Request);
};
