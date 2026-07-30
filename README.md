# Unreal Toolsets Extension

[![Validate repository](https://github.com/fairypark/unreal-toolsets-extension/actions/workflows/validate.yml/badge.svg)](https://github.com/fairypark/unreal-toolsets-extension/actions/workflows/validate.yml)

`UnrealToolsetsExtension` is an independent, unofficial, distributable Unreal
Editor plugin for typed
ToolsetRegistry capabilities that are missing from the official Unreal toolsets.
It complements Epic plugins without modifying or replacing them.

The plugin is MCP-client neutral. Once Unreal's `ModelContextProtocol` plugin
discovers the registered toolsets, Claude, Codex, and any other compatible MCP
client can call the same typed operations. No runtime API is tied to a Codex
installation.

`LandscapeExtensionToolset` exposes:

- `GetLandscapeInfo`
- `CreateLandscape`
- `ImportHeightmap`
- `ReimportHeightmap`
- `PreflightReimportLandscapeData`
- `ReimportLandscapeData`
- `ImportWeightmap`
- `SetLandscapeMaterial`
- `GetHeightStatistics`
- `ValidateBuildingPad`
- `AuditStreamingProxies`
- `RebuildStreamingProxies`
- `SaveLandscape`

`PlacementExtensionToolset` exposes:

- `SnapActorToGround`

`PCGExtensionToolset` complements the official generic PCG node tools with:

- `GetWeightedMeshEntries`
- `ReplaceWeightedMeshEntries`
- `AddWeightedMeshEntry`
- `UpdateWeightedMeshEntry`
- `RemoveWeightedMeshEntry`
- `ReorderWeightedMeshEntry`
- `AuditGraphInstance`

`LandscapeGrassExtensionToolset` provides typed `LandscapeGrassType` editing:

- `GetGrassVarieties`
- `ReplaceGrassVarieties`
- `AddGrassVariety`
- `UpdateGrassVariety`
- `RemoveGrassVariety`
- `ReorderGrassVariety`
- `SetGrassDensityScaling`

`FabExtensionToolset` provides guarded, read-only inspection through Fab's
public C++ interfaces:

- `GetFabIntegrationInfo`
- `GetFabCacheSummary`
- `ListFabCachedEntries`
- `GetFabCacheEntryStatus`
- `GetFabImportTypeSupport`

`ObservabilityExtensionToolset` exposes client-neutral, read-only metadata:

- `GetExtensionObservabilityInfo`

It returns the extension and Engine versions plus a versioned catalog of exact
operation classes, postcondition and persistence semantics, and allowlisted
stable error codes. It does not enable collection or store usage data.

## Requirements

- Unreal Engine 5.8 for version `0.2.0`
- the experimental `ToolsetRegistry` plugin
- the built-in `PCG` plugin
- the built-in `Fab` plugin
- an Editor target

Win64 packaging was verified with Unreal Engine 5.8. Other Engine versions and
platforms require their own clean build and live test before being declared
supported.

`AllToolsets` and `ModelContextProtocol` are not build dependencies. A project
needs `ModelContextProtocol` only when it wants to expose registered toolsets
over MCP.

## Build a distributable package

From the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\package-unreal-extension.ps1 `
  -EngineRoot "C:\Program Files\Epic Games\UE_5.8" `
  -OutputDirectory "C:\short-path\UnrealToolsetsExtension"
```

Use a short output path on Windows because Unreal intermediate paths can exceed
the legacy 260-character limit. The packaging script refuses to overwrite an
existing output directory. `-ExecutionPolicy Bypass` applies only to this
process; it does not change the machine or user policy.

## Install

For normal installation, download the ZIP matching the user's exact Unreal
Engine version and platform from GitHub Releases. Verify it against the
published `.sha256` file before extraction. Do not use a binary built for a
different Engine version.

1. Copy the packaged `UnrealToolsetsExtension` folder to:

   ```text
   <Project>/Plugins/UnrealToolsetsExtension
   ```

2. Enable `UnrealToolsetsExtension` in the project. Do not edit
   `Engine/Plugins/Experimental/Toolsets/AllToolsets/AllToolsets.uplugin`.
3. Restart the Editor after adding a packaged binary plugin.
4. When using MCP, start the server and run:

   ```text
   ModelContextProtocol.RefreshTools
   ```

5. Confirm `list_toolsets` contains `LandscapeExtensionToolset`,
   `LandscapeGrassExtensionToolset`, `PlacementExtensionToolset`,
   `PCGExtensionToolset`, `FabExtensionToolset`, and
   `ObservabilityExtensionToolset`.

## Safety and current limits

- Landscape mutations require an explicit editable Landscape edit-layer GUID.
- Heightmap and weightmap imports require an exact full-Landscape resolution.
- Mutations refuse regions with unloaded World Partition components.
- `CreateLandscape` currently refuses World Partition worlds because version
  `0.2.0` does not create streaming proxies.
- Reimport validates the recorded source, saves a recovery point, captures the
  original height data, verifies the complete write plus representative points,
  and rolls back on failure.
- Batch reimport supports preflight and dry-run validation for one RAW16
  heightmap plus RAW8 Landscape layer maps while preserving transform,
  material, component layout, and loaded streaming-proxy layout.
- Streaming-proxy audit is read-only. Rebuild requires a matching prior audit,
  a dry run, an explicit confirmation token, complete loaded coverage, and a
  durable recovery save.
- Ground snapping ignores the moved Actor and its attachments, supports
  caller-selected trace channels and negative world heights, can prefer or
  require Landscape hits, aligns the Actor bounds bottom, and reports the
  selected hit and applied delta.
- PCG weighted mesh editing uses typed, index-checked CRUD operations instead
  of generic JSON array mutation. It defaults bulk placement entries to the
  `NoCollision` profile unless the caller explicitly selects another valid
  profile.
- PCG audit is read-only. It reports managed ISM/HISM instance counts, mesh,
  collision, navigation, cull distances, sampled Landscape coverage and
  grounding ratios, plus likely empty-output causes without regenerating or
  cleaning the graph.
- Landscape Grass variety edits are typed, transactional, and index-checked.
  They validate mesh loading, density, jitter, cull distances, LOD, and scale
  ranges, then mark the asset package dirty without saving it. Replacing a
  variety intentionally resets per-platform and per-quality overrides to the
  supplied defaults.
- Fab inspection uses only the public Fab module, cache, and workflow-factory
  interfaces. It reads top-level cached ZIP metadata and exact import-type
  registration without inspecting authentication, querying marketplace
  services, inferring ownership, downloading content, installing plugins, or
  clearing the cache.
- No tool uses Slate UI automation.

Follow `docs/TOOLSETS_EXTENSION_GUIDELINES.md` when adding another domain
toolset. Runtime module, class, schema, error, and package
names must remain independent of the calling MCP client.

The GitHub repository and release process are specified in
`docs/GITHUB_RELEASES.md`.

## Development and support

- Contribution rules: `CONTRIBUTING.md`
- Security policy: `SECURITY.md`
- Extension design contract: `docs/TOOLSETS_EXTENSION_GUIDELINES.md`
- Usefulness-information design: `docs/USEFULNESS_INFORMATION.md`
- Capability roadmap, including the guarded Fab scope: `docs/ROADMAP.md`

Use GitHub Issues for reproducible feature requests and non-sensitive bugs.
Report security issues through GitHub private vulnerability reporting.
