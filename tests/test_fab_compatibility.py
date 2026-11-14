"""Static safety/compatibility contracts; no Engine, Fab session or network needed."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Source/UnrealToolsetsExtension"
HEADER = (SOURCE / "Public/FabExtensionToolset.h").read_text(encoding="utf-8")
CPP = (SOURCE / "Private/FabExtensionToolset.cpp").read_text(encoding="utf-8")
TESTS = (SOURCE / "Private/Tests/FabExtensionToolsetTests.cpp").read_text(encoding="utf-8")


class FabCompatibilityTests(unittest.TestCase):
    def test_legacy_enum_and_defaults_remain_stable(self):
        enum = HEADER.split("enum class EFabExtensionLibraryCapabilityStatus : uint8", 1)[1].split("};", 1)[0]
        self.assertEqual(re.findall(r"\b[A-Z]\w+\b", enum), [
            "Available", "RunningCommandlet", "FabPluginUnavailable",
            "UnsupportedFabPluginVersion", "DataStorageUnavailable",
            "PrivateSchemaUnavailable", "SyncCommandUnavailable",
        ])
        for declaration in (
            "bool bAvailable = false;", "bool bUsesPrivateReflectedSchema = true;",
            "bool bRequiresActiveFabLoginSession = true;",
            "bool bCanInitiateDownloads = false;",
            "bool bRequiresManualFabWindowDownloadStart = true;",
            "bool bCanObserveCompletedZipCacheEntries = true;",
        ):
            self.assertIn(declaration, HEADER)

    def test_original_exact_version_gate_precedes_private_access(self):
        self.assertIn("constexpr int32 SupportedFabPluginVersion = 14;", CPP)
        build = CPP.split("FFabExtensionLibraryCapability BuildFabLibraryCapability()", 1)[1].split("bool GetFabLibraryRows(", 1)[0]
        gate = build.index("EngineVersion.GetMajor() != 5")
        self.assertIn("EngineVersion.GetMinor() != 8", build)
        self.assertIn("Result.FabVersion != SupportedFabPluginVersion", build)
        failure = build.index("EFabExtensionLibraryCapabilityStatus::UnsupportedFabPluginVersion", gate)
        returned = build.index("return Result;", failure)
        for access in ("GetMutableDataStorageFeature", "GetFabLibrarySchema()", "GetFabSyncCommand()"):
            self.assertGreater(build.index(access), returned)
        self.assertNotIn("Result.bAvailable = true", build[:returned])

    def test_official_discovery_is_passive_and_optional(self):
        discovery = CPP.split("void InspectOfficialFabTools(", 1)[1].split("FFabExtensionLibraryCapability BuildFabLibraryCapability()", 1)[0]
        for required in (
            '/Script/FabToolset.FabToolset', 'IsModuleLoaded(TEXT("FabToolset"))',
            "FindObject<UClass>", "FindFunctionByName", "FUNC_Static",
            "WITH_METADATA", 'TEXT("AICallable")', 'TEXT("AIIgnore")',
            "UToolsetDefinition::StaticClass()", "OfficialClass->GetName()",
            'TEXT("FabToolset.") + OfficialClass->GetName()',
        ):
            self.assertIn(required, discovery)
        for function in ("SearchFabMyLibrary", "SearchFabListings", "GetFabListingDetails"):
            self.assertIn(f'TEXT("{function}")', discovery)
        for forbidden in (
            "LoadModule", "LoadObject", "ProcessEvent", "ExecuteTool", "GetDefaultObject",
            "FabSignIn", "InstallFabListing", "GetMutableDataStorageFeature",
            "GetFabSyncCommand", "bAvailable =", "Status =", "Http",
        ):
            self.assertNotIn(forbidden, discovery)
        self.assertNotIn('#include "FabToolset.h"', CPP)
        self.assertNotIn('#include "IFabModule.h"', CPP)

    def test_existing_operations_keep_capability_checks_and_error_code(self):
        for function in ("RequestFabOwnedLibrarySync", "GetFabOwnedLibrarySnapshotInfo", "SearchFabOwnedAssets"):
            body = CPP.split(f"UFabExtensionToolset::{function}(", 1)[1]
            body = body.split("\n}\n", 1)[0]
            self.assertIn("if (!Capability.bAvailable)", body)
            self.assertIn('TEXT("FAB_EXTENSION_LIBRARY_UNAVAILABLE")', body)
            self.assertIn("FabToolset.FabToolset.SearchFabMyLibrary", body)
            self.assertLess(body.index("if (!Capability.bAvailable)"), body.index("ICoreProvider* Storage"))

    def test_explicit_access_modes_preserve_nine_operations(self):
        self.assertEqual(HEADER.count("UFUNCTION("), 9)
        self.assertEqual(HEADER.count('AIAccessMode = "Read"'), 8)
        self.assertEqual(HEADER.count('AIAccessMode = "Write"'), 1)
        self.assertRegex(HEADER, r'AIAccessMode = "Write"[^\n]*\n\s*static FFabExtensionLibrarySyncReceipt RequestFabOwnedLibrarySync')

    def test_automation_discovery_is_offline_and_version_aware(self):
        body = TESTS.split("bool FFabExtensionCompatibilityTest::RunTest", 1)[1].split("\n}\n", 1)[0]
        self.assertIn("Integration.Version == 14", body)
        self.assertIn("EngineVersion.GetMinor() == 8", body)
        self.assertIn("UnsupportedFabPluginVersion", body)
        self.assertIn("OfficialReflectedToolNames.Contains", body)
        self.assertIn("ExpectedCount", body)
        for call in ("RequestFabOwnedLibrarySync(", "SearchFabOwnedAssets(", "ExecuteTool(", "ProcessEvent("):
            self.assertNotIn(call, body)
        self.assertNotIn('LibraryCapability.FabVersion, 14)', TESTS)
        self.assertIn('TEXT("RunFabOwnedLibraryLiveTest")', TESTS)


if __name__ == "__main__":
    unittest.main()
