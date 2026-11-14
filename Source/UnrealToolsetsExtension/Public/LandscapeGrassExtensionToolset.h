#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "LandscapeGrassExtensionToolset.generated.h"

class ULandscapeGrassType;
class UStaticMesh;

UENUM(BlueprintType)
enum class ELandscapeGrassExtensionScaling : uint8
{
	Uniform,
	Free,
	LockXY
};

USTRUCT(BlueprintType)
struct FLandscapeGrassVarietyRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Instances per 10 square meters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
	float GrassDensity = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset")
	bool bUseGrid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PlacementJitter = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset", meta = (ClampMin = "0"))
	int32 StartCullDistanceCentimeters = 10000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset", meta = (ClampMin = "0"))
	int32 EndCullDistanceCentimeters = 10000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset", meta = (ClampMin = "-1", ClampMax = "8"))
	int32 MinLOD = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset")
	ELandscapeGrassExtensionScaling Scaling = ELandscapeGrassExtensionScaling::Uniform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset")
	FVector2D ScaleX = FVector2D(1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset")
	FVector2D ScaleY = FVector2D(1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset")
	FVector2D ScaleZ = FVector2D(1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset")
	bool bRandomRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset")
	bool bAlignToSurface = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset")
	bool bReceivesDecals = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeGrassExtensionToolset")
	bool bCastDynamicShadow = true;
};

USTRUCT(BlueprintType)
struct FLandscapeGrassVariety
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	float GrassDensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	bool bUseGrid = true;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	float PlacementJitter = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	int32 StartCullDistanceCentimeters = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	int32 EndCullDistanceCentimeters = 0;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	int32 MinLOD = -1;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	ELandscapeGrassExtensionScaling Scaling = ELandscapeGrassExtensionScaling::Uniform;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	FVector2D ScaleX = FVector2D(1.0, 1.0);

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	FVector2D ScaleY = FVector2D(1.0, 1.0);

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	FVector2D ScaleZ = FVector2D(1.0, 1.0);

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	bool bRandomRotation = true;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	bool bAlignToSurface = true;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	bool bReceivesDecals = true;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	bool bCastDynamicShadow = true;
};

USTRUCT(BlueprintType)
struct FLandscapeGrassMutation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	TSoftObjectPtr<ULandscapeGrassType> GrassType;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	FString ModifiedPackagePath;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	bool bEnableDensityScaling = true;

	UPROPERTY(BlueprintReadOnly, Category = "LandscapeGrassExtensionToolset")
	TArray<FLandscapeGrassVariety> Varieties;
};

/**
 * Structured LandscapeGrassType editing that avoids generic reflected-array mutation.
 *
 * Mutations are transactional and mark the asset package dirty, but deliberately
 * do not save it. Per-platform and per-quality overrides are reset to the request's
 * default values when a variety is replaced.
 */
UCLASS(BlueprintType, MinimalAPI)
class ULandscapeGrassExtensionToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "LandscapeGrassExtensionToolset")
	static TArray<FLandscapeGrassVariety> GetGrassVarieties(
		ULandscapeGrassType* GrassType);

	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "LandscapeGrassExtensionToolset")
	static FLandscapeGrassMutation ReplaceGrassVarieties(
		ULandscapeGrassType* GrassType,
		const TArray<FLandscapeGrassVarietyRequest>& Varieties);

	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "LandscapeGrassExtensionToolset")
	static FLandscapeGrassMutation AddGrassVariety(
		ULandscapeGrassType* GrassType,
		const FLandscapeGrassVarietyRequest& Variety,
		int32 InsertIndex = -1);

	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "LandscapeGrassExtensionToolset")
	static FLandscapeGrassMutation UpdateGrassVariety(
		ULandscapeGrassType* GrassType,
		int32 Index,
		const FLandscapeGrassVarietyRequest& Variety);

	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "LandscapeGrassExtensionToolset")
	static FLandscapeGrassMutation RemoveGrassVariety(
		ULandscapeGrassType* GrassType,
		int32 Index);

	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "LandscapeGrassExtensionToolset")
	static FLandscapeGrassMutation ReorderGrassVariety(
		ULandscapeGrassType* GrassType,
		int32 FromIndex,
		int32 ToIndex);

	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "LandscapeGrassExtensionToolset")
	static FLandscapeGrassMutation SetGrassDensityScaling(
		ULandscapeGrassType* GrassType,
		bool bEnableDensityScaling);
};
