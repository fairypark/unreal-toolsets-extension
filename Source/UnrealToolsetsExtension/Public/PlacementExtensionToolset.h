#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "PlacementExtensionToolset.generated.h"

USTRUCT(BlueprintType)
struct FPlacementExtensionSnapRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlacementExtensionToolset")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Distance above the Actor bounds origin, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlacementExtensionToolset", meta = (ClampMin = "0.0"))
	double TraceUpCentimeters = 100000.0;

	/** Distance below the Actor bounds origin, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlacementExtensionToolset", meta = (ClampMin = "0.0"))
	double TraceDownCentimeters = 100000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlacementExtensionToolset")
	bool bTraceComplex = false;

	/** Select a Landscape hit when one exists, even if a non-Landscape hit is closer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlacementExtensionToolset")
	bool bPreferLandscape = true;

	/** Reject every non-Landscape hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlacementExtensionToolset")
	bool bLandscapeOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlacementExtensionToolset")
	bool bIgnoreAttachedActors = true;

	/** Align the bottom of the complete Actor bounds, rather than its pivot, to the hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlacementExtensionToolset")
	bool bUseActorBoundsBottom = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlacementExtensionToolset")
	TArray<TObjectPtr<AActor>> AdditionalIgnoredActors;
};

USTRUCT(BlueprintType)
struct FPlacementExtensionSnapResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlacementExtensionToolset")
	TSoftObjectPtr<AActor> Actor;

	UPROPERTY(BlueprintReadOnly, Category = "PlacementExtensionToolset")
	TSoftObjectPtr<AActor> HitActor;

	UPROPERTY(BlueprintReadOnly, Category = "PlacementExtensionToolset")
	FString HitComponentPath;

	UPROPERTY(BlueprintReadOnly, Category = "PlacementExtensionToolset")
	FVector PreviousLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "PlacementExtensionToolset")
	FVector NewLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "PlacementExtensionToolset")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "PlacementExtensionToolset")
	FVector HitNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "PlacementExtensionToolset")
	double AppliedDeltaZCentimeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "PlacementExtensionToolset")
	double BoundsBottomBeforeCentimeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "PlacementExtensionToolset")
	FString TraceChannel;
};

/** Safe, diagnostic placement operations that complement the stock scene tools. */
UCLASS(BlueprintType, MinimalAPI)
class UPlacementExtensionToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Snaps an Actor to a traced surface while ignoring the Actor's own
	 * components. Supports negative world heights and reports the selected hit.
	 */
	UFUNCTION(meta = (AICallable), Category = "PlacementExtensionToolset")
	static FPlacementExtensionSnapResult SnapActorToGround(
		AActor* Actor,
		const FPlacementExtensionSnapRequest& Request);
};
