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
 * Typed Landscape inspection and mutation tools supplied by CommunityToolsetsExtension.
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

	UFUNCTION(meta = (AICallable), Category = "LandscapeExtensionToolset")
	static FLandscapeExtensionSaveResult SaveLandscape(ALandscapeProxy* Landscape);
};
