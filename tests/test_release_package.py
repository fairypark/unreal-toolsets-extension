import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[1]
PS = shutil.which("powershell.exe") or shutil.which("pwsh")


@unittest.skipUnless(PS, "PowerShell is required")
class ReleasePackageTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.repo = self.root / "repo"
        scripts = self.repo / "scripts"
        scripts.mkdir(parents=True)
        self.builder = scripts / "build-unreal-extension-github-release.ps1"
        shutil.copyfile(ROOT / "scripts" / self.builder.name, self.builder)
        shutil.copyfile(ROOT / "UnrealToolsetsExtension.uplugin", self.repo / "UnrealToolsetsExtension.uplugin")
        (scripts / "package-unreal-extension.ps1").write_text(
            'param($EngineRoot, $OutputDirectory, $TargetPlatforms)\n'
            '$bin = Join-Path $OutputDirectory "Binaries/Win64"\n'
            'New-Item -ItemType Directory -Path $bin -Force | Out-Null\n'
            'Set-Content (Join-Path $bin "UnrealEditor.modules") '
            '\'{"Modules":{"UnrealToolsetsExtension":"test.dll"}}\'\n'
            'Set-Content (Join-Path $bin "test.dll") "sample"\n',
            encoding="utf-8",
        )
        def git(*args):
            return subprocess.check_output(["git", *args], cwd=self.repo, text=True).strip()
        git("init", "-q")
        git("add", "--", "scripts", "UnrealToolsetsExtension.uplugin")
        git("-c", "user.name=Release Test", "-c", "user.email=test@example.invalid",
            "-c", "commit.gpgsign=false", "commit", "-qm", "fixture")
        self.commit = git("rev-parse", "HEAD")
        self.engine = self.root / "engine"
        build_dir = self.engine / "Engine/Build"
        build_dir.mkdir(parents=True)
        (build_dir / "Build.version").write_text(json.dumps({
            "MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 4, "Changelist": 123,
        }), encoding="utf-8")
        self.output = self.root / "out"

    def build(self, *args):
        return subprocess.run(
            [PS, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(self.builder),
             "-EngineRoot", str(self.engine), "-OutputDirectory", str(self.output),
             "-SourceCommit", self.commit, *args], capture_output=True, text=True, timeout=30,
        )

    def test_archive_matches_engine_revision_and_checksum(self):
        result = self.build()
        self.assertEqual(0, result.returncode, result.stderr)
        archive, = self.output.glob("*.zip")
        self.assertIn("UE5.8.4-CL123-Win64", archive.name)
        manifest = json.loads(archive.with_suffix(".manifest.json").read_text())
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        self.assertEqual(manifest["assets"], [{"file": archive.name, "sha256": digest}])
        self.assertEqual(manifest["sourceCommit"], self.commit)
        self.assertEqual(manifest["engineChangelist"], 123)
        self.assertEqual(manifest["targetPlatforms"], ["Win64"])
        self.assertEqual(archive.with_suffix(".zip.sha256").read_text().strip(), f"{digest}  {archive.name}")
        with zipfile.ZipFile(archive) as package:
            self.assertIn("UnrealToolsetsExtension/Binaries/Win64/test.dll", package.namelist())

    def test_stale_engine_label_stops_before_packaging(self):
        result = self.build("-EngineVersionLabel", "UE5.8.1")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("EngineVersionLabel must match", result.stderr)
        self.assertFalse(self.output.exists())

    def test_missing_platform_cannot_be_advertised(self):
        result = self.build("-TargetPlatforms", "Win64+Linux")
        self.assertNotEqual(0, result.returncode)
        self.assertFalse(self.output.exists())
        self.assertFalse(self.output.with_suffix(".staging").exists())
