import json
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_ROOT = REPO_ROOT


class UnrealExtensionDistributionTests(unittest.TestCase):
    def test_descriptor_is_editor_only_and_independent_of_all_toolsets(self):
        descriptor = json.loads(
            (PLUGIN_ROOT / "CommunityToolsetsExtension.uplugin").read_text(encoding="utf-8")
        )
        self.assertTrue(descriptor["EditorOnly"])
        self.assertEqual("CommunityToolsetsExtension", descriptor["Modules"][0]["Name"])
        dependencies = {item["Name"] for item in descriptor["Plugins"]}
        self.assertIn("ToolsetRegistry", dependencies)
        self.assertNotIn("AllToolsets", dependencies)
        self.assertNotIn("ModelContextProtocol", dependencies)

    def test_module_registers_domain_toolset(self):
        module = (
            PLUGIN_ROOT
            / "Source"
            / "CommunityToolsetsExtension"
            / "Private"
            / "CommunityToolsetsExtensionModule.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn(
            "RegisterToolsetClass(ULandscapeExtensionToolset::StaticClass())", module
        )
        self.assertIn(
            "UnregisterToolsetClass(ULandscapeExtensionToolset::StaticClass())", module
        )

    def test_public_contract_contains_nine_typed_tools(self):
        header = (
            PLUGIN_ROOT
            / "Source"
            / "CommunityToolsetsExtension"
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
            "SaveLandscape",
        ):
            self.assertIn(f"{tool_name}(", header)
        self.assertEqual(9, header.count("meta = (AICallable)"))

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
            PLUGIN_ROOT / "CommunityToolsetsExtension.uplugin"
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
            "CommunityToolsetsExtension-$pluginVersion-$safeEngineLabel-$safePlatforms",
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
            "unreal-community-toolsets-extension",
            "community-toolsets-v0.1.0",
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
            / "release-community-toolsets.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("pull_request:", validation)
        self.assertIn("runs-on: windows-latest", validation)
        self.assertNotIn("self-hosted", validation)
        for required in (
            "workflow_dispatch:",
            "startsWith(github.ref, 'refs/tags/community-toolsets-v')",
            "self-hosted",
            "environment: community-toolsets-release",
            "UE_5_8_ROOT",
            "--draft",
            "--verify-tag",
        ):
            self.assertIn(required, release)
        self.assertNotIn("pull_request:", release)


if __name__ == "__main__":
    unittest.main()
