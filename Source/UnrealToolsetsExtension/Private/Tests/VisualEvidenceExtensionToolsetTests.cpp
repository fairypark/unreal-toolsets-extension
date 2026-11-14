#if WITH_DEV_AUTOMATION_TESTS

#include "VisualEvidenceExtensionToolset.h"

#include "AssetCompilingManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "IAssetViewport.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationEditorCommon.h"

namespace VisualEvidenceExtensionToolsetTests
{
	struct FScopedLargeAutomationViewport
	{
		TSharedPtr<IAssetViewport> AssetViewport;
		TSharedPtr<FSceneViewport> SceneViewport;
		FLevelEditorViewportClient* OriginalCurrentViewportClient = nullptr;
		FIntPoint OriginalSize = FIntPoint::ZeroValue;
		EWindowMode::Type OriginalWindowMode = EWindowMode::Windowed;
		bool bRestoreImmersive = false;

		FScopedLargeAutomationViewport()
		{
			OriginalCurrentViewportClient = GCurrentLevelEditingViewportClient;
			FLevelEditorModule& LevelEditorModule =
				FModuleManager::LoadModuleChecked<FLevelEditorModule>(
					TEXT("LevelEditor"));
			AssetViewport = LevelEditorModule.GetFirstActiveViewport();
			if (!AssetViewport.IsValid() ||
				AssetViewport->GetActiveViewport() == nullptr)
			{
				return;
			}

			SceneViewport = AssetViewport->GetSharedActiveViewport();
			if (!SceneViewport.IsValid())
			{
				return;
			}
			FViewport* Viewport = SceneViewport.Get();
			GCurrentLevelEditingViewportClient =
				static_cast<FLevelEditorViewportClient*>(
					&AssetViewport->GetAssetViewportClient());
			OriginalSize = Viewport->GetSizeXY();
			OriginalWindowMode = SceneViewport->GetWindowMode();
			bRestoreImmersive = !AssetViewport->IsImmersive();
			if (bRestoreImmersive)
			{
				AssetViewport->MakeImmersive(true, false);
			}
			SceneViewport->UpdateViewportRHI(
				false,
				FMath::Max(1920, OriginalSize.X),
				FMath::Max(1080, OriginalSize.Y),
				OriginalWindowMode,
				PF_Unknown);
		}

		~FScopedLargeAutomationViewport()
		{
			if (!AssetViewport.IsValid() || !SceneViewport.IsValid())
			{
				GCurrentLevelEditingViewportClient =
					OriginalCurrentViewportClient;
				return;
			}
			if (OriginalSize.X > 0 && OriginalSize.Y > 0)
			{
				SceneViewport->UpdateViewportRHI(
					false,
					OriginalSize.X,
					OriginalSize.Y,
					OriginalWindowMode,
					PF_Unknown);
			}
			if (bRestoreImmersive)
			{
				AssetViewport->MakeImmersive(false, false);
			}
			GCurrentLevelEditingViewportClient = OriginalCurrentViewportClient;
		}
	};

