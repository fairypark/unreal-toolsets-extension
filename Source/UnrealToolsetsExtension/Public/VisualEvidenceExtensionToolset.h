#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "VisualEvidenceExtensionToolset.generated.h"

class ACameraActor;

USTRUCT(BlueprintType)
struct FVisualEvidenceCaptureRequest
{
	GENERATED_BODY()

	/** Stable review role such as arrival, hero, route, reverse, elevated, or contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualEvidenceExtensionToolset")
	FString CameraRole;

	/**
	 * PNG path relative to the current project's Saved directory.
	 * Absolute paths, parent traversal, and existing PNG or receipt files are rejected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualEvidenceExtensionToolset")
	FString OutputRelativePath;

	/** Draws completed after the camera jump before temporal stability sampling begins. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "VisualEvidenceExtensionToolset",
		meta = (ClampMin = "3", ClampMax = "8"))
	int32 WarmupDrawCount = 3;

	/** Maximum extra draws used to obtain two consecutive stable frame comparisons. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "VisualEvidenceExtensionToolset",
		meta = (ClampMin = "3", ClampMax = "16"))
	int32 MaxStabilizationDrawCount = 8;

	/** Maximum mean absolute RGB difference, in 0-255 channel units, for a stable frame pair. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "VisualEvidenceExtensionToolset",
		meta = (ClampMin = "0.0", ClampMax = "0.5"))
	double MaxMeanPixelDelta = 0.5;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "VisualEvidenceExtensionToolset",
		meta = (ClampMin = "1280", ClampMax = "16384"))
	int32 MinimumWidthPixels = 1280;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "VisualEvidenceExtensionToolset",
		meta = (ClampMin = "720", ClampMax = "16384"))
	int32 MinimumHeightPixels = 720;
};

USTRUCT(BlueprintType)
struct FVisualEvidenceVerificationRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualEvidenceExtensionToolset")
	FString EvidenceRelativePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualEvidenceExtensionToolset")
	FString ExpectedCameraRole;

	/** Capture-returned PNG digest. The verifier never derives this trust anchor from the receipt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualEvidenceExtensionToolset")
	FString ExpectedEvidenceSha256;

	/** Capture-returned receipt digest held outside the receipt file. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisualEvidenceExtensionToolset")
	FString ExpectedReceiptSha256;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "VisualEvidenceExtensionToolset",
		meta = (ClampMin = "1280", ClampMax = "16384"))
	int32 MinimumWidthPixels = 1280;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "VisualEvidenceExtensionToolset",
		meta = (ClampMin = "720", ClampMax = "16384"))
	int32 MinimumHeightPixels = 720;
};

USTRUCT(BlueprintType)
struct FVisualEvidenceReceipt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	int32 ContractVersion = 1;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString CameraRole;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	TSoftObjectPtr<ACameraActor> CameraActor;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString CameraObjectPath;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString WorldPackageName;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString EvidenceRelativePath;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString EvidenceAbsolutePath;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString ReceiptRelativePath;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString ReceiptAbsolutePath;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	int32 WidthPixels = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	int32 HeightPixels = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	int32 AppliedMinimumWidthPixels = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	int32 AppliedMinimumHeightPixels = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	int64 ByteCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString EvidenceSha256;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString ReceiptSha256;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString CameraViewStateSha256;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FVector CameraLocationCentimeters = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FRotator CameraRotationDegrees = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	float CameraFieldOfViewDegrees = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString CameraProjectionMode;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	float CameraAspectRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	bool bCameraConstrainAspectRatio = false;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	float CameraPostProcessBlendWeight = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	bool bCameraViewApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString CameraAutoExposureMethod;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	float CameraAutoExposureBias = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	float CameraAutoExposureMinBrightness = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	float CameraAutoExposureMaxBrightness = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	bool bViewportFixedExposure = false;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	float ViewportFixedEV100 = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString ViewMode;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	int32 WarmupDrawCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	int32 StabilizationDrawCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	double FinalMeanPixelDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	double MaximumMeanPixelDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	int32 StableFramePairCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	int32 RemainingStreamingRequests = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	int32 RemainingAssetCompileCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	bool bShaderCompilationActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	bool bUiClean = false;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString CaptureStartedAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString CaptureCompletedAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "VisualEvidenceExtensionToolset")
	FString EvidenceFileModifiedAtUtc;
};

/** Persistent fixed-camera evidence and fail-closed supervision handoff verification. */
UCLASS(BlueprintType, MinimalAPI)
class UVisualEvidenceExtensionToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Captures a current-world CameraActor, writes a unique PNG plus evidence receipt,
	 * verifies both, and restores the viewport state.
	 */
	UFUNCTION(meta = (AICallable), Category = "VisualEvidenceExtensionToolset")
	static FVisualEvidenceReceipt CaptureCameraToPng(
		ACameraActor* CameraActor,
		const FVisualEvidenceCaptureRequest& Request);

	/**
	 * Re-reads a PNG and its receipt immediately before supervisor handoff.
	 * Raises when camera identity or state, provenance, dimensions, hash, UI-clean
	 * status, or temporal and streaming settlement no longer matches.
	 */
	UFUNCTION(meta = (AICallable), Category = "VisualEvidenceExtensionToolset")
	static FVisualEvidenceReceipt VerifyEvidenceForSupervision(
		ACameraActor* ExpectedCameraActor,
		const FVisualEvidenceVerificationRequest& Request);
};
