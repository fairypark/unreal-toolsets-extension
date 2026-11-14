"""Source-order regression checks; live framebuffer proof still needs Unreal Editor."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Source/UnrealToolsetsExtension/Private"
CPP = (SOURCE / "VisualEvidenceExtensionToolset.cpp").read_text(encoding="utf-8")
TESTS = (SOURCE / "Tests/VisualEvidenceExtensionToolsetTests.cpp").read_text(encoding="utf-8")
CAPTURE = CPP.split("UVisualEvidenceExtensionToolset::CaptureCameraToPng(", 1)[1]


class VisualCompatibilityTests(unittest.TestCase):
    def test_capture_fixture_yields_for_lazy_compilation_before_capture(self):
        fixture = TESTS.split("bool FVisualEvidenceExtensionCaptureAndVerifyTest::RunTest", 1)[1]
        fixture = fixture.split("IMPLEMENT_SIMPLE_AUTOMATION_TEST(", 1)[0]
        latent = fixture.index("ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(")
        prewarm = fixture.index("PrewarmClient->Viewport->Draw();", latent)
        readiness = fixture.index("const bool bCompilationPending", prewarm)
        capture = fixture.index("UVisualEvidenceExtensionToolset::CaptureCameraToPng(", readiness)
        self.assertLess(latent, prewarm)
        self.assertLess(prewarm, readiness)
        self.assertLess(readiness, capture)
        waiting = fixture[latent:capture]
        for expression in (
            "FPlatformTime::Seconds() - StartedAt > 120.0",
            "FAssetCompilingManager::Get().GetNumRemainingAssets() != 0",
            "GShaderCompilingManager->IsCompiling()",
            "ReadyDraws = bCompilationPending ? 0 : ReadyDraws + 1;",
            "if (ReadyDraws < 3)", "return false;",
            "PrewarmClient->SetActorLock(SavedLock)",
            "PrewarmClient->SetViewLocation(SavedLocation)",
            "PrewarmClient->SetViewRotation(SavedRotation)",
            "PrewarmClient->ViewFOV = SavedFOV",
        ):
            self.assertIn(expression, waiting)
        # The viewport fixture outlives RunTest and restores when the command ends.
        self.assertIn("MakeShared<FScopedLargeAutomationViewport>()", fixture)
        self.assertIn("[this, World, Camera, LargeViewport,", fixture)
        self.assertEqual(fixture.count("return false;", latent), 1)
        # No speculative change to production streaming order or timing.
        self.assertEqual(CAPTURE.count("StreamAllResources("), 1)
        self.assertLess(CAPTURE.index("StreamAllResources(5.0f)"),
                        CAPTURE.index("ViewportClient->SetActorLock(CameraActor)"))

    def test_acceptance_still_fails_closed(self):
        self.assertIn("DrawIndex <= Request.MaxStabilizationDrawCount", CAPTURE)
        self.assertIn("Receipt.FinalMeanPixelDelta <= Request.MaxMeanPixelDelta", CAPTURE)
        self.assertIn("Receipt.StableFramePairCount < 2 || AcceptedBitmap.IsEmpty()", CAPTURE)
        accepted = CAPTURE.index("AcceptedBitmap = PreviousBitmap;")
        encoding = CAPTURE.index("FImageUtils::PNGCompressImageArray(", accepted)
        guard = CAPTURE[accepted:encoding]
        for expression in (
            "FAssetCompilingManager::Get().GetNumRemainingAssets()",
            "GShaderCompilingManager->IsCompiling()",
            "IStreamingManager::Get().GetNumWantingResources()",
            "Receipt.RemainingAssetCompileCount != 0",
            "Receipt.bShaderCompilationActive ||",
            "Receipt.RemainingStreamingRequests != 0",
            "VISUAL_EVIDENCE_RENDER_STATE_UNSETTLED",
            "return {};",
        ):
            self.assertIn(expression, guard)
        # Camera aspect validation is production policy, not a fixture workaround.
        self.assertIn("if (CameraComponent->bConstrainAspectRatio)", CAPTURE)
        self.assertIn("Constrained camera aspect ratio", CAPTURE)

    def test_validation_fixture_is_independent_of_host_aspect_ratio(self):
        validation = TESTS.split("bool FVisualEvidenceExtensionValidationTest::RunTest", 1)[1]
        configure = validation.index("Camera->GetCameraComponent()->bConstrainAspectRatio = false;")
        first_request = validation.index("UVisualEvidenceExtensionToolset::CaptureCameraToPng(Camera, Request)")
        self.assertLess(configure, first_request)
        self.assertIn('TEXT("VISUAL_EVIDENCE_DIMENSIONS_TOO_SMALL")', validation)
        self.assertIn('TEXT("VISUAL_EVIDENCE_REQUEST_INVALID")', validation)


if __name__ == "__main__":
    unittest.main()
