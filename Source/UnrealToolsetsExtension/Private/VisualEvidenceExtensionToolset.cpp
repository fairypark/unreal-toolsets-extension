#include "VisualEvidenceExtensionToolset.h"

#include "AssetCompilingManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "ContentStreaming.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LevelEditorViewport.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

THIRD_PARTY_INCLUDES_START
#include <openssl/sha.h>
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogVisualEvidenceExtensionToolset, Log, All);

namespace VisualEvidenceExtensionToolsetPrivate
{
	constexpr int32 ReceiptContractVersion = 1;
	constexpr int32 EvidencePolicyMinimumWidthPixels = 1280;
	constexpr int32 EvidencePolicyMinimumHeightPixels = 720;
	constexpr double EvidencePolicyMaximumMeanPixelDelta = 0.5;

	void RaiseError(const TCHAR* Code, const FString& Detail)
	{
		UE_LOG(
			LogVisualEvidenceExtensionToolset,
			Error,
			TEXT("[%s] %s"),
			Code,
			*Detail);
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("[%s] %s"), Code, *Detail));
	}

	bool IsValidCameraRole(const FString& Role)
	{
		if (Role.IsEmpty() || Role.Len() > 64)
		{
			return false;
		}
		for (const TCHAR Character : Role)
		{
			if (!FChar::IsAlnum(Character) &&
				Character != TEXT('_') &&
				Character != TEXT('-'))
			{
				return false;
			}
		}
		return true;
	}

	bool ResolveEvidencePaths(
		const FString& RequestedRelativePath,
		FString& OutEvidenceRelativePath,
		FString& OutEvidenceAbsolutePath,
		FString& OutReceiptRelativePath,
		FString& OutReceiptAbsolutePath,
		FString& OutError)
	{
		if (RequestedRelativePath.IsEmpty() || !FPaths::IsRelative(RequestedRelativePath))
		{
			OutError = TEXT("Evidence path must be a non-empty relative path.");
			return false;
		}

		FString NormalizedRelativePath = RequestedRelativePath;
		FPaths::NormalizeFilename(NormalizedRelativePath);
		if (NormalizedRelativePath.IsEmpty() ||
			NormalizedRelativePath == TEXT(".") ||
			NormalizedRelativePath.StartsWith(TEXT("../")) ||
			NormalizedRelativePath.Contains(TEXT("/../")) ||
			!FPaths::GetExtension(NormalizedRelativePath, true).Equals(
				TEXT(".png"),
				ESearchCase::IgnoreCase))
		{
			OutError = TEXT("Evidence path must identify a .png beneath Project Saved.");
			return false;
		}

		FString SavedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		FPaths::NormalizeDirectoryName(SavedRoot);
		FString EvidenceAbsolutePath = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(SavedRoot, NormalizedRelativePath));
		FPaths::NormalizeFilename(EvidenceAbsolutePath);
		if (!FPaths::IsUnderDirectory(EvidenceAbsolutePath, SavedRoot))
		{
			OutError = TEXT("Evidence path resolves outside Project Saved.");
			return false;
		}

		const FString ReceiptRelativePath =
			FPaths::ChangeExtension(NormalizedRelativePath, TEXT("evidence.json"));
		FString ReceiptAbsolutePath = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(SavedRoot, ReceiptRelativePath));
		FPaths::NormalizeFilename(ReceiptAbsolutePath);
		if (!FPaths::IsUnderDirectory(ReceiptAbsolutePath, SavedRoot))
		{
			OutError = TEXT("Receipt path resolves outside Project Saved.");
			return false;
		}

		OutEvidenceRelativePath = MoveTemp(NormalizedRelativePath);
		OutEvidenceAbsolutePath = MoveTemp(EvidenceAbsolutePath);
		OutReceiptRelativePath = ReceiptRelativePath;
		OutReceiptAbsolutePath = MoveTemp(ReceiptAbsolutePath);
		return true;
	}

	bool HasPngSignature(const TArray64<uint8>& Bytes)
	{
		static constexpr uint8 PngSignature[] = {
			0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
		};
		if (Bytes.Num() < 24)
		{
			return false;
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(PngSignature); ++Index)
		{
			if (Bytes[Index] != PngSignature[Index])
			{
				return false;
			}
		}
		return Bytes[12] == static_cast<uint8>('I') &&
			Bytes[13] == static_cast<uint8>('H') &&
			Bytes[14] == static_cast<uint8>('D') &&
			Bytes[15] == static_cast<uint8>('R');
	}

	int32 ReadPngBigEndianInt32(const TArray64<uint8>& Bytes, int32 Offset)
	{
		return
			(static_cast<int32>(Bytes[Offset]) << 24) |
			(static_cast<int32>(Bytes[Offset + 1]) << 16) |
			(static_cast<int32>(Bytes[Offset + 2]) << 8) |
			static_cast<int32>(Bytes[Offset + 3]);
	}

	bool ReadPngDimensions(
		const TArray64<uint8>& Bytes,
		int32& OutWidth,
		int32& OutHeight)
	{
		if (!HasPngSignature(Bytes))
		{
			return false;
		}
		OutWidth = ReadPngBigEndianInt32(Bytes, 16);
		OutHeight = ReadPngBigEndianInt32(Bytes, 20);
		return OutWidth > 0 && OutHeight > 0;
	}

	bool ComputeSha256(const TArray64<uint8>& Bytes, FString& OutHash)
	{
		if (Bytes.IsEmpty())
		{
			return false;
		}
		uint8 Digest[SHA256_DIGEST_LENGTH] = {};
		if (::SHA256(
			Bytes.GetData(),
			static_cast<size_t>(Bytes.Num()),
			Digest) == nullptr)
		{
			return false;
		}
		OutHash = BytesToHex(Digest, UE_ARRAY_COUNT(Digest));
		return OutHash.Len() == 64;
	}

	bool IsValidSha256(const FString& Hash)
	{
		if (Hash.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Hash)
		{
			if (!FChar::IsHexDigit(Character))
			{
				return false;
			}
		}
		return true;
	}

	bool ComputeSha256String(const FString& Value, FString& OutHash)
	{
		FTCHARToUTF8 Utf8(*Value);
		TArray64<uint8> Bytes;
		Bytes.Append(
			reinterpret_cast<const uint8*>(Utf8.Get()),
			Utf8.Length());
		return ComputeSha256(Bytes, OutHash);
	}

	bool ComputeCameraViewStateSha256(
		UCameraComponent* CameraComponent,
		FString& OutHash)
	{
		if (!IsValid(CameraComponent))
		{
			return false;
		}
		FMinimalViewInfo ViewInfo;
		CameraComponent->GetCameraView(0.0f, ViewInfo);
		FString ExportedView;
		FMinimalViewInfo::StaticStruct()->ExportText(
			ExportedView,
			&ViewInfo,
			nullptr,
			nullptr,
			PPF_None,
			nullptr);
		return !ExportedView.IsEmpty() &&
			ComputeSha256String(ExportedView, OutHash);
	}

	double MeanPixelDelta(const TArray<FColor>& A, const TArray<FColor>& B)
	{
		if (A.Num() == 0 || A.Num() != B.Num())
		{
			return TNumericLimits<double>::Max();
		}
		double TotalDelta = 0.0;
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			TotalDelta += FMath::Abs(static_cast<int32>(A[Index].R) - B[Index].R);
			TotalDelta += FMath::Abs(static_cast<int32>(A[Index].G) - B[Index].G);
			TotalDelta += FMath::Abs(static_cast<int32>(A[Index].B) - B[Index].B);
		}
		return TotalDelta / (static_cast<double>(A.Num()) * 3.0);
	}

	FString ProjectionModeString(const UCameraComponent* CameraComponent)
	{
		return UEnum::GetValueAsString(CameraComponent->ProjectionMode);
	}

	FString ExposureMethodString(const UCameraComponent* CameraComponent)
	{
		return UEnum::GetValueAsString(
			CameraComponent->PostProcessSettings.AutoExposureMethod);
	}

	FString ViewModeString(const FLevelEditorViewportClient* ViewportClient)
	{
		return FString::FromInt(static_cast<int32>(ViewportClient->GetViewMode()));
	}

	void PopulateCameraState(
		ACameraActor* CameraActor,
		FLevelEditorViewportClient* ViewportClient,
		FVisualEvidenceReceipt& Receipt)
	{
		UCameraComponent* CameraComponent = CameraActor->GetCameraComponent();
		Receipt.CameraActor = CameraActor;
		Receipt.CameraObjectPath = CameraActor->GetPathName();
		Receipt.WorldPackageName = CameraActor->GetWorld()->GetPackage()->GetName();
		Receipt.CameraLocationCentimeters = CameraActor->GetActorLocation();
		Receipt.CameraRotationDegrees = CameraActor->GetActorRotation();
		Receipt.CameraFieldOfViewDegrees = CameraComponent->FieldOfView;
		Receipt.CameraProjectionMode = ProjectionModeString(CameraComponent);
		Receipt.CameraAspectRatio = CameraComponent->AspectRatio;
		Receipt.bCameraConstrainAspectRatio = CameraComponent->bConstrainAspectRatio;
		Receipt.CameraPostProcessBlendWeight =
			CameraComponent->PostProcessBlendWeight;
		ComputeCameraViewStateSha256(
			CameraComponent,
			Receipt.CameraViewStateSha256);
		Receipt.CameraAutoExposureMethod = ExposureMethodString(CameraComponent);
		Receipt.CameraAutoExposureBias =
			CameraComponent->PostProcessSettings.AutoExposureBias;
		Receipt.CameraAutoExposureMinBrightness =
			CameraComponent->PostProcessSettings.AutoExposureMinBrightness;
		Receipt.CameraAutoExposureMaxBrightness =
			CameraComponent->PostProcessSettings.AutoExposureMaxBrightness;
		Receipt.bViewportFixedExposure = ViewportClient->ExposureSettings.bFixed;
		Receipt.ViewportFixedEV100 = ViewportClient->ExposureSettings.FixedEV100;
		Receipt.ViewMode = ViewModeString(ViewportClient);
	}

	TSharedRef<FJsonObject> VectorToJson(const FVector& Value)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("x"), Value.X);
		Object->SetNumberField(TEXT("y"), Value.Y);
		Object->SetNumberField(TEXT("z"), Value.Z);
		return Object;
	}

	TSharedRef<FJsonObject> RotatorToJson(const FRotator& Value)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("pitch"), Value.Pitch);
		Object->SetNumberField(TEXT("yaw"), Value.Yaw);
		Object->SetNumberField(TEXT("roll"), Value.Roll);
		return Object;
	}

	bool JsonToVector(
		const TSharedPtr<FJsonObject>& Root,
		const TCHAR* Field,
		FVector& OutValue)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
		return Root->TryGetObjectField(Field, Object) &&
			Object != nullptr &&
			(*Object)->TryGetNumberField(TEXT("x"), X) &&
			(*Object)->TryGetNumberField(TEXT("y"), Y) &&
			(*Object)->TryGetNumberField(TEXT("z"), Z) &&
			(OutValue = FVector(X, Y, Z), true);
	}

	bool JsonToRotator(
		const TSharedPtr<FJsonObject>& Root,
		const TCHAR* Field,
		FRotator& OutValue)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		double Pitch = 0.0;
		double Yaw = 0.0;
		double Roll = 0.0;
		return Root->TryGetObjectField(Field, Object) &&
			Object != nullptr &&
			(*Object)->TryGetNumberField(TEXT("pitch"), Pitch) &&
			(*Object)->TryGetNumberField(TEXT("yaw"), Yaw) &&
			(*Object)->TryGetNumberField(TEXT("roll"), Roll) &&
			(OutValue = FRotator(Pitch, Yaw, Roll), true);
	}

	bool SerializeReceipt(
		const FVisualEvidenceReceipt& Receipt,
		FString& OutJson)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("contract_version"), Receipt.ContractVersion);
		Root->SetStringField(TEXT("camera_role"), Receipt.CameraRole);
		Root->SetStringField(TEXT("camera_object_path"), Receipt.CameraObjectPath);
		Root->SetStringField(TEXT("world_package_name"), Receipt.WorldPackageName);
		Root->SetStringField(TEXT("evidence_relative_path"), Receipt.EvidenceRelativePath);
		Root->SetStringField(TEXT("receipt_relative_path"), Receipt.ReceiptRelativePath);
		Root->SetNumberField(TEXT("width_pixels"), Receipt.WidthPixels);
		Root->SetNumberField(TEXT("height_pixels"), Receipt.HeightPixels);
		Root->SetNumberField(
			TEXT("applied_minimum_width_pixels"),
			Receipt.AppliedMinimumWidthPixels);
		Root->SetNumberField(
			TEXT("applied_minimum_height_pixels"),
			Receipt.AppliedMinimumHeightPixels);
		Root->SetNumberField(TEXT("byte_count"), static_cast<double>(Receipt.ByteCount));
		Root->SetStringField(TEXT("evidence_sha256"), Receipt.EvidenceSha256);
		Root->SetStringField(
			TEXT("camera_view_state_sha256"),
			Receipt.CameraViewStateSha256);
		Root->SetObjectField(
			TEXT("camera_location_centimeters"),
			VectorToJson(Receipt.CameraLocationCentimeters));
		Root->SetObjectField(
			TEXT("camera_rotation_degrees"),
			RotatorToJson(Receipt.CameraRotationDegrees));
		Root->SetNumberField(
			TEXT("camera_field_of_view_degrees"),
			Receipt.CameraFieldOfViewDegrees);
		Root->SetStringField(TEXT("camera_projection_mode"), Receipt.CameraProjectionMode);
		Root->SetNumberField(TEXT("camera_aspect_ratio"), Receipt.CameraAspectRatio);
		Root->SetBoolField(
			TEXT("camera_constrain_aspect_ratio"),
			Receipt.bCameraConstrainAspectRatio);
		Root->SetNumberField(
			TEXT("camera_post_process_blend_weight"),
			Receipt.CameraPostProcessBlendWeight);
		Root->SetBoolField(
			TEXT("camera_view_applied"),
			Receipt.bCameraViewApplied);
		Root->SetStringField(
			TEXT("camera_auto_exposure_method"),
			Receipt.CameraAutoExposureMethod);
		Root->SetNumberField(
			TEXT("camera_auto_exposure_bias"),
			Receipt.CameraAutoExposureBias);
		Root->SetNumberField(
			TEXT("camera_auto_exposure_min_brightness"),
			Receipt.CameraAutoExposureMinBrightness);
		Root->SetNumberField(
			TEXT("camera_auto_exposure_max_brightness"),
			Receipt.CameraAutoExposureMaxBrightness);
		Root->SetBoolField(
			TEXT("viewport_fixed_exposure"),
			Receipt.bViewportFixedExposure);
		Root->SetNumberField(TEXT("viewport_fixed_ev100"), Receipt.ViewportFixedEV100);
		Root->SetStringField(TEXT("view_mode"), Receipt.ViewMode);
		Root->SetNumberField(TEXT("warmup_draw_count"), Receipt.WarmupDrawCount);
		Root->SetNumberField(
			TEXT("stabilization_draw_count"),
			Receipt.StabilizationDrawCount);
		Root->SetNumberField(
			TEXT("final_mean_pixel_delta"),
			Receipt.FinalMeanPixelDelta);
		Root->SetNumberField(
			TEXT("maximum_mean_pixel_delta"),
			Receipt.MaximumMeanPixelDelta);
		Root->SetNumberField(
			TEXT("stable_frame_pair_count"),
			Receipt.StableFramePairCount);
		Root->SetNumberField(
			TEXT("remaining_streaming_requests"),
			Receipt.RemainingStreamingRequests);
		Root->SetNumberField(
			TEXT("remaining_asset_compile_count"),
			Receipt.RemainingAssetCompileCount);
		Root->SetBoolField(
			TEXT("shader_compilation_active"),
			Receipt.bShaderCompilationActive);
		Root->SetBoolField(TEXT("ui_clean"), Receipt.bUiClean);
		Root->SetStringField(
			TEXT("capture_started_at_utc"),
			Receipt.CaptureStartedAtUtc);
		Root->SetStringField(
			TEXT("capture_completed_at_utc"),
			Receipt.CaptureCompletedAtUtc);
		Root->SetStringField(
			TEXT("evidence_file_modified_at_utc"),
			Receipt.EvidenceFileModifiedAtUtc);

		const TSharedRef<TJsonWriter<>> Writer =
			TJsonWriterFactory<>::Create(&OutJson);
		return FJsonSerializer::Serialize(Root, Writer);
	}

	bool DeserializeReceipt(
		const FString& JsonText,
		FVisualEvidenceReceipt& OutReceipt,
		FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader =
			TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("Evidence receipt is not valid JSON.");
			return false;
		}

		double ContractVersion = 0.0;
		double Width = 0.0;
		double Height = 0.0;
		double AppliedMinimumWidth = 0.0;
		double AppliedMinimumHeight = 0.0;
		double ByteCount = 0.0;
		double FOV = 0.0;
		double AspectRatio = 0.0;
		double PostProcessWeight = 0.0;
		double ExposureBias = 0.0;
		double ExposureMin = 0.0;
		double ExposureMax = 0.0;
		double FixedEV100 = 0.0;
		double Warmup = 0.0;
		double Stabilization = 0.0;
		double MeanDelta = 0.0;
		double MaximumMeanDelta = 0.0;
		double StablePairs = 0.0;
		double Streaming = 0.0;
		double Compiling = 0.0;
		if (!Root->TryGetNumberField(TEXT("contract_version"), ContractVersion) ||
			!Root->TryGetStringField(TEXT("camera_role"), OutReceipt.CameraRole) ||
			!Root->TryGetStringField(
				TEXT("camera_object_path"),
				OutReceipt.CameraObjectPath) ||
			!Root->TryGetStringField(
				TEXT("world_package_name"),
				OutReceipt.WorldPackageName) ||
			!Root->TryGetStringField(
				TEXT("evidence_relative_path"),
				OutReceipt.EvidenceRelativePath) ||
			!Root->TryGetStringField(
				TEXT("receipt_relative_path"),
				OutReceipt.ReceiptRelativePath) ||
			!Root->TryGetNumberField(TEXT("width_pixels"), Width) ||
			!Root->TryGetNumberField(TEXT("height_pixels"), Height) ||
			!Root->TryGetNumberField(
				TEXT("applied_minimum_width_pixels"),
				AppliedMinimumWidth) ||
			!Root->TryGetNumberField(
				TEXT("applied_minimum_height_pixels"),
				AppliedMinimumHeight) ||
			!Root->TryGetNumberField(TEXT("byte_count"), ByteCount) ||
			!Root->TryGetStringField(
				TEXT("evidence_sha256"),
				OutReceipt.EvidenceSha256) ||
			!Root->TryGetStringField(
				TEXT("camera_view_state_sha256"),
				OutReceipt.CameraViewStateSha256) ||
			!JsonToVector(
				Root,
				TEXT("camera_location_centimeters"),
				OutReceipt.CameraLocationCentimeters) ||
			!JsonToRotator(
				Root,
				TEXT("camera_rotation_degrees"),
				OutReceipt.CameraRotationDegrees) ||
			!Root->TryGetNumberField(TEXT("camera_field_of_view_degrees"), FOV) ||
			!Root->TryGetStringField(
				TEXT("camera_projection_mode"),
				OutReceipt.CameraProjectionMode) ||
			!Root->TryGetNumberField(TEXT("camera_aspect_ratio"), AspectRatio) ||
			!Root->TryGetBoolField(
				TEXT("camera_constrain_aspect_ratio"),
				OutReceipt.bCameraConstrainAspectRatio) ||
			!Root->TryGetNumberField(
				TEXT("camera_post_process_blend_weight"),
				PostProcessWeight) ||
			!Root->TryGetBoolField(
				TEXT("camera_view_applied"),
				OutReceipt.bCameraViewApplied) ||
			!Root->TryGetStringField(
				TEXT("camera_auto_exposure_method"),
				OutReceipt.CameraAutoExposureMethod) ||
			!Root->TryGetNumberField(
				TEXT("camera_auto_exposure_bias"),
				ExposureBias) ||
			!Root->TryGetNumberField(
				TEXT("camera_auto_exposure_min_brightness"),
				ExposureMin) ||
			!Root->TryGetNumberField(
				TEXT("camera_auto_exposure_max_brightness"),
				ExposureMax) ||
			!Root->TryGetBoolField(
				TEXT("viewport_fixed_exposure"),
				OutReceipt.bViewportFixedExposure) ||
			!Root->TryGetNumberField(TEXT("viewport_fixed_ev100"), FixedEV100) ||
			!Root->TryGetStringField(TEXT("view_mode"), OutReceipt.ViewMode) ||
			!Root->TryGetNumberField(TEXT("warmup_draw_count"), Warmup) ||
			!Root->TryGetNumberField(
				TEXT("stabilization_draw_count"),
				Stabilization) ||
			!Root->TryGetNumberField(
				TEXT("final_mean_pixel_delta"),
				MeanDelta) ||
			!Root->TryGetNumberField(
				TEXT("maximum_mean_pixel_delta"),
				MaximumMeanDelta) ||
			!Root->TryGetNumberField(
				TEXT("stable_frame_pair_count"),
				StablePairs) ||
			!Root->TryGetNumberField(
				TEXT("remaining_streaming_requests"),
				Streaming) ||
			!Root->TryGetNumberField(
				TEXT("remaining_asset_compile_count"),
				Compiling) ||
			!Root->TryGetBoolField(
				TEXT("shader_compilation_active"),
				OutReceipt.bShaderCompilationActive) ||
			!Root->TryGetBoolField(TEXT("ui_clean"), OutReceipt.bUiClean) ||
			!Root->TryGetStringField(
				TEXT("capture_started_at_utc"),
				OutReceipt.CaptureStartedAtUtc) ||
			!Root->TryGetStringField(
				TEXT("capture_completed_at_utc"),
				OutReceipt.CaptureCompletedAtUtc) ||
			!Root->TryGetStringField(
				TEXT("evidence_file_modified_at_utc"),
				OutReceipt.EvidenceFileModifiedAtUtc))
		{
			OutError = TEXT("Evidence receipt is missing required fields.");
			return false;
		}

		OutReceipt.ContractVersion = static_cast<int32>(ContractVersion);
		OutReceipt.WidthPixels = static_cast<int32>(Width);
		OutReceipt.HeightPixels = static_cast<int32>(Height);
		OutReceipt.AppliedMinimumWidthPixels =
			static_cast<int32>(AppliedMinimumWidth);
		OutReceipt.AppliedMinimumHeightPixels =
			static_cast<int32>(AppliedMinimumHeight);
		OutReceipt.ByteCount = static_cast<int64>(ByteCount);
		OutReceipt.CameraFieldOfViewDegrees = static_cast<float>(FOV);
		OutReceipt.CameraAspectRatio = static_cast<float>(AspectRatio);
		OutReceipt.CameraPostProcessBlendWeight =
			static_cast<float>(PostProcessWeight);
		OutReceipt.CameraAutoExposureBias = static_cast<float>(ExposureBias);
		OutReceipt.CameraAutoExposureMinBrightness = static_cast<float>(ExposureMin);
		OutReceipt.CameraAutoExposureMaxBrightness = static_cast<float>(ExposureMax);
		OutReceipt.ViewportFixedEV100 = static_cast<float>(FixedEV100);
		OutReceipt.WarmupDrawCount = static_cast<int32>(Warmup);
		OutReceipt.StabilizationDrawCount = static_cast<int32>(Stabilization);
		OutReceipt.FinalMeanPixelDelta = MeanDelta;
		OutReceipt.MaximumMeanPixelDelta = MaximumMeanDelta;
		OutReceipt.StableFramePairCount = static_cast<int32>(StablePairs);
		OutReceipt.RemainingStreamingRequests = static_cast<int32>(Streaming);
		OutReceipt.RemainingAssetCompileCount = static_cast<int32>(Compiling);
		return true;
	}

	bool ValidateCurrentCameraState(
		ACameraActor* ExpectedCameraActor,
		FLevelEditorViewportClient* ViewportClient,
		const FVisualEvidenceReceipt& Receipt,
		FString& OutError)
	{
		if (!IsValid(ExpectedCameraActor) ||
			!IsValid(ExpectedCameraActor->GetWorld()) ||
			!IsValid(ExpectedCameraActor->GetCameraComponent()))
		{
			OutError = TEXT("ExpectedCameraActor is not a loaded camera.");
			return false;
		}
		UCameraComponent* CameraComponent =
			ExpectedCameraActor->GetCameraComponent();
		FString CurrentCameraViewStateSha256;
		if (ExpectedCameraActor->GetPathName() != Receipt.CameraObjectPath ||
			ExpectedCameraActor->GetWorld()->GetPackage()->GetName() !=
				Receipt.WorldPackageName ||
			!ExpectedCameraActor->GetActorLocation().Equals(
				Receipt.CameraLocationCentimeters,
				0.01) ||
			!ExpectedCameraActor->GetActorRotation().Equals(
				Receipt.CameraRotationDegrees,
				0.001) ||
			!FMath::IsNearlyEqual(
				CameraComponent->FieldOfView,
				Receipt.CameraFieldOfViewDegrees,
				0.001f) ||
			ProjectionModeString(CameraComponent) != Receipt.CameraProjectionMode ||
			!FMath::IsNearlyEqual(
				CameraComponent->AspectRatio,
				Receipt.CameraAspectRatio,
				0.0001f) ||
			CameraComponent->bConstrainAspectRatio !=
				Receipt.bCameraConstrainAspectRatio ||
			!FMath::IsNearlyEqual(
				CameraComponent->PostProcessBlendWeight,
				Receipt.CameraPostProcessBlendWeight,
				0.0001f) ||
			ExposureMethodString(CameraComponent) !=
				Receipt.CameraAutoExposureMethod ||
			!FMath::IsNearlyEqual(
				CameraComponent->PostProcessSettings.AutoExposureBias,
				Receipt.CameraAutoExposureBias,
				0.0001f) ||
			!FMath::IsNearlyEqual(
				CameraComponent->PostProcessSettings.AutoExposureMinBrightness,
				Receipt.CameraAutoExposureMinBrightness,
				0.0001f) ||
			!FMath::IsNearlyEqual(
				CameraComponent->PostProcessSettings.AutoExposureMaxBrightness,
				Receipt.CameraAutoExposureMaxBrightness,
				0.0001f) ||
			!ComputeCameraViewStateSha256(
				CameraComponent,
				CurrentCameraViewStateSha256) ||
			CurrentCameraViewStateSha256 != Receipt.CameraViewStateSha256 ||
			ViewportClient->ExposureSettings.bFixed !=
				Receipt.bViewportFixedExposure ||
			!FMath::IsNearlyEqual(
				ViewportClient->ExposureSettings.FixedEV100,
				Receipt.ViewportFixedEV100,
				0.0001f) ||
			ViewModeString(ViewportClient) != Receipt.ViewMode)
		{
			OutError =
				TEXT("Current camera or viewport view state differs from the receipt.");
			return false;
		}
		return true;
	}

	bool VerifyEvidenceInternal(
		ACameraActor* ExpectedCameraActor,
		const FVisualEvidenceVerificationRequest& Request,
		FVisualEvidenceReceipt& OutReceipt,
		FString& OutErrorCode,
		FString& OutError)
	{
		if (!IsValidCameraRole(Request.ExpectedCameraRole) ||
			!IsValidSha256(Request.ExpectedEvidenceSha256) ||
			!IsValidSha256(Request.ExpectedReceiptSha256) ||
			Request.MinimumWidthPixels < EvidencePolicyMinimumWidthPixels ||
			Request.MinimumHeightPixels < EvidencePolicyMinimumHeightPixels)
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_REQUEST_INVALID");
			OutError = TEXT("Expected role and minimum dimensions are invalid.");
			return false;
		}

		FString EvidenceRelativePath;
		FString EvidenceAbsolutePath;
		FString ReceiptRelativePath;
		FString ReceiptAbsolutePath;
		if (!ResolveEvidencePaths(
			Request.EvidenceRelativePath,
			EvidenceRelativePath,
			EvidenceAbsolutePath,
			ReceiptRelativePath,
			ReceiptAbsolutePath,
			OutError))
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_REQUEST_INVALID");
			return false;
		}

		FLevelEditorViewportClient* ViewportClient =
			GCurrentLevelEditingViewportClient;
		if (ViewportClient == nullptr ||
			ViewportClient->Viewport == nullptr ||
			!ViewportClient->IsPerspective())
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VIEWPORT_UNAVAILABLE");
			OutError = TEXT("A current perspective level viewport is required.");
			return false;
		}

		TArray64<uint8> EvidenceBytes;
		TArray64<uint8> ReceiptBytes;
		FString ReceiptJson;
		if (!FFileHelper::LoadFileToArray(EvidenceBytes, *EvidenceAbsolutePath) ||
			!FFileHelper::LoadFileToArray(ReceiptBytes, *ReceiptAbsolutePath) ||
			!FFileHelper::LoadFileToString(ReceiptJson, *ReceiptAbsolutePath))
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VERIFY_FAILED");
			OutError = TEXT("Evidence PNG or receipt is missing or unreadable.");
			return false;
		}

		FVisualEvidenceReceipt Receipt;
		if (!DeserializeReceipt(ReceiptJson, Receipt, OutError))
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VERIFY_FAILED");
			return false;
		}
		Receipt.CameraActor = ExpectedCameraActor;
		Receipt.EvidenceAbsolutePath = EvidenceAbsolutePath;
		Receipt.ReceiptAbsolutePath = ReceiptAbsolutePath;
		if (!ComputeSha256(ReceiptBytes, Receipt.ReceiptSha256))
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VERIFY_FAILED");
			OutError = TEXT("Receipt SHA-256 could not be computed.");
			return false;
		}
		if (!Receipt.ReceiptSha256.Equals(
			Request.ExpectedReceiptSha256,
			ESearchCase::IgnoreCase))
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VERIFY_FAILED");
			OutError =
				TEXT("Receipt SHA-256 differs from the capture-returned trust anchor.");
			return false;
		}

		int32 PngWidth = 0;
		int32 PngHeight = 0;
		FString EvidenceHash;
		const FString CurrentModifiedTime =
			IFileManager::Get().GetTimeStamp(*EvidenceAbsolutePath).ToIso8601();
		FDateTime StartedAt;
		FDateTime CompletedAt;
		FDateTime ModifiedAt;
		if (Receipt.ContractVersion != ReceiptContractVersion ||
			Receipt.CameraRole != Request.ExpectedCameraRole ||
			Receipt.EvidenceRelativePath != EvidenceRelativePath ||
			Receipt.ReceiptRelativePath != ReceiptRelativePath)
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VERIFY_FAILED");
			OutError = TEXT("Receipt version, role, or relative paths do not match the request.");
			return false;
		}
		if (!ReadPngDimensions(EvidenceBytes, PngWidth, PngHeight) ||
			PngWidth != Receipt.WidthPixels ||
			PngHeight != Receipt.HeightPixels ||
			Receipt.AppliedMinimumWidthPixels <
				EvidencePolicyMinimumWidthPixels ||
			Receipt.AppliedMinimumHeightPixels <
				EvidencePolicyMinimumHeightPixels ||
			PngWidth < Receipt.AppliedMinimumWidthPixels ||
			PngHeight < Receipt.AppliedMinimumHeightPixels ||
			PngWidth < Request.MinimumWidthPixels ||
			PngHeight < Request.MinimumHeightPixels)
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VERIFY_FAILED");
			OutError = TEXT("PNG dimensions or applied minimum-dimension policy do not match.");
			return false;
		}
		if (EvidenceBytes.Num() != Receipt.ByteCount ||
			!ComputeSha256(EvidenceBytes, EvidenceHash) ||
			EvidenceHash != Receipt.EvidenceSha256 ||
			!EvidenceHash.Equals(
				Request.ExpectedEvidenceSha256,
				ESearchCase::IgnoreCase) ||
			!IsValidSha256(Receipt.CameraViewStateSha256))
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VERIFY_FAILED");
			OutError = TEXT("PNG byte count, evidence hash, or camera-view digest does not match.");
			return false;
		}
		if (CurrentModifiedTime != Receipt.EvidenceFileModifiedAtUtc)
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VERIFY_FAILED");
			OutError = TEXT("PNG modification time differs from the committed receipt.");
			return false;
		}
		if (!FDateTime::ParseIso8601(
				*Receipt.CaptureStartedAtUtc,
				StartedAt) ||
			!FDateTime::ParseIso8601(
				*Receipt.CaptureCompletedAtUtc,
				CompletedAt) ||
			!FDateTime::ParseIso8601(
				*Receipt.EvidenceFileModifiedAtUtc,
				ModifiedAt))
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VERIFY_FAILED");
			OutError = TEXT("Receipt timestamps are not valid ISO-8601 values.");
			return false;
		}
		const FTimespan FileTimestampPrecisionTolerance =
			FTimespan::FromSeconds(2.0);
		if (
			ModifiedAt + FileTimestampPrecisionTolerance < StartedAt ||
			CompletedAt + FileTimestampPrecisionTolerance < ModifiedAt)
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VERIFY_FAILED");
			OutError = TEXT("Receipt timestamp ordering does not prove an atomic capture.");
			return false;
		}
		if (Receipt.WarmupDrawCount < 3 ||
			Receipt.StableFramePairCount < 2 ||
			!FMath::IsFinite(Receipt.MaximumMeanPixelDelta) ||
			!FMath::IsFinite(Receipt.FinalMeanPixelDelta) ||
			Receipt.MaximumMeanPixelDelta < 0.0 ||
			Receipt.MaximumMeanPixelDelta >
				EvidencePolicyMaximumMeanPixelDelta ||
			Receipt.FinalMeanPixelDelta >
				Receipt.MaximumMeanPixelDelta ||
			!Receipt.bCameraViewApplied ||
			Receipt.RemainingStreamingRequests != 0 ||
			Receipt.RemainingAssetCompileCount != 0 ||
			Receipt.bShaderCompilationActive ||
			!Receipt.bUiClean)
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_VERIFY_FAILED");
			OutError =
				TEXT("Stability, camera-application, UI-clean, or render-settlement fields do not pass.");
			return false;
		}

		if (!ValidateCurrentCameraState(
			ExpectedCameraActor,
			ViewportClient,
			Receipt,
			OutError))
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_CAMERA_MISMATCH");
			return false;
		}

		const int32 CurrentAssetCompileCount =
			FAssetCompilingManager::Get().GetNumRemainingAssets();
		const bool bShaderCompilationActive =
			GShaderCompilingManager != nullptr &&
			GShaderCompilingManager->IsCompiling();
		const int32 CurrentStreamingRequests =
			IStreamingManager::Get().GetNumWantingResources();
		if (CurrentAssetCompileCount != 0 ||
			bShaderCompilationActive ||
			CurrentStreamingRequests != 0)
		{
			OutErrorCode = TEXT("VISUAL_EVIDENCE_RENDER_STATE_UNSETTLED");
			OutError =
				TEXT("Current asset compilation, shader compilation, or streaming is not settled.");
			return false;
		}

		OutReceipt = MoveTemp(Receipt);
		return true;
	}
}

