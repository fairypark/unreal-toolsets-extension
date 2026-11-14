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
public C++ interfaces plus an exact-version, headless owned-library search
prototype:

- `GetFabIntegrationInfo`
- `GetFabCacheSummary`
- `ListFabCachedEntries`
- `GetFabCacheEntryStatus`
- `GetFabImportTypeSupport`
- `GetFabOwnedLibrarySearchCapability`
- `RequestFabOwnedLibrarySync`
- `GetFabOwnedLibrarySnapshotInfo`
- `SearchFabOwnedAssets`

The prototype is restricted to Unreal Engine 5.8 and Fab plugin version
`0.0.14` (`Version` 14). It submits Fab's internal TEDS My Library command and
reads only reflected product metadata. The current Editor process must already
have an active Fab login session; that legacy Fab version does not expose a
public headless login initializer. The prototype never reads or exposes credentials, account ids,
cookies, or signed download URLs. Because the internal command has no public
completion signal, callers must poll the snapshot count until it stabilizes and
results always report that completeness is unknown.

The extension does not initiate downloads. For the legacy workflow, download
initiation remains in Epic's Fab window. A client can record the
current Unix time, ask the user to click Download, and then poll
`ListFabCachedEntries` with `ModifiedAfterUnixSeconds` to detect newly completed
top-level ZIP cache entries without further screen control. Some native Unreal
content uses an install/Add to Project path instead of the ZIP cache; those
artifacts must be verified in the destination project rather than inferred from
this cache tool.

On UE 5.9, prefer Epic's official `FabToolset.FabToolset` search operations.
The extension's capability report passively identifies their reflected names;
it does not authenticate or call the service. The legacy private-schema gate
remains unchanged. See `docs/FAB_HEADLESS_PROTOTYPE.md` for migration details.

`ObservabilityExtensionToolset` exposes client-neutral, read-only metadata:

- `GetExtensionObservabilityInfo`

It returns the extension and Engine versions plus a versioned catalog of exact
operation classes, postcondition and persistence semantics, and allowlisted
stable error codes. It does not enable collection or store usage data.

`VisualEvidenceExtensionToolset` provides persistent fixed-camera review
evidence:

- `CaptureCameraToPng`
- `VerifyEvidenceForSupervision`

The operation requires a loaded `CameraActor` from the current Editor world,
requires a stable camera role and minimum dimensions, waits for compilation and
streaming to settle, enforces an invariant 1280x720 floor and a maximum 0.5
mean RGB pixel-delta threshold, proves two consecutive stable frame pairs,
removes editor overlays, restores the previous viewport state, and writes only
beneath the current project's `Saved` directory. Every unique PNG is paired with a
`.evidence.json` receipt binding its SHA-256, dimensions, camera and world
identity, applied camera-component view and post process, complete
`FMinimalViewInfo` digest, applied dimension and stability thresholds,
timestamps, UI-clean state, and settlement data.
`VerifyEvidenceForSupervision` re-reads that pair immediately before handoff and
requires the capture-returned PNG and receipt SHA-256 values as trust anchors.
It fails closed if the files, role, persistent camera, complete camera/view
state, minimum dimensions, timestamps, or render-settlement contract no longer
match.

It was added because ordinary screenshots do not prove that a review image
came from the intended persistent camera, current world and camera state, a
settled renderer, or unchanged bytes at supervisor handoff. The Toolset fills
that provenance and integrity gap; it deliberately does not judge composition
or visual quality. See `docs/VISUAL_EVIDENCE_TOOLSET.md` for the addition
rationale, workflow, guarantees, and limits.

## Engine compatibility

Version 0.5.0 provides a Win64 Editor ZIP for UE 5.8.4 (CL 58902052) and
a separate experimental ZIP for UE 5.9.0 Preview (CL 60351404). Match the
version and changelist shown in the Editor's About dialog to the ZIP name.
See [the compatibility notes](docs/UE59_COMPATIBILITY.md) for the official
Fab migration path and test coverage.

Existing extension toolset and operation names remain unchanged. Official
Editor tool names and MCP discovery changed in UE 5.9; see
`docs/MCP_COMPATIBILITY.md` before adapting external workflows.

## Requirements

- one of the two Win64 Editor builds listed above
- the experimental `ToolsetRegistry` plugin
- the built-in `PCG` plugin
- the built-in `Fab` plugin
- an Editor target

Win64 is the validation target. Other Engine builds and platforms require
their own clean build and live test before being declared supported. Preview
compatibility does not establish compatibility with later Preview or final
releases.

`AllToolsets` and `ModelContextProtocol` are not build dependencies. A project
needs `ModelContextProtocol` only when it wants to expose registered toolsets
over MCP.

## Connect this source workspace to Unreal MCP

The tool-search operations `list_toolsets`, `describe_toolset` (UE 5.8),
`describe_toolsets` and `describe_tools` (UE 5.9), and `call_tool` belong to
Unreal Engine's `ModelContextProtocol` plugin. They are
not implemented by this extension. Codex only sees them when the workspace
being opened contains an MCP client configuration that points to the running
Editor.

Copy the portable example to a machine-local configuration:

```powershell
Copy-Item .\.codex\config.toml.example .\.codex\config.toml
```

Then update its URL to match the Editor's
`ServerPortNumber` and `ServerUrlPath`. The local file is intentionally ignored
by Git because the endpoint is machine-specific. Start a new Codex task after
creating or changing the configuration; MCP servers are connected when the
task starts.

The connection can be verified without driving the Editor UI:

```powershell
.\scripts\Test-UnrealMcpConnection.ps1 `
  -Endpoint http://localhost:8000/mcp `
  -ToolsetName UnrealToolsetsExtension.VisualEvidenceExtensionToolset `
  -RequiredOperationNames CaptureCameraToPng,VerifyEvidenceForSupervision
```

The test initializes an MCP session and detects the available tool-search
protocol. It also verifies every requested operation against the live schema.
It never reads or prints credentials, cookies, or session identifiers.

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
   `PCGExtensionToolset`, `FabExtensionToolset`,
   `ObservabilityExtensionToolset`, and
   `VisualEvidenceExtensionToolset`.

## Safety and current limits

- Landscape mutations require an explicit editable Landscape edit-layer GUID.
- Heightmap and weightmap imports require an exact full-Landscape resolution.
- Mutations refuse regions with unloaded World Partition components.
- `CreateLandscape` requires a world with World Partition disabled.
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
- Fab cache inspection uses the public Fab module, cache, and workflow-factory
  interfaces. The optional UE 5.8 owned-library prototype invokes an internal
  console command and reads a private reflected TEDS schema behind an exact
  Fab-version gate. It never accesses authentication values, account ids,
  signed URLs, payment data, download initiation, installation, or cache
  deletion. After a user starts a download in the Fab window, it can inspect
  and time-filter completed top-level ZIP cache entries.
- No tool uses Slate UI automation.
- Visual evidence capture requires a persistent `CameraActor`, rejects paths
  outside Project `Saved`, settles multiple viewport draws, preserves existing
  evidence by refusing overwrite, atomically moves a verified temporary PNG
  and receipt into place, and verifies the final byte count and SHA-256 before
  return.

See `docs/FAB_HEADLESS_PROTOTYPE.md` for the prototype contract, verification
limits, and the requirements for a future safe download implementation.

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
- Visual evidence rationale and contract: `docs/VISUAL_EVIDENCE_TOOLSET.md`
- Capability roadmap, including the guarded Fab scope: `docs/ROADMAP.md`

Use GitHub Issues for reproducible feature requests and non-sensitive bugs.
Report security issues through GitHub private vulnerability reporting.
