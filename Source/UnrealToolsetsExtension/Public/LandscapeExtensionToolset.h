#pragma once

#include "CoreMinimal.h"
#include "Landscape.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "LandscapeExtensionToolset.generated.h"

USTRUCT(BlueprintType)
struct FLandscapeExtensionRegion
{
	GENERATED_BODY()

	/** Use the complete loaded Landscape vertex extent instead of MinVertex and MaxVertex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bUseFullExtent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FIntPoint MinVertex = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FIntPoint MaxVertex = FIntPoint::ZeroValue;

	/** Empty reads the merged Landscape heightmap; otherwise use an edit-layer GUID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString EditLayerGuid;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionHeightSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FIntPoint Vertex = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 RawHeight = 0;

	/** Height in world-space centimeters. */
	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	double WorldZCentimeters = 0.0;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionHeightStatistics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FIntPoint MinVertex = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FIntPoint MaxVertex = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 SampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 MinRawHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 MaxRawHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	double MinWorldZCentimeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	double MaxWorldZCentimeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	double MeanWorldZCentimeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	double StandardDeviationCentimeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	double RangeCentimeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bFullRequestedRegionCovered = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FLandscapeExtensionHeightSample> RepresentativeSamples;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionEditLayerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString Guid;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionInfoResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TSoftObjectPtr<ALandscape> Landscape;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString LandscapeGuid;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString WorldPath;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FIntPoint MinVertex = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FIntPoint MaxVertex = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FIntPoint VertexResolution = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FVector BoundsMinCentimeters = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FVector BoundsMaxCentimeters = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 LoadedComponentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 LoadedStreamingProxyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bWorldPartition = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bHasUnloadedComponents = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString MaterialPath;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString ReimportHeightmapFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FLandscapeExtensionEditLayerInfo> EditLayers;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> DirtyPackagePaths;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionSaveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> SavedPackagePaths;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> UnchangedPackagePaths;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionMutationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString Operation;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TSoftObjectPtr<ALandscape> Landscape;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString SourceFile;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bDryRun = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bBackupCreated = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString BackupDirectory;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> RecoveryPackagePaths;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> ModifiedPackagePaths;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FLandscapeExtensionHeightStatistics BeforeHeight;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FLandscapeExtensionHeightStatistics AfterHeight;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FLandscapeExtensionSaveResult SaveResult;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> Warnings;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> ValidationMessages;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionCreateRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString ActorLabel = TEXT("Landscape");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Landscape vertex spacing and height scale in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FVector Scale = FVector(100.0, 100.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset", meta = (ClampMin = "1", ClampMax = "2"))
	int32 SectionsPerComponent = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	int32 QuadsPerSection = 63;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset", meta = (ClampMin = "1"))
	int32 ComponentCountX = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset", meta = (ClampMin = "1"))
	int32 ComponentCountY = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset", meta = (ClampMin = "0", ClampMax = "65535"))
	int32 FlatRawHeight = 32768;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bSaveAfterCreate = true;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionHeightmapRequest
{
	GENERATED_BODY()

	/** Absolute or project-relative 16-bit PNG or RAW heightmap path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString SourceFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bFlipYAxis = false;

	/** Required edit-layer GUID. Empty is rejected for mutation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString DestinationEditLayerGuid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bSaveAfterSuccess = true;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionWeightmapRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString SourceFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bFlipYAxis = false;

	/** Required edit-layer GUID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString DestinationEditLayerGuid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bSaveAfterSuccess = true;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionLayerReimportRequest
{
	GENERATED_BODY()

	/** Target layer assignment that must already belong to the Landscape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	TObjectPtr<ULandscapeLayerInfoObject> LayerInfo = nullptr;

	/** Absolute or project-relative 8-bit PNG or RAW layer map path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString SourceFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bFlipYAxis = false;

	/** Required destination edit-layer GUID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString DestinationEditLayerGuid;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionReimportRequest
{
	GENERATED_BODY()

	/** Empty uses the Landscape's recorded heightmap source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString HeightmapSourceFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bFlipHeightmapYAxis = false;

	/** Required destination edit-layer GUID for the heightmap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString HeightDestinationEditLayerGuid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	TArray<FLandscapeExtensionLayerReimportRequest> LayerMaps;

	/** Validate and report the complete plan without saving, backing up, or mutating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bDryRun = true;

	/** Export exact pre-mutation height and layer samples into the project's Saved directory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bCreateDataBackup = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bSaveAfterSuccess = true;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionValidatedFile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString Kind;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString LayerName;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString ResolvedFile;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FIntPoint Resolution = FIntPoint::ZeroValue;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionReimportPreflight
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bWorldPartition = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bAllComponentsLoaded = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TSoftObjectPtr<ALandscape> Landscape;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FIntPoint VertexResolution = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FTransform PreservedTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FString PreservedMaterialPath;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 PreservedStreamingProxyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 PreservedGridSizeComponents = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FLandscapeExtensionValidatedFile> ValidatedFiles;

	/** Validation failures are data, not script errors, so dry-run callers can repair the request. */
	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> Errors;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> Warnings;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionProxyRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TSoftObjectPtr<ALandscapeProxy> Proxy;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 ComponentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FIntPoint MinSectionBase = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FIntPoint MaxSectionBase = FIntPoint::ZeroValue;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionProxyAuditRequest
{
	GENERATED_BODY()

	/** Flag a layout whose average loaded components per proxy falls below this value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset", meta = (ClampMin = "1"))
	int32 MinimumComponentsPerProxy = 4;

	/** Flag a layout that exceeds this count without treating proxy count as a quality target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset", meta = (ClampMin = "1"))
	int32 MaximumExpectedProxyCount = 64;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionProxyAudit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bWorldPartition = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bAllComponentsLoaded = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bPotentiallyOverPartitioned = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 LoadedComponentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 LoadedProxyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 CurrentGridSizeComponents = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 RecommendedGridSizeComponents = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	double MeanComponentsPerProxy = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FLandscapeExtensionProxyRecord> Proxies;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> Findings;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionProxyRebuildRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset", meta = (ClampMin = "1", ClampMax = "32"))
	int32 NewGridSizeComponents = 0;

	/** Guards against executing a stale plan after another layout change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	int32 ExpectedCurrentGridSizeComponents = 0;

	/** Must equal REBUILD_LANDSCAPE_STREAMING_PROXIES for a non-dry-run call. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString Confirmation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bDryRun = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	bool bSaveAfterSuccess = true;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionProxyRebuildResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bDryRun = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 PreviousGridSizeComponents = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 NewGridSizeComponents = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 PreviousProxyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	int32 NewProxyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> RecoveryPackagePaths;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	FLandscapeExtensionSaveResult SaveResult;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FString> Warnings;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionBuildingPadRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FVector Center = FVector::ZeroVector;

	/** Half extents of the oriented footprint in world-space centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FVector2D HalfExtentsCentimeters = FVector2D(500.0, 500.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	double YawDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset", meta = (ClampMin = "0.0"))
	double MaxHeightVariationCentimeters = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	double MaxSlopeDegrees = 1.0;

	/** Empty reads the merged Landscape heightmap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeExtensionToolset")
	FString EditLayerGuid;
};

USTRUCT(BlueprintType)
struct FLandscapeExtensionBuildingPadValidation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	TArray<FLandscapeExtensionHeightSample> Samples;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	double HeightVariationCentimeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	double BestFitSlopeDegrees = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bWithinHeightVariation = false;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeExtensionToolset")
	bool bWithinSlope = false;
};

/**
 * Typed Landscape inspection and mutation tools supplied by UnrealToolsetsExtension.
 *
 * The extension is independent of Epic's AllToolsets plugin. It rejects partial
 * World Partition writes and never drives the Landscape Slate UI.
 */
UCLASS(BlueprintType, MinimalAPI)
class ULandscapeExtensionToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionInfoResult GetLandscapeInfo(ALandscapeProxy* Landscape);

	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionMutationResult CreateLandscape(UWorld* World, const FLandscapeExtensionCreateRequest& Request);

	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionMutationResult ImportHeightmap(ALandscapeProxy* Landscape, const FLandscapeExtensionHeightmapRequest& Request);

	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionMutationResult ReimportHeightmap(ALandscapeProxy* Landscape, const FLandscapeExtensionHeightmapRequest& Request);

	/**
	 * Validates an atomic heightmap and layer-map reimport plan. A null Landscape
	 * auto-selects only when the Editor world contains exactly one Landscape.
	 */
	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionReimportPreflight PreflightReimportLandscapeData(
		UWorld* World,
		ALandscapeProxy* Landscape,
		const FLandscapeExtensionReimportRequest& Request);

	/** Reimports the validated batch atomically, with a saved recovery point and optional raw-data backup. */
	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionMutationResult ReimportLandscapeData(
		UWorld* World,
		ALandscapeProxy* Landscape,
		const FLandscapeExtensionReimportRequest& Request);

	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionMutationResult ImportWeightmap(
		ALandscapeProxy* Landscape,
		ULandscapeLayerInfoObject* LayerInfo,
		const FLandscapeExtensionWeightmapRequest& Request);

	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionMutationResult SetLandscapeMaterial(ALandscapeProxy* Landscape, UMaterialInterface* Material);

	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionHeightStatistics GetHeightStatistics(ALandscapeProxy* Landscape, const FLandscapeExtensionRegion& Region);

	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionBuildingPadValidation ValidateBuildingPad(
		ALandscapeProxy* Landscape,
		const FLandscapeExtensionBuildingPadRequest& Request);

	/** Audits the current streaming proxy layout; proxy count is reported as an outcome, never a target. */
	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionProxyAudit AuditStreamingProxies(
		ALandscapeProxy* Landscape,
		const FLandscapeExtensionProxyAuditRequest& Request);

	/**
	 * Reassigns fully loaded Landscape components to a new World Partition grid
	 * only after dry-run review and explicit confirmation.
	 */
	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionProxyRebuildResult RebuildStreamingProxies(
		ALandscapeProxy* Landscape,
		const FLandscapeExtensionProxyRebuildRequest& Request);

	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionSaveResult SaveLandscape(ALandscapeProxy* Landscape);
};
