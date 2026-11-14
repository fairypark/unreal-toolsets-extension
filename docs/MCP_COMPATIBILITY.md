# MCP connection compatibility

`Test-UnrealMcpConnection.ps1` selects the describe operation from the actual
`tools/list` response, never from a reported engine/server version:

- UE 5.8: `describe_toolset` with `toolset_name` returns one descriptor.
- Installed UE 5.9 preview: `describe_toolsets` with `toolset_names` (an array)
  and `include_schemas: true` returns
  `{ "toolsets": { "qualified.name": descriptor }, "errors": {} }`.
- If both operations are advertised, the plural form is preferred.
- Missing capabilities, malformed descriptions, JSON-RPC/tool errors, nonempty
  per-toolset errors, and absent requested operations fail the check.

Both installed engines return a plain-text `list_toolsets` catalog, with one
`- qualified.name: description` entry per toolset. Pass the exact advertised
name using `-ToolsetName`. The default remains
`ToolsetRegistry.AgentSkillToolset`. The check rejects an unadvertised name
instead of silently switching to a different toolset.

## Editor toolsets

UE 5.8's Editor toolsets include Python classes `ActorTools`, `AssetTools`, and
`SceneTools` under `EditorToolset/Content/Python/editor_toolset/toolsets`.
The installed UE 5.9 preview registers native classes such as
`EditorToolset.ActorToolset`, `EditorToolset.AssetToolset`, and
`EditorToolset.LevelToolset`. Discover these names from the running editor.
Do not assume that a renamed class preserves legacy operation names or
argument schemas; inspect the current descriptor first. The connection check
does not translate operations or invoke a scene/asset mutation.

Example, after confirming the name in the running editor's catalog:

```powershell
.\scripts\Test-UnrealMcpConnection.ps1 `
    -Endpoint http://localhost:8000/mcp `
    -ToolsetName EditorToolset.LevelToolset
```

## Evidence and scope

The implementation was checked against installed engine source:

- UE 5.8: `ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolSearch.{h,cpp}`
- UE 5.9: `ModelContextProtocol/Source/ModelContextProtocol/Private/ModelContextProtocolToolSearch.{h,cpp}`
- Both: `ModelContextProtocolToolsetRegistryAdapter.cpp`, especially
  `GetToolsetCatalogText` and the describe delegates.
- UE 5.9: `EditorToolset/Source/EditorToolset/Private/EditorToolsetModule.cpp`.

These are installed-preview API observations, not a claim of compatibility
with every future UE 5.9 release. Build and live editor validation are separate
from the deterministic script tests.

## Tests

```powershell
python -m unittest discover -s tests -p test_mcp_compatibility.py -v
```

The suite runs the real PowerShell script against a temporary loopback HTTP
mock on an operating-system-assigned port. It never contacts the normal MCP
port or a running Unreal Editor. It checks singular/plural request shapes,
schema inclusion, SSE, exact qualified names, required operations, failure
handling, and the PowerShell parser. Windows PowerShell is preferred when
available so its older JSON behavior is covered; PowerShell 7 is a fallback.
