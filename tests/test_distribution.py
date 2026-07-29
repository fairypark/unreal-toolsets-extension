import json
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_ROOT = REPO_ROOT


class UnrealExtensionDistributionTests(unittest.TestCase):
    def test_descriptor_is_editor_only_and_independent_of_all_toolsets(self):
        descriptor = json.loads(
            (PLUGIN_ROOT / "UnrealToolsetsExtension.uplugin").read_text(encoding="utf-8")
        )
        self.assertTrue(descriptor["EditorOnly"])
        self.assertEqual("UnrealToolsetsExtension", descriptor["Modules"][0]["Name"])
        dependencies = {item["Name"] for item in descriptor["Plugins"]}
        self.assertIn("ToolsetRegistry", dependencies)
        self.assertIn("PCG", dependencies)
        self.assertIn("Fab", dependencies)
        self.assertNotIn("AllToolsets", dependencies)
        self.assertNotIn("ModelContextProtocol", dependencies)

    def test_module_registers_domain_toolsets(self):
        module = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealToolsetsExtension"
            / "Private"
            / "UnrealToolsetsExtensionModule.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn(
            "RegisterToolsetClass(ULandscapeExtensionToolset::StaticClass())", module
        )
        self.assertIn(
            "UnregisterToolsetClass(ULandscapeExtensionToolset::StaticClass())", module
        )
        self.assertIn(
            "RegisterToolsetClass(UPlacementExtensionToolset::StaticClass())", module
        )
        self.assertIn(
            "UnregisterToolsetClass(UPlacementExtensionToolset::StaticClass())", module
        )
        self.assertIn(
            "RegisterToolsetClass(UPCGExtensionToolset::StaticClass())", module
        )
        self.assertIn(
            "UnregisterToolsetClass(UPCGExtensionToolset::StaticClass())", module
        )
        self.assertIn(
            "RegisterToolsetClass(ULandscapeGrassExtensionToolset::StaticClass())",
            module,
        )
        self.assertIn(
            "UnregisterToolsetClass(ULandscapeGrassExtensionToolset::StaticClass())",
            module,
        )
        self.assertIn(
            "RegisterToolsetClass(UFabExtensionToolset::StaticClass())", module
        )
        self.assertIn(
            "UnregisterToolsetClass(UFabExtensionToolset::StaticClass())", module
        )

    def test_public_contract_contains_typed_p0_tools(self):
        header = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealToolsetsExtension"
            / "Public"
            / "LandscapeExtensionToolset.h"
        ).read_text(encoding="utf-8")
        for tool_name in (
            "GetLandscapeInfo",
            "CreateLandscape",
            "ImportHeightmap",
            "ReimportHeightmap",
            "ImportWeightmap",
            "SetLandscapeMaterial",
            "GetHeightStatistics",
            "ValidateBuildingPad",
            "PreflightReimportLandscapeData",
            "ReimportLandscapeData",
            "AuditStreamingProxies",
            "RebuildStreamingProxies",
            "SaveLandscape",
        ):
            self.assertIn(f"{tool_name}(", header)
        self.assertEqual(13, header.count("meta = (AICallable)"))

        placement_header = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealToolsetsExtension"
            / "Public"
            / "PlacementExtensionToolset.h"
        ).read_text(encoding="utf-8")
        self.assertIn("SnapActorToGround(", placement_header)
        self.assertEqual(1, placement_header.count("meta = (AICallable)"))

    def test_snap_to_ground_contract_is_diagnostic_and_self_safe(self):
        header = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealToolsetsExtension"
            / "Public"
            / "PlacementExtensionToolset.h"
        ).read_text(encoding="utf-8")
        implementation = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealToolsetsExtension"
            / "Private"
            / "PlacementExtensionToolset.cpp"
        ).read_text(encoding="utf-8")
        for required in (
            "TraceChannel",
            "bPreferLandscape",
            "bLandscapeOnly",
            "bUseActorBoundsBottom",
            "HitActor",
            "HitLocation",
            "HitNormal",
            "AppliedDeltaZCentimeters",
        ):
            self.assertIn(required, header)
        self.assertIn("QueryParams.AddIgnoredActor(Actor)", implementation)
        self.assertIn("PLACEMENT_EXTENSION_NO_GROUND_HIT", implementation)
        self.assertNotIn("8000.0", implementation)

    def test_pcg_extension_has_structured_weighted_mesh_crud_and_audit(self):
        header = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealToolsetsExtension"
            / "Public"
            / "PCGExtensionToolset.h"
        ).read_text(encoding="utf-8")
        implementation = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealToolsetsExtension"
            / "Private"
            / "PCGExtensionToolset.cpp"
        ).read_text(encoding="utf-8")
        for function_name in (
            "GetWeightedMeshEntries",
            "ReplaceWeightedMeshEntries",
            "AddWeightedMeshEntry",
            "UpdateWeightedMeshEntry",
            "RemoveWeightedMeshEntry",
            "ReorderWeightedMeshEntry",
            "AuditGraphInstance",
        ):
            self.assertIn(f"{function_name}(", header)
        self.assertEqual(7, header.count("meta = (AICallable)"))
        self.assertIn("UPCGMeshSelectorWeighted", implementation)
        self.assertIn("ForEachConstManagedResource", implementation)
        self.assertIn("GetHeightAtLocation", implementation)

    def test_landscape_grass_extension_has_structured_variety_crud(self):
        header = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealToolsetsExtension"
            / "Public"
            / "LandscapeGrassExtensionToolset.h"
        ).read_text(encoding="utf-8")
        implementation = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealToolsetsExtension"
            / "Private"
            / "LandscapeGrassExtensionToolset.cpp"
        ).read_text(encoding="utf-8")
        for function_name in (
            "GetGrassVarieties",
            "ReplaceGrassVarieties",
            "AddGrassVariety",
            "UpdateGrassVariety",
            "RemoveGrassVariety",
            "ReorderGrassVariety",
            "SetGrassDensityScaling",
        ):
            self.assertIn(f"{function_name}(", header)
        self.assertEqual(7, header.count("meta = (AICallable)"))
        self.assertIn("FScopedTransaction", implementation)
        self.assertIn("MarkPackageDirty", implementation)
        self.assertNotIn("SavePackage", implementation)

    def test_fab_extension_is_read_only_and_uses_public_surfaces(self):
        header = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealToolsetsExtension"
            / "Public"
            / "FabExtensionToolset.h"
        ).read_text(encoding="utf-8")
        implementation = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealToolsetsExtension"
            / "Private"
            / "FabExtensionToolset.cpp"
        ).read_text(encoding="utf-8")
        for function_name in (
            "GetFabIntegrationInfo",
            "GetFabCacheSummary",
            "ListFabCachedEntries",
            "GetFabCacheEntryStatus",
            "GetFabImportTypeSupport",
        ):
            self.assertIn(f"{function_name}(", header)
        self.assertEqual(5, header.count("meta = (AICallable)"))
        for public_surface in (
            "Utilities/FabAssetsCache.h",
            "FabWorkflowFactoryRegistry.h",
            "IPluginManager",
        ):
            self.assertIn(public_surface, implementation)
        for forbidden in (
            "FabAuthentication",
            "FabBrowserApi",
            "FFabDownloadRequest",
            "ClearCache(",
            "DeleteDirectory",
            "Http",
            "Cookie",
            "Token",
        ):
            self.assertNotIn(forbidden, implementation)

    def test_distribution_has_no_personal_project_constants(self):
        scanned = []
        for path in PLUGIN_ROOT.rglob("*"):
            if path.is_file() and path.suffix.lower() in {
                ".h",
                ".cpp",
                ".cs",
                ".md",
                ".uplugin",
            }:
                scanned.append(path.read_text(encoding="utf-8"))
        contents = "\n".join(scanned)
        for forbidden in (
            "PrivateProjectName",
            "C:\\Users\\ExampleUser",
            "BP_",
            "/Game/",
            "127.0.0.1",
            "CodexToolsetsExtension",
            "CodexLandscapeToolset",
            "CODEX_LANDSCAPE",
        ):
            self.assertNotIn(forbidden, contents)

    def test_runtime_brand_is_mcp_client_neutral(self):
        descriptor = (
            PLUGIN_ROOT / "UnrealToolsetsExtension.uplugin"
        ).read_text(encoding="utf-8")
        readme = (PLUGIN_ROOT / "README.md").read_text(encoding="utf-8")
        self.assertNotIn("Codex", descriptor)
        self.assertIn("MCP-client neutral", readme)
        self.assertIn("Claude", readme)

    def test_packaging_script_removes_generated_intermediate_files(self):
        script = (REPO_ROOT / "scripts" / "package-unreal-extension.ps1").read_text(
            encoding="utf-8"
        )
        self.assertIn('Join-Path $resolvedOutput "Intermediate"', script)
        self.assertIn(
            "Refusing to clean an Intermediate directory outside the package", script
        )
        self.assertIn(
            "Remove-Item -LiteralPath $intermediateDirectory -Recurse -Force", script
        )

    def test_github_release_assets_are_commit_and_engine_specific(self):
        script = (
            REPO_ROOT / "scripts" / "build-unreal-extension-github-release.ps1"
        ).read_text(encoding="utf-8")
        for required in (
            'ValidatePattern("^[0-9a-fA-F]{40}$")',
            "UnrealToolsetsExtension-$pluginVersion-$safeEngineLabel-$safePlatforms",
            "Get-FileHash -LiteralPath $assetPath -Algorithm SHA256",
            '"release-manifest.json"',
            "sourceCommit = $SourceCommit.ToLowerInvariant()",
            "engineVersion = $EngineVersionLabel",
            "SourceCommit does not match repository HEAD",
            "Refusing to build GitHub Release assets from a dirty working tree",
        ):
            self.assertIn(required, script)

    def test_github_distribution_is_client_neutral_and_release_gated(self):
        guide = (REPO_ROOT / "docs" / "GITHUB_RELEASES.md").read_text(
            encoding="utf-8"
        )
        for required in (
            "unreal-toolsets-extension",
            "unreal-toolsets-v0.1.0",
            "Claude, Codex, and other MCP users",
            "protected tags",
            "Never run untrusted pull-request code",
            "live Unreal MCP server",
        ):
            self.assertIn(required, guide)
        self.assertNotIn("CodexToolsetsExtension", guide)

    def test_github_workflows_separate_untrusted_validation_from_release_runner(self):
        validation = (
            REPO_ROOT / ".github" / "workflows" / "validate.yml"
        ).read_text(encoding="utf-8")
        release = (
            REPO_ROOT
            / ".github"
            / "workflows"
            / "release-unreal-toolsets.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("pull_request:", validation)
        self.assertIn("runs-on: windows-latest", validation)
        self.assertNotIn("self-hosted", validation)
        for required in (
            "workflow_dispatch:",
            "startsWith(github.ref, 'refs/tags/unreal-toolsets-v')",
            "self-hosted",
            "environment: unreal-toolsets-release",
            "UE_5_8_ROOT",
            "--draft",
            "--verify-tag",
        ):
            self.assertIn(required, release)
        self.assertNotIn("pull_request:", release)


if __name__ == "__main__":
    unittest.main()
