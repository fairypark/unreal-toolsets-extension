# Community Toolsets Extension

[![Validate repository](https://github.com/fairypark/unreal-community-toolsets-extension/actions/workflows/validate.yml/badge.svg)](https://github.com/fairypark/unreal-community-toolsets-extension/actions/workflows/validate.yml)

`CommunityToolsetsExtension` is a distributable Unreal Editor plugin for typed
ToolsetRegistry capabilities that are missing from the official Unreal toolsets.
It complements Epic plugins without modifying or replacing them.

The plugin is MCP-client neutral. Once Unreal's `ModelContextProtocol` plugin
discovers the registered toolsets, Claude, Codex, and any other compatible MCP
client can call the same typed operations. No runtime API is tied to a Codex
installation.

The first domain toolset is `LandscapeExtensionToolset`, which exposes:

- `GetLandscapeInfo`
- `CreateLandscape`
- `ImportHeightmap`
- `ReimportHeightmap`
- `ImportWeightmap`
- `SetLandscapeMaterial`
- `GetHeightStatistics`
- `ValidateBuildingPad`
- `SaveLandscape`

## Requirements

- Unreal Engine 5.8 for version `0.1.0`
- the experimental `ToolsetRegistry` plugin
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
  -OutputDirectory "C:\short-path\CommunityToolsetsExtension"
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

1. Copy the packaged `CommunityToolsetsExtension` folder to:

   ```text
   <Project>/Plugins/CommunityToolsetsExtension
   ```

2. Enable `CommunityToolsetsExtension` in the project. Do not edit
   `Engine/Plugins/Experimental/Toolsets/AllToolsets/AllToolsets.uplugin`.
3. Restart the Editor after adding a packaged binary plugin.
4. When using MCP, start the server and run:

   ```text
   ModelContextProtocol.RefreshTools
   ```

5. Confirm `list_toolsets` contains `LandscapeExtensionToolset`.

## Safety and current limits

- Landscape mutations require an explicit editable Landscape edit-layer GUID.
- Heightmap and weightmap imports require an exact full-Landscape resolution.
- Mutations refuse regions with unloaded World Partition components.
- `CreateLandscape` currently refuses World Partition worlds because version
  `0.1.0` does not create streaming proxies.
- Reimport validates the recorded source, saves a recovery point, captures the
  original height data, verifies the complete write plus representative points,
  and rolls back on failure.
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
- Capability roadmap, including the guarded Fab scope: `docs/ROADMAP.md`

Use GitHub Issues for reproducible feature requests and non-sensitive bugs.
Report security issues through GitHub private vulnerability reporting.
