"""End-to-end MCP compatibility checks using only a private loopback mock server."""
import json
from pathlib import Path
import shutil
import subprocess
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "Test-UnrealMcpConnection.ps1"
POWERSHELL = shutil.which("powershell.exe") or shutil.which("pwsh")


@unittest.skipUnless(POWERSHELL, "PowerShell is required")
class McpCompatibilityTests(unittest.TestCase):
    def run_mock(self, version="5.9", fault=None, requested=None, advertised=None,
                 required=None, sse=False, both=False):
        name = advertised or "ToolsetRegistry.AgentSkillToolset"
        calls = []
        describe = "describe_toolset" if version == "5.8" else "describe_toolsets"

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_POST(self):
                request = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                calls.append(request)
                method = request["method"]
                result = {}
                error = None
                if method == "initialize":
                    result = {"serverInfo": {"name": "mock", "version": "unrelated-version"}}
                elif method == "tools/list":
                    names = ["list_toolsets", describe, "call_tool"]
                    if both:
                        names.append("describe_toolset")
                    if fault == "unsupported":
                        names.remove(describe)
                    if fault == "missing_call":
                        names.remove("call_tool")
                    result = {"tools": [{"name": n} for n in names]}
                    if fault == "bad_list":
                        result = {"tools": {"name": "describe_toolsets"}}
                    if fault == "rpc_list":
                        error = {"code": -32603, "message": "mock failure"}
                elif method == "tools/call":
                    tool = request["params"]["name"]
                    if tool in ("list_toolsets", "call_tool"):
                        text = "- " + name + ": mock catalog\n"
                        if fault == "bad_catalog":
                            text = "not a catalog"
                    else:
                        description = {"name": name, "tools": [
                            {"name": name + ".Inspect", "inputSchema": {"type": "object"}}
                        ]}
                        if fault == "bad_description":
                            description = {"tools": "not an array"}
                        if fault == "bad_operation":
                            description = {"tools": [{}]}
                        if fault == "foreign_operation":
                            description["tools"] = [{"name": "Other.Toolset.Inspect"}]
                        body = description if version == "5.8" else {
                            "toolsets": {name: description}, "errors": {}}
                        if fault == "per_toolset":
                            body["errors"] = {name: "not found"}
                        if fault == "wrong_name":
                            body["toolsets"] = {"Unrelated.Toolset": description}
                        if fault == "bad_wrapper":
                            body = {"toolsets": [], "errors": {}}
                        text = json.dumps(body) if fault != "bad_json" else "not JSON"
                    result = {"content": [{"type": "text", "text": text}]}
                    if tool == describe and fault == "tool_error":
                        result["isError"] = True
                    if tool == describe and fault == "no_text":
                        result["content"] = []
                    if tool == describe and fault == "rpc_describe":
                        error = {"code": -32602, "message": "mock failure"}
                    if tool == "call_tool" and fault == "bad_dispatch":
                        result = {}
                payload = {"jsonrpc": "2.0", "id": request.get("id")}
                payload["error" if error else "result"] = error or result
                data = json.dumps(payload)
                if sse:
                    data = "event: message\ndata: " + data + "\n\n"
                encoded = data.encode()
                self.send_response(200)
                self.send_header("Mcp-Session-Id", "mock-session")
                self.send_header("Content-Type", "text/event-stream" if sse else "application/json")
                self.send_header("Content-Length", str(len(encoded)))
                self.end_headers()
                self.wfile.write(encoded)

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            command = [POWERSHELL, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                       str(SCRIPT), "-Endpoint", f"http://127.0.0.1:{server.server_port}/mcp"]
            if requested:
                command += ["-ToolsetName", requested]
            if required:
                command += ["-RequiredOperationNames", required]
            process = subprocess.run(command, capture_output=True, text=True, timeout=25)
        finally:
            server.shutdown()
            server.server_close()
            worker.join(timeout=5)
        return process, calls

    def test_58_singular_arguments_and_bare_operation_requirement(self):
        result, calls = self.run_mock("5.8", requested="ToolsetRegistry.AgentSkillToolset", required="Inspect")
        self.assertEqual(result.returncode, 0, result.stderr)
        call = calls[4]["params"]
        self.assertEqual(call["name"], "describe_toolset")
        self.assertEqual(call["arguments"], {"toolset_name": "ToolsetRegistry.AgentSkillToolset"})

    def test_59_plural_array_and_schemas(self):
        result, calls = self.run_mock(required="Inspect")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(calls[4]["params"], {
            "name": "describe_toolsets", "arguments": {
                "toolset_names": ["ToolsetRegistry.AgentSkillToolset"], "include_schemas": True}})
        self.assertEqual(calls[5]["params"]["name"], "call_tool")

    def test_plural_preferred_when_both_advertised(self):
        result, calls = self.run_mock(both=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(calls[4]["params"]["name"], "describe_toolsets")

    def test_sse(self):
        result, _ = self.run_mock(sse=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_current_editor_class_name_is_accepted(self):
        result, calls = self.run_mock(requested="EditorToolset.LevelToolset",
                                      advertised="EditorToolset.LevelToolset", required="Inspect")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(calls[4]["params"]["arguments"]["toolset_names"],
                         ["EditorToolset.LevelToolset"])

    def test_unadvertised_toolset_is_not_guessed(self):
        result, calls = self.run_mock(requested="Unknown.Toolset")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not advertised", result.stderr)
        self.assertEqual(len(calls), 4)

    def test_missing_operation_fails(self):
        result, _ = self.run_mock(requested="ToolsetRegistry.AgentSkillToolset", required="Inspect,Missing")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Required operations are missing", result.stderr)

    def test_same_suffix_in_foreign_namespace_is_rejected(self):
        result, _ = self.run_mock(fault="foreign_operation", required="Inspect")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Required operations are missing", result.stderr)

    def test_dispatch_requires_a_tool_result(self):
        result, _ = self.run_mock(fault="bad_dispatch")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("malformed tool result", result.stderr)

    def test_failures_never_report_success(self):
        cases = {
            "unsupported": "meta-tools are missing",
            "missing_call": "meta-tools are missing",
            "bad_list": "malformed tools array",
            "rpc_list": "JSON-RPC error",
            "bad_catalog": "no recognizable toolset names",
            "bad_description": "malformed toolset description",
            "bad_operation": "operation without a name",
            "per_toolset": "per-toolset errors",
            "wrong_name": "did not return the requested toolset",
            "bad_wrapper": "malformed response wrapper",
            "bad_json": "not JSON",
            "tool_error": "MCP tool error",
            "no_text": "no text result",
            "rpc_describe": "JSON-RPC error",
        }
        for fault, expected in cases.items():
            with self.subTest(fault=fault):
                result, calls = self.run_mock(fault=fault)
                self.assertNotEqual(result.returncode, 0, fault)
                self.assertIn(expected, result.stderr)
                self.assertFalse(any(c.get("params", {}).get("name") == "call_tool" for c in calls))

    def test_58_malformed_description_fails(self):
        result, _ = self.run_mock("5.8", fault="bad_description")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("malformed toolset description", result.stderr)

    def test_script_parses(self):
        command = "$tokens=$null; $errors=$null; " + (
            "[void][System.Management.Automation.Language.Parser]::ParseFile("
            "'" + str(SCRIPT).replace("'", "''") + "',[ref]$tokens,[ref]$errors); "
            "if($errors.Count){$errors | Out-String | Write-Error; exit 1}")
        result = subprocess.run([POWERSHELL, "-NoProfile", "-Command", command],
                                capture_output=True, text=True, timeout=25)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