	FString MakeTestDirectory()
	{
		return FPaths::Combine(
			TEXT("Automation"),
			TEXT("VisualEvidence"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	void DeleteEvidencePair(const FVisualEvidenceReceipt& Receipt)
	{
		IFileManager::Get().Delete(*Receipt.EvidenceAbsolutePath, false, true, true);
		IFileManager::Get().Delete(*Receipt.ReceiptAbsolutePath, false, true, true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVisualEvidenceExtensionCaptureAndVerifyTest,
	"AI.UnrealToolsetsExtension.VisualEvidence.CaptureAndVerifyEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVisualEvidenceExtensionCaptureAndVerifyTest::RunTest(
	const FString& Parameters)
{
	using namespace VisualEvidenceExtensionToolsetTests;

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Disposable Editor world was created"), World);
	if (!IsValid(World))
	{
		return false;
	}
	const TSharedRef<FScopedLargeAutomationViewport> LargeViewport =
		MakeShared<FScopedLargeAutomationViewport>();

	ACameraActor* Camera = World->SpawnActor<ACameraActor>();
	TestNotNull(TEXT("Persistent evidence camera was created"), Camera);
	if (!IsValid(Camera))
	{
		return false;
	}
	Camera->SetActorLabel(TEXT("VQCam_Automation_Hero"));
	Camera->SetActorLocation(FVector(1200.0, -800.0, 350.0));
	Camera->SetActorRotation(FRotator(-12.0, 37.0, 0.0));
	Camera->GetCameraComponent()->FieldOfView = 47.0f;
	Camera->GetCameraComponent()->bConstrainAspectRatio = false;

	// Creating a disposable world/camera can start lazy material compilation on
	// first draw. Yield to real Editor ticks instead of capturing in that frame.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(
		[this, World, Camera, LargeViewport,
		 StartedAt = FPlatformTime::Seconds(), ReadyDraws = 0]() mutable -> bool
		{
			if (!IsValid(World) || !IsValid(Camera))
			{
				AddError(TEXT("Disposable visual evidence fixture was lost during prewarm."));
				return true;
			}
			if (FPlatformTime::Seconds() - StartedAt > 120.0)
			{
				AddError(TEXT("Visual evidence fixture compilation did not settle within 120 seconds."));
				World->DestroyActor(Camera);
				return true;
			}
			FLevelEditorViewportClient* PrewarmClient = GCurrentLevelEditingViewportClient;
			if (PrewarmClient == nullptr || PrewarmClient->Viewport == nullptr)
			{
				AddError(TEXT("Visual evidence fixture has no viewport for prewarm."));
				World->DestroyActor(Camera);
				return true;
			}
			const FLevelViewportActorLock SavedLock = PrewarmClient->GetActorLock();
			const bool bSavedLockedCameraView = PrewarmClient->bLockedCameraView;
			const FVector SavedLocation = PrewarmClient->GetViewLocation();
			const FRotator SavedRotation = PrewarmClient->GetViewRotation();
			const float SavedFOV = PrewarmClient->ViewFOV;
			PrewarmClient->SetActorLock(Camera);
			PrewarmClient->bLockedCameraView = true;
			PrewarmClient->UpdateViewForLockedActor();
			PrewarmClient->Viewport->Draw();
			FlushRenderingCommands();
			PrewarmClient->SetActorLock(SavedLock);
			PrewarmClient->bLockedCameraView = bSavedLockedCameraView;
			PrewarmClient->UpdateViewForLockedActor();
			PrewarmClient->SetViewLocation(SavedLocation);
			PrewarmClient->SetViewRotation(SavedRotation);
			PrewarmClient->ViewFOV = SavedFOV;

			const bool bCompilationPending =
				FAssetCompilingManager::Get().GetNumRemainingAssets() != 0 ||
				(GShaderCompilingManager != nullptr && GShaderCompilingManager->IsCompiling());
			ReadyDraws = bCompilationPending ? 0 : ReadyDraws + 1;
			if (ReadyDraws < 3)
			{
				return false;
			}

			// Production capture and all evidence assertions remain unchanged below.

			FEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
			TestNotNull(TEXT("A level viewport is available"), ViewportClient);
			if (ViewportClient == nullptr || ViewportClient->Viewport == nullptr)
			{
				World->DestroyActor(Camera);
				return true;
			}
			const FVector OriginalLocation = ViewportClient->GetViewLocation();
			const FRotator OriginalRotation = ViewportClient->GetViewRotation();
			const float OriginalFOV = ViewportClient->ViewFOV;
			const FIntPoint ViewportSize = ViewportClient->Viewport->GetSizeXY();
			if (ViewportSize.X < 1280 || ViewportSize.Y < 720)
			{
				AddError(FString::Printf(
					TEXT("Automation viewport must be at least 1280x720; actual size is %dx%d."),
					ViewportSize.X,
					ViewportSize.Y));
				World->DestroyActor(Camera);
				return true;
			}

			const FString TestDirectory = MakeTestDirectory();
			FVisualEvidenceCaptureRequest CaptureRequest;
			CaptureRequest.CameraRole = TEXT("hero");
			CaptureRequest.OutputRelativePath =
				FPaths::Combine(TestDirectory, TEXT("hero_v001.png"));
			CaptureRequest.WarmupDrawCount = 3;
			CaptureRequest.MinimumWidthPixels = 1280;
			CaptureRequest.MinimumHeightPixels = 720;

			const FVisualEvidenceReceipt Captured =
				UVisualEvidenceExtensionToolset::CaptureCameraToPng(
					Camera,
					CaptureRequest);
			if (Captured.EvidenceAbsolutePath.IsEmpty())
			{
				AddError(TEXT("Visual evidence capture returned an empty receipt."));
				World->DestroyActor(Camera);
				return true;
			}
			TestEqual(TEXT("Receipt contract version"), Captured.ContractVersion, 1);
			TestEqual(TEXT("Camera role is bound"), Captured.CameraRole, FString(TEXT("hero")));
			TestEqual(TEXT("Camera identity is bound"), Captured.CameraActor.Get(), Camera);
			TestEqual(TEXT("Camera object path is bound"), Captured.CameraObjectPath, Camera->GetPathName());
			TestTrue(TEXT("PNG is beneath Project Saved"), FPaths::IsUnderDirectory(
				Captured.EvidenceAbsolutePath,
				FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir())));
			TestTrue(TEXT("PNG exists"), IFileManager::Get().FileExists(
				*Captured.EvidenceAbsolutePath));
			TestTrue(TEXT("Receipt exists"), IFileManager::Get().FileExists(
				*Captured.ReceiptAbsolutePath));
			TestEqual(TEXT("Capture width matches viewport"), Captured.WidthPixels, ViewportSize.X);
			TestEqual(TEXT("Capture height matches viewport"), Captured.HeightPixels, ViewportSize.Y);
			TestEqual(TEXT("Receipt preserves minimum width policy"), Captured.AppliedMinimumWidthPixels, 1280);
			TestEqual(TEXT("Receipt preserves minimum height policy"), Captured.AppliedMinimumHeightPixels, 720);
			TestEqual(TEXT("Receipt preserves temporal threshold"), Captured.MaximumMeanPixelDelta, 0.5);
			TestTrue(TEXT("PNG has payload bytes"), Captured.ByteCount > 24);
			TestEqual(TEXT("PNG SHA-256 length"), Captured.EvidenceSha256.Len(), 64);
			TestEqual(TEXT("Receipt SHA-256 length"), Captured.ReceiptSha256.Len(), 64);
			TestEqual(TEXT("Complete camera view-state SHA-256 length"), Captured.CameraViewStateSha256.Len(), 64);
			TestTrue(TEXT("Warmup is at least three draws"), Captured.WarmupDrawCount >= 3);
			TestTrue(TEXT("Two stable frame pairs were proven"), Captured.StableFramePairCount >= 2);
			TestTrue(TEXT("Camera component view and post process were applied"), Captured.bCameraViewApplied);
			TestEqual(TEXT("Streaming is settled"), Captured.RemainingStreamingRequests, 0);
			TestEqual(TEXT("Asset compilation is settled"), Captured.RemainingAssetCompileCount, 0);
			TestFalse(TEXT("Shader compilation is settled"), Captured.bShaderCompilationActive);
			TestTrue(TEXT("Evidence is UI-clean"), Captured.bUiClean);
			TestFalse(TEXT("Capture start timestamp is present"), Captured.CaptureStartedAtUtc.IsEmpty());
			TestFalse(TEXT("Capture end timestamp is present"), Captured.CaptureCompletedAtUtc.IsEmpty());
			TestFalse(TEXT("PNG mtime is present"), Captured.EvidenceFileModifiedAtUtc.IsEmpty());
			TestTrue(TEXT("Viewport location is restored"), ViewportClient->GetViewLocation().Equals(
				OriginalLocation));
			TestTrue(TEXT("Viewport rotation is restored"), ViewportClient->GetViewRotation().Equals(
				OriginalRotation));
			TestEqual(TEXT("Viewport FOV is restored"), ViewportClient->ViewFOV, OriginalFOV);

			FVisualEvidenceVerificationRequest VerifyRequest;
			VerifyRequest.EvidenceRelativePath = Captured.EvidenceRelativePath;
			VerifyRequest.ExpectedCameraRole = TEXT("hero");
			VerifyRequest.ExpectedEvidenceSha256 = Captured.EvidenceSha256;
			VerifyRequest.ExpectedReceiptSha256 = Captured.ReceiptSha256;
			VerifyRequest.MinimumWidthPixels = 1280;
			VerifyRequest.MinimumHeightPixels = 720;
			const FVisualEvidenceReceipt Verified =
				UVisualEvidenceExtensionToolset::VerifyEvidenceForSupervision(
					Camera,
					VerifyRequest);
			TestEqual(TEXT("Verifier returns the same PNG hash"), Verified.EvidenceSha256, Captured.EvidenceSha256);
			TestEqual(TEXT("Verifier returns the same receipt path"), Verified.ReceiptAbsolutePath, Captured.ReceiptAbsolutePath);
			TestEqual(TEXT("Verifier returns a receipt SHA-256"), Verified.ReceiptSha256.Len(), 64);

			AddExpectedError(
				TEXT("VISUAL_EVIDENCE_OUTPUT_EXISTS"),
				EAutomationExpectedErrorFlags::Contains,
				1);
			const FVisualEvidenceReceipt Refused =
				UVisualEvidenceExtensionToolset::CaptureCameraToPng(
					Camera,
					CaptureRequest);
			TestTrue(TEXT("Existing pair is never overwritten"), Refused.EvidenceAbsolutePath.IsEmpty());

			FVisualEvidenceVerificationRequest WrongRoleRequest = VerifyRequest;
			WrongRoleRequest.ExpectedCameraRole = TEXT("reverse");
			AddExpectedError(
				TEXT("VISUAL_EVIDENCE_VERIFY_FAILED"),
				EAutomationExpectedErrorFlags::Contains,
				1);
			const FVisualEvidenceReceipt WrongRole =
				UVisualEvidenceExtensionToolset::VerifyEvidenceForSupervision(
					Camera,
					WrongRoleRequest);
			TestTrue(TEXT("Wrong camera role fails closed"), WrongRole.EvidenceAbsolutePath.IsEmpty());

			FVisualEvidenceVerificationRequest WrongReceiptHashRequest = VerifyRequest;
			WrongReceiptHashRequest.ExpectedReceiptSha256 =
				FString::ChrN(64, TEXT('0'));
			AddExpectedError(
				TEXT("VISUAL_EVIDENCE_VERIFY_FAILED"),
				EAutomationExpectedErrorFlags::Contains,
				1);
			const FVisualEvidenceReceipt WrongReceiptHash =
				UVisualEvidenceExtensionToolset::VerifyEvidenceForSupervision(
					Camera,
					WrongReceiptHashRequest);
			TestTrue(
				TEXT("Receipt differing from the external trust anchor fails closed"),
				WrongReceiptHash.EvidenceAbsolutePath.IsEmpty());

			ACameraActor* OtherCamera = World->SpawnActor<ACameraActor>();
			TestNotNull(TEXT("Mismatch camera was created"), OtherCamera);
			if (IsValid(OtherCamera))
			{
				AddExpectedError(
					TEXT("VISUAL_EVIDENCE_CAMERA_MISMATCH"),
					EAutomationExpectedErrorFlags::Contains,
					1);
				const FVisualEvidenceReceipt WrongCamera =
					UVisualEvidenceExtensionToolset::VerifyEvidenceForSupervision(
						OtherCamera,
						VerifyRequest);
				TestTrue(TEXT("Wrong camera fails closed"), WrongCamera.EvidenceAbsolutePath.IsEmpty());
				World->DestroyActor(OtherCamera);
			}

			TArray64<uint8> TamperedBytes;
			TamperedBytes.Add(0x89);
			TamperedBytes.Add(0x50);
			TestTrue(TEXT("Test can tamper the PNG"), FFileHelper::SaveArrayToFile(
				TamperedBytes,
				*Captured.EvidenceAbsolutePath));
			AddExpectedError(
				TEXT("VISUAL_EVIDENCE_VERIFY_FAILED"),
				EAutomationExpectedErrorFlags::Contains,
				1);
			const FVisualEvidenceReceipt Tampered =
				UVisualEvidenceExtensionToolset::VerifyEvidenceForSupervision(
					Camera,
					VerifyRequest);
			TestTrue(TEXT("Tampered PNG fails closed"), Tampered.EvidenceAbsolutePath.IsEmpty());

			DeleteEvidencePair(Captured);
			IFileManager::Get().DeleteDirectory(
				*FPaths::Combine(FPaths::ProjectSavedDir(), TestDirectory),
				false,
				true);
			World->DestroyActor(Camera);
			return true;
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVisualEvidenceExtensionValidationTest,
	"AI.UnrealToolsetsExtension.VisualEvidence.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVisualEvidenceExtensionValidationTest::RunTest(const FString& Parameters)
{
	using namespace VisualEvidenceExtensionToolsetTests;

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Disposable Editor world was created"), World);
	if (!IsValid(World))
	{
		return false;
	}
	FScopedLargeAutomationViewport LargeViewport;
	ACameraActor* Camera = World->SpawnActor<ACameraActor>();
	TestNotNull(TEXT("Validation camera was created"), Camera);
	if (!IsValid(Camera))
	{
		return false;
	}

	// Invalid-request tests must not fail earlier on the host window aspect ratio.
	Camera->GetCameraComponent()->bConstrainAspectRatio = false;

	FVisualEvidenceCaptureRequest Request;
	Request.CameraRole = TEXT("hero");
	Request.OutputRelativePath = TEXT("../escape.png");
	Request.MinimumWidthPixels = 1280;
	Request.MinimumHeightPixels = 720;
	AddExpectedError(
		TEXT("VISUAL_EVIDENCE_REQUEST_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	UVisualEvidenceExtensionToolset::CaptureCameraToPng(Camera, Request);

	Request.OutputRelativePath = TEXT("Automation/VisualEvidence/invalid.png");
	Request.CameraRole = TEXT("invalid role");
	AddExpectedError(
		TEXT("VISUAL_EVIDENCE_REQUEST_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	UVisualEvidenceExtensionToolset::CaptureCameraToPng(Camera, Request);

	Request.CameraRole = TEXT("hero");
	Request.WarmupDrawCount = 2;
	AddExpectedError(
		TEXT("VISUAL_EVIDENCE_REQUEST_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	UVisualEvidenceExtensionToolset::CaptureCameraToPng(Camera, Request);

	Request.WarmupDrawCount = 3;
	Request.MaxMeanPixelDelta = 0.6;
	AddExpectedError(
		TEXT("VISUAL_EVIDENCE_REQUEST_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	UVisualEvidenceExtensionToolset::CaptureCameraToPng(Camera, Request);

	Request.MaxMeanPixelDelta = 0.5;
	Request.MinimumWidthPixels = 640;
	AddExpectedError(
		TEXT("VISUAL_EVIDENCE_REQUEST_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	UVisualEvidenceExtensionToolset::CaptureCameraToPng(Camera, Request);

	Request.MinimumWidthPixels = 1280;
	Request.MinimumHeightPixels = 16384;
	AddExpectedError(
		TEXT("VISUAL_EVIDENCE_DIMENSIONS_TOO_SMALL"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	UVisualEvidenceExtensionToolset::CaptureCameraToPng(Camera, Request);

	Request.MinimumHeightPixels = 720;
	AddExpectedError(
		TEXT("VISUAL_EVIDENCE_CAMERA_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	UVisualEvidenceExtensionToolset::CaptureCameraToPng(nullptr, Request);

	FVisualEvidenceVerificationRequest VerifyRequest;
	VerifyRequest.EvidenceRelativePath =
		TEXT("Automation/VisualEvidence/missing.png");
	VerifyRequest.ExpectedCameraRole = TEXT("hero");
	VerifyRequest.ExpectedEvidenceSha256 = FString::ChrN(64, TEXT('A'));
	VerifyRequest.ExpectedReceiptSha256 = FString::ChrN(64, TEXT('B'));
	VerifyRequest.MinimumWidthPixels = 640;
	AddExpectedError(
		TEXT("VISUAL_EVIDENCE_REQUEST_INVALID"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	UVisualEvidenceExtensionToolset::VerifyEvidenceForSupervision(
		Camera,
		VerifyRequest);

	World->DestroyActor(Camera);
	return true;
}

#endif
