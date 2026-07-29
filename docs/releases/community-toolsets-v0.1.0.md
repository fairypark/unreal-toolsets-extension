# Community Toolsets Extension 0.1.0

This is the first client-neutral release of the separately installed Unreal
Editor plugin that fills verified gaps in Epic's typed ToolsetRegistry APIs.
It does not modify or replace `AllToolsets`, `ToolsetRegistry`, or any Engine
file.

## Included toolset

`LandscapeExtensionToolset` provides:

- `GetLandscapeInfo`
- `CreateLandscape`
- `ImportHeightmap`
- `ReimportHeightmap`
- `ImportWeightmap`
- `SetLandscapeMaterial`
- `GetHeightStatistics`
- `ValidateBuildingPad`
- `SaveLandscape`

## Compatibility

- Verified Engine: Unreal Engine 5.8.1
- Verified platform: Win64 Editor
- Required Engine plugin: `ToolsetRegistry`
- Optional transport plugin: `ModelContextProtocol`
- Compatible clients: any MCP client that can connect to Unreal's MCP server,
  including Claude and Codex

Other Engine versions and platforms are unverified until a matching release
asset passes the complete build and runtime matrix.

## Safety

Landscape mutations use explicit targets and edit layers, validate complete
source dimensions, save recovery packages, reject unsafe partial World
Partition writes, verify written height data and representative samples, and
attempt in-memory rollback after post-write failures.

`CreateLandscape` does not yet create World Partition streaming proxies and
therefore rejects partitioned worlds. No runtime tool drives Slate UI.

## Validation

- Unreal `BuildPlugin`: passed for UE 5.8 Win64
- Disposable-project plugin loading: passed
- `AI.LandscapeExtensionToolset.Contract`: passed
- Repository unit tests: passed
- Standalone repository tests and release-script validation: passed

Live MCP discovery and mutating Landscape scenarios must be completed before
the draft release is promoted to a final public release.
