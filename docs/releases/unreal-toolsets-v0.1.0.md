# Unreal Toolsets Extension 0.1.0

This is the first client-neutral release of the separately installed Unreal
Editor plugin that fills verified gaps in Epic's typed ToolsetRegistry APIs.
It does not modify or replace `AllToolsets`, `ToolsetRegistry`, or any Engine
file.

## Included toolsets

`LandscapeExtensionToolset` provides:

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

`PlacementExtensionToolset` provides:

- `SnapActorToGround`

`PCGExtensionToolset` provides:

- `GetWeightedMeshEntries`
- `ReplaceWeightedMeshEntries`
- `AddWeightedMeshEntry`
- `UpdateWeightedMeshEntry`
- `RemoveWeightedMeshEntry`
- `ReorderWeightedMeshEntry`
- `AuditGraphInstance`

`LandscapeGrassExtensionToolset` provides:

- `GetGrassVarieties`
- `ReplaceGrassVarieties`
- `AddGrassVariety`
- `UpdateGrassVariety`
- `RemoveGrassVariety`
- `ReorderGrassVariety`
- `SetGrassDensityScaling`

`FabExtensionToolset` provides:

- `GetFabIntegrationInfo`
- `GetFabCacheSummary`
- `ListFabCachedEntries`
- `GetFabCacheEntryStatus`
- `GetFabImportTypeSupport`

## Compatibility

- Verified Engine: Unreal Engine 5.8.1
- Verified platform: Win64 Editor
- Required Engine plugin: `ToolsetRegistry`
- Required built-in plugins: `PCG`, `Fab`
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

Batch Landscape reimport supports preflight and dry-run validation for exact
RAW16 height and RAW8 layer data, records a durable recovery save, can export a
raw-data backup, and verifies that transform, material, component layout, and
loaded streaming-proxy layout remain unchanged.

Streaming-proxy audit is read-only. Rebuild requires a complete loaded
Landscape, a current-grid guard, dry-run review, a literal confirmation token,
and recovery package saves. Proxy count is treated as a measured outcome rather
than a target.

Ground snapping ignores the moved Actor and its attached Actors, supports
negative world heights and caller-selected collision channels, can prefer or
require Landscape hits, aligns the complete Actor bounds bottom, and reports
the hit Actor, component, location, normal, and applied vertical delta.

PCG weighted mesh mutation is a typed replacement for ambiguous generic
`MeshEntries` array edits. Every operation validates the node, selector, index,
mesh, collision profile, cull distances, and navigation setting before
changing the graph. PCG audit reads existing managed ISM/HISM output without
executing or cleaning the graph.

Landscape Grass mutation uses typed, transactional variety operations with
mesh, density, jitter, cull-distance, LOD, and scale validation. It marks the
asset package dirty without saving it. Replacing a variety resets its
per-platform and per-quality overrides to the supplied defaults.

Fab operations are read-only and use the public Fab module, cache, and workflow
factory registry. They do not inspect authentication, call marketplace
services, infer ownership, download content, install plugins, or clear the
cache.

`CreateLandscape` does not yet create World Partition streaming proxies and
therefore rejects partitioned worlds. No runtime tool drives Slate UI.

## Validation

- Unreal `BuildPlugin`: passed for UE 5.8 Win64
- Disposable-project plugin loading: passed
- `AI.UnrealToolsetsExtension.Contract`: passed
- `AI.UnrealToolsetsExtension.Landscape.CreateAndAudit`: passed
- `AI.UnrealToolsetsExtension.Placement.NegativeHeightAndSelfIgnore`: passed
- `AI.UnrealToolsetsExtension.PCG.WeightedMeshEntries`: passed
- `AI.UnrealToolsetsExtension.PCG.EmptyGraphAudit`: passed
- `AI.UnrealToolsetsExtension.LandscapeGrass.Varieties`: passed
- `AI.UnrealToolsetsExtension.Fab.ReadOnlyInspection`: passed
- `AI.UnrealToolsetsExtension.Fab.Validation`: passed
- Repository unit tests: passed
- Standalone repository tests and release-script validation: passed

Live MCP discovery, exact RAW16/RAW8 mutation fixtures, rollback/save-failure
injection, World Partition rebuild scenarios, and a non-empty generated PCG
HISM audit, plus Landscape material Grass Output binding fixtures, must be
completed before the draft release is promoted to a final public release.
Live MCP validation must also confirm Fab cache responses contain no
authentication or entitlement data.