FVisualEvidenceReceipt UVisualEvidenceExtensionToolset::CaptureCameraToPng(
	ACameraActor* CameraActor,
	const FVisualEvidenceCaptureRequest& Request)
{
	using namespace VisualEvidenceExtensionToolsetPrivate;

	if (!IsValidCameraRole(Request.CameraRole) ||
		Request.WarmupDrawCount < 3 ||
		Request.WarmupDrawCount > 8 ||
		Request.MaxStabilizationDrawCount < 3 ||
		Request.MaxStabilizationDrawCount > 16 ||
		!FMath::IsFinite(Request.MaxMeanPixelDelta) ||
		Request.MaxMeanPixelDelta < 0.0 ||
		Request.MaxMeanPixelDelta > EvidencePolicyMaximumMeanPixelDelta ||
		Request.MinimumWidthPixels < EvidencePolicyMinimumWidthPixels ||
		Request.MinimumHeightPixels < EvidencePolicyMinimumHeightPixels)
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_REQUEST_INVALID"),
			TEXT("Role, warmup, stabilization, pixel delta, or minimum dimensions are invalid."));
		return {};
	}
	if (!IsValid(CameraActor) ||
		!IsValid(CameraActor->GetWorld()) ||
		!IsValid(CameraActor->GetCameraComponent()))
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_CAMERA_INVALID"),
			TEXT("CameraActor must be a loaded camera with a valid CameraComponent."));
		return {};
	}

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!IsValid(EditorWorld) || CameraActor->GetWorld() != EditorWorld)
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_CAMERA_INVALID"),
			TEXT("CameraActor must belong to the current Editor world."));
		return {};
	}

	const UCameraComponent* CameraComponent = CameraActor->GetCameraComponent();
	if (CameraComponent->ProjectionMode != ECameraProjectionMode::Perspective ||
		!FMath::IsFinite(CameraComponent->FieldOfView) ||
		CameraComponent->FieldOfView <= 0.0f ||
		CameraComponent->FieldOfView >= 180.0f)
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_CAMERA_INVALID"),
			TEXT("CameraActor must be a perspective camera with a finite 0-180 degree FOV."));
		return {};
	}

	FLevelEditorViewportClient* ViewportClient =
		GCurrentLevelEditingViewportClient;
	if (ViewportClient == nullptr ||
		ViewportClient->Viewport == nullptr ||
		!ViewportClient->IsPerspective())
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_VIEWPORT_UNAVAILABLE"),
			TEXT("A current perspective level viewport is required."));
		return {};
	}
	if (ViewportClient->GetCinematicActorLock().HasValidLockedActor())
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_VIEWPORT_UNAVAILABLE"),
			TEXT("The current viewport has an active cinematic lock; use an unlocked review viewport."));
		return {};
	}

	FViewport* Viewport = ViewportClient->Viewport;
	const FIntPoint Size = Viewport->GetSizeXY();
	if (Size.X < Request.MinimumWidthPixels ||
		Size.Y < Request.MinimumHeightPixels)
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_DIMENSIONS_TOO_SMALL"),
			FString::Printf(
				TEXT("Viewport is %dx%d but evidence requires at least %dx%d."),
				Size.X,
				Size.Y,
				Request.MinimumWidthPixels,
				Request.MinimumHeightPixels));
		return {};
	}
	if (CameraComponent->bConstrainAspectRatio)
	{
		const float ViewportAspectRatio =
			static_cast<float>(Size.X) / static_cast<float>(Size.Y);
		if (!FMath::IsNearlyEqual(
			ViewportAspectRatio,
			CameraComponent->AspectRatio,
			0.01f))
		{
			RaiseError(
				TEXT("VISUAL_EVIDENCE_CAMERA_INVALID"),
				FString::Printf(
					TEXT("Constrained camera aspect ratio %.4f does not match viewport ratio %.4f."),
					CameraComponent->AspectRatio,
					ViewportAspectRatio));
			return {};
		}
	}

	FString EvidenceRelativePath;
	FString EvidenceAbsolutePath;
	FString ReceiptRelativePath;
	FString ReceiptAbsolutePath;
	FString PathError;
	if (!ResolveEvidencePaths(
		Request.OutputRelativePath,
		EvidenceRelativePath,
		EvidenceAbsolutePath,
		ReceiptRelativePath,
		ReceiptAbsolutePath,
		PathError))
	{
		RaiseError(TEXT("VISUAL_EVIDENCE_REQUEST_INVALID"), PathError);
		return {};
	}

	IFileManager& FileManager = IFileManager::Get();
	if (FileManager.FileExists(*EvidenceAbsolutePath) ||
		FileManager.DirectoryExists(*EvidenceAbsolutePath) ||
		FileManager.FileExists(*ReceiptAbsolutePath) ||
		FileManager.DirectoryExists(*ReceiptAbsolutePath))
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_OUTPUT_EXISTS"),
			TEXT("Evidence PNG or receipt path already exists; use a new versioned path."));
		return {};
	}
	if (!FileManager.MakeDirectory(*FPaths::GetPath(EvidenceAbsolutePath), true))
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_SAVE_FAILED"),
			TEXT("Evidence output directory could not be created."));
		return {};
	}

	FVisualEvidenceReceipt Receipt;
	Receipt.ContractVersion = ReceiptContractVersion;
	Receipt.CameraRole = Request.CameraRole;
	Receipt.EvidenceRelativePath = EvidenceRelativePath;
	Receipt.EvidenceAbsolutePath = EvidenceAbsolutePath;
	Receipt.ReceiptRelativePath = ReceiptRelativePath;
	Receipt.ReceiptAbsolutePath = ReceiptAbsolutePath;
	Receipt.WarmupDrawCount = Request.WarmupDrawCount;
	Receipt.WidthPixels = Size.X;
	Receipt.HeightPixels = Size.Y;
	Receipt.AppliedMinimumWidthPixels = Request.MinimumWidthPixels;
	Receipt.AppliedMinimumHeightPixels = Request.MinimumHeightPixels;
	Receipt.MaximumMeanPixelDelta = Request.MaxMeanPixelDelta;
	Receipt.CaptureStartedAtUtc = FDateTime::UtcNow().ToIso8601();
	PopulateCameraState(CameraActor, ViewportClient, Receipt);
	if (!IsValidSha256(Receipt.CameraViewStateSha256))
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_CAMERA_INVALID"),
			TEXT("The complete camera view and post-process state could not be hashed."));
		return {};
	}

	Receipt.RemainingAssetCompileCount =
		FAssetCompilingManager::Get().GetNumRemainingAssets();
	Receipt.bShaderCompilationActive =
		GShaderCompilingManager != nullptr &&
		GShaderCompilingManager->IsCompiling();
	if (Receipt.RemainingAssetCompileCount != 0 ||
		Receipt.bShaderCompilationActive)
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_RENDER_STATE_UNSETTLED"),
			TEXT("Asset or shader compilation must finish before evidence capture."));
		return {};
	}

	Receipt.RemainingStreamingRequests =
		IStreamingManager::Get().StreamAllResources(5.0f);
	if (Receipt.RemainingStreamingRequests != 0)
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_RENDER_STATE_UNSETTLED"),
			TEXT("Streaming did not settle within the evidence capture time limit."));
		return {};
	}

	const FVector SavedLocation = ViewportClient->GetViewLocation();
	const FRotator SavedRotation = ViewportClient->GetViewRotation();
	const float SavedViewFOV = ViewportClient->ViewFOV;
	const FEngineShowFlags SavedShowFlags = ViewportClient->EngineShowFlags;
	const FLevelViewportActorLock SavedActorLock = ViewportClient->GetActorLock();
	const bool bSavedLockedCameraView = ViewportClient->bLockedCameraView;
	const bool bSavedShowStats = ViewportClient->ShouldShowStats();
	ON_SCOPE_EXIT
	{
		ViewportClient->SetActorLock(SavedActorLock);
		ViewportClient->bLockedCameraView = bSavedLockedCameraView;
		ViewportClient->UpdateViewForLockedActor();
		ViewportClient->SetViewLocation(SavedLocation);
		ViewportClient->SetViewRotation(SavedRotation);
		ViewportClient->ViewFOV = SavedViewFOV;
		ViewportClient->EngineShowFlags = SavedShowFlags;
		ViewportClient->SetShowStats(bSavedShowStats);
		Viewport->Draw();
		FlushRenderingCommands();
	};

	ViewportClient->SetActorLock(CameraActor);
	ViewportClient->bLockedCameraView = true;
	ViewportClient->UpdateViewForLockedActor();
	Receipt.bCameraViewApplied =
		ViewportClient->GetCameraComponentForView() == CameraComponent;
	if (!Receipt.bCameraViewApplied)
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_CAMERA_INVALID"),
			TEXT("The persistent camera could not become the viewport's active camera view."));
		return {};
	}
	ViewportClient->EngineShowFlags.SetModeWidgets(false);
	ViewportClient->EngineShowFlags.SetSelectionOutline(false);
	ViewportClient->EngineShowFlags.SetSelection(false);
	ViewportClient->EngineShowFlags.SetGrid(false);
	ViewportClient->SetShowStats(false);
	ViewportClient->EngineShowFlags.SetOnScreenDebug(false);
	ViewportClient->EngineShowFlags.SetCompositeEditorPrimitives(false);
	ViewportClient->EngineShowFlags.SetCameraFrustums(false);
	ViewportClient->EngineShowFlags.SetBounds(false);
	ViewportClient->EngineShowFlags.SetCollision(false);
	ViewportClient->EngineShowFlags.SetBrushes(false);
	ViewportClient->EngineShowFlags.SetVolumes(false);
	ViewportClient->EngineShowFlags.SetSplines(false);
	ViewportClient->EngineShowFlags.SetConstraints(false);
	Receipt.bUiClean = true;

	for (int32 DrawIndex = 0; DrawIndex < Request.WarmupDrawCount; ++DrawIndex)
	{
		Viewport->Draw();
		FlushRenderingCommands();
	}

	TArray<FColor> PreviousBitmap;
	if (!GetViewportScreenShot(Viewport, PreviousBitmap) ||
		PreviousBitmap.Num() != Size.X * Size.Y)
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_CAPTURE_FAILED"),
			TEXT("Initial settled-frame sample could not be captured."));
		return {};
	}

	TArray<FColor> AcceptedBitmap;
	int32 StablePairCount = 0;
	for (int32 DrawIndex = 1;
		DrawIndex <= Request.MaxStabilizationDrawCount;
		++DrawIndex)
	{
		Viewport->Draw();
		FlushRenderingCommands();

		TArray<FColor> CurrentBitmap;
		if (!GetViewportScreenShot(Viewport, CurrentBitmap) ||
			CurrentBitmap.Num() != Size.X * Size.Y)
		{
			RaiseError(
				TEXT("VISUAL_EVIDENCE_CAPTURE_FAILED"),
				TEXT("Temporal stability sample could not be captured."));
			return {};
		}

		Receipt.FinalMeanPixelDelta =
			MeanPixelDelta(PreviousBitmap, CurrentBitmap);
		StablePairCount =
			Receipt.FinalMeanPixelDelta <= Request.MaxMeanPixelDelta
			? StablePairCount + 1
			: 0;
		Receipt.StabilizationDrawCount = DrawIndex;
		Receipt.StableFramePairCount = StablePairCount;
		PreviousBitmap = MoveTemp(CurrentBitmap);
		if (StablePairCount >= 2)
		{
			AcceptedBitmap = PreviousBitmap;
			break;
		}
	}
	if (Receipt.StableFramePairCount < 2 || AcceptedBitmap.IsEmpty())
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_RENDER_STATE_UNSETTLED"),
			TEXT("Two consecutive stable frame comparisons were not obtained."));
		return {};
	}

	Receipt.RemainingAssetCompileCount =
		FAssetCompilingManager::Get().GetNumRemainingAssets();
	Receipt.bShaderCompilationActive =
		GShaderCompilingManager != nullptr &&
		GShaderCompilingManager->IsCompiling();
	Receipt.RemainingStreamingRequests =
		IStreamingManager::Get().GetNumWantingResources();
	if (Receipt.RemainingAssetCompileCount != 0 ||
		Receipt.bShaderCompilationActive ||
		Receipt.RemainingStreamingRequests != 0)
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_RENDER_STATE_UNSETTLED"),
			TEXT("Compilation or streaming resumed before the stable frame was accepted."));
		return {};
	}

	for (FColor& Pixel : AcceptedBitmap)
	{
		Pixel.A = 255;
	}
	TArray64<uint8> EncodedPng;
	FImageUtils::PNGCompressImageArray(
		Size.X,
		Size.Y,
		TArrayView64<const FColor>(
			AcceptedBitmap.GetData(),
			AcceptedBitmap.Num()),
		EncodedPng);
	if (!HasPngSignature(EncodedPng) ||
		!ComputeSha256(EncodedPng, Receipt.EvidenceSha256))
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_CAPTURE_FAILED"),
			TEXT("Stable framebuffer did not encode to a verifiable PNG."));
		return {};
	}
	Receipt.ByteCount = EncodedPng.Num();

	const FString TemporarySuffix =
		FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString TemporaryEvidencePath =
		EvidenceAbsolutePath + TEXT(".tmp-") + TemporarySuffix;
	const FString TemporaryReceiptPath =
		ReceiptAbsolutePath + TEXT(".tmp-") + TemporarySuffix;
	ON_SCOPE_EXIT
	{
		if (FileManager.FileExists(*TemporaryEvidencePath))
		{
			FileManager.Delete(*TemporaryEvidencePath, false, true, true);
		}
		if (FileManager.FileExists(*TemporaryReceiptPath))
		{
			FileManager.Delete(*TemporaryReceiptPath, false, true, true);
		}
	};

	if (!FFileHelper::SaveArrayToFile(EncodedPng, *TemporaryEvidencePath) ||
		!FileManager.Move(
			*EvidenceAbsolutePath,
			*TemporaryEvidencePath,
			false,
			false,
			false,
			true))
	{
		RaiseError(
			TEXT("VISUAL_EVIDENCE_SAVE_FAILED"),
			TEXT("Verified PNG could not be committed to its unique final path."));
		return {};
	}

	Receipt.EvidenceFileModifiedAtUtc =
		FileManager.GetTimeStamp(*EvidenceAbsolutePath).ToIso8601();
	Receipt.CaptureCompletedAtUtc = FDateTime::UtcNow().ToIso8601();
	FString ReceiptJson;
	if (!SerializeReceipt(Receipt, ReceiptJson) ||
		!FFileHelper::SaveStringToFile(
			ReceiptJson,
			*TemporaryReceiptPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
		!FileManager.Move(
			*ReceiptAbsolutePath,
			*TemporaryReceiptPath,
			false,
			false,
			false,
			true))
	{
		FileManager.Delete(*EvidenceAbsolutePath, false, true, true);
		RaiseError(
			TEXT("VISUAL_EVIDENCE_SAVE_FAILED"),
			TEXT("Evidence receipt could not be committed; the unpaired PNG was removed."));
		return {};
	}

	FVisualEvidenceVerificationRequest VerificationRequest;
	VerificationRequest.EvidenceRelativePath = EvidenceRelativePath;
	VerificationRequest.ExpectedCameraRole = Request.CameraRole;
	VerificationRequest.ExpectedEvidenceSha256 = Receipt.EvidenceSha256;
	TArray64<uint8> CommittedReceiptBytes;
	if (!FFileHelper::LoadFileToArray(
			CommittedReceiptBytes,
			*ReceiptAbsolutePath) ||
		!ComputeSha256(
			CommittedReceiptBytes,
			VerificationRequest.ExpectedReceiptSha256))
	{
		FileManager.Delete(*EvidenceAbsolutePath, false, true, true);
		FileManager.Delete(*ReceiptAbsolutePath, false, true, true);
		RaiseError(
			TEXT("VISUAL_EVIDENCE_SAVE_FAILED"),
			TEXT("Committed receipt trust-anchor hash could not be computed."));
		return {};
	}
	VerificationRequest.MinimumWidthPixels = Request.MinimumWidthPixels;
	VerificationRequest.MinimumHeightPixels = Request.MinimumHeightPixels;
	FVisualEvidenceReceipt VerifiedReceipt;
	FString VerificationErrorCode;
	FString VerificationError;
	if (!VerifyEvidenceInternal(
		CameraActor,
		VerificationRequest,
		VerifiedReceipt,
		VerificationErrorCode,
		VerificationError))
	{
		FileManager.Delete(*EvidenceAbsolutePath, false, true, true);
		FileManager.Delete(*ReceiptAbsolutePath, false, true, true);
		RaiseError(
			*VerificationErrorCode,
			FString::Printf(
				TEXT("Post-capture handoff verification failed: %s"),
				*VerificationError));
		return {};
	}
	return VerifiedReceipt;
}

FVisualEvidenceReceipt
UVisualEvidenceExtensionToolset::VerifyEvidenceForSupervision(
	ACameraActor* ExpectedCameraActor,
	const FVisualEvidenceVerificationRequest& Request)
{
	using namespace VisualEvidenceExtensionToolsetPrivate;

	FVisualEvidenceReceipt Receipt;
	FString ErrorCode;
	FString Error;
	if (!VerifyEvidenceInternal(
		ExpectedCameraActor,
		Request,
		Receipt,
		ErrorCode,
		Error))
	{
		RaiseError(*ErrorCode, Error);
		return {};
	}
	return Receipt;
}
