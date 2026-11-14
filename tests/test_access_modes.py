import json
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


class AccessModeTests(unittest.TestCase):
    def test_all_access_modes_match_catalog_semantics(self):
        catalog = json.loads((ROOT / "Resources/UsefulnessCatalog.json").read_text())
        expected = {
            (operation["toolset"], operation["operation"]):
                "Read" if operation["class"] in {"read", "validation", "discovery"} else "Write"
            for operation in catalog["operations"]
        }
        actual = {}
        for header in (ROOT / "Source/UnrealToolsetsExtension/Public").glob("*ExtensionToolset.h"):
            for match in re.finditer(
                r'UFUNCTION\(meta = \(AICallable, AIAccessMode = "(Read|Write)"\), '
                r'Category = "([^"]+)"\)\s+static\s+[\w:<>,*&\s]+?\s+(\w+)\s*\(',
                header.read_text(encoding="utf-8"),
            ):
                mode, toolset, operation = match.groups()
                self.assertNotIn((toolset, operation), actual)
                actual[(toolset, operation)] = mode
        self.assertEqual(actual, expected)
        # These operations can write even when they also offer a dry-run path.
        self.assertEqual(actual[("LandscapeExtensionToolset", "RebuildStreamingProxies")], "Write")
        self.assertEqual(actual[("LandscapeExtensionToolset", "ReimportLandscapeData")], "Write")
        self.assertEqual(actual[("VisualEvidenceExtensionToolset", "CaptureCameraToPng")], "Write")


if __name__ == "__main__":
    unittest.main()
