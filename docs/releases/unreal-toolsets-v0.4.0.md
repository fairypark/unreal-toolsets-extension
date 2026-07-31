# Unreal Toolsets Extension 0.4.0

This beta prerelease targets Unreal Engine 5.8.1 Editor on Win64.

## Added

- `VisualEvidenceExtensionToolset.CaptureCameraToPng` for persistent,
  fixed-camera, UI-clean review evidence beneath Project `Saved`
- `VisualEvidenceExtensionToolset.VerifyEvidenceForSupervision` for fail-closed
  verification immediately before an independent review handoff
- a versioned JSON receipt binding PNG and receipt SHA-256 trust anchors,
  dimensions, camera and world identity, complete camera view state, capture
  policy, timestamps, and render-settlement observations
- live MCP connection diagnostics that can require a named Toolset and its
  expected operations
- explicit documentation of why the Toolset exists, its workflow, guarantees,
  trust boundary, and non-goals

## Safety boundary

The Toolset does not score or approve visual quality, automate desktop UI,
capture arbitrary windows, upload evidence, overwrite files, or write outside
Project `Saved`. It restores the previous viewport state and refuses the result
unless compilation, streaming, dimensions, temporal stability, persistent
camera identity, committed bytes, and receipt integrity satisfy the contract.

## Compatibility

- Unreal Engine 5.8.1
- Win64 Editor targets
- MCP-client neutral

Other Engine versions and platforms remain unsupported until their clean build
and live validation matrix pass independently.

## Validation

- repository distribution and observability-contract tests: 16 passed
- clean UE 5.8.1 Win64 `BuildPlugin` package: passed
- packaged-plugin VisualEvidence automation tests in `CodexMiniArena`: 2
  passed, 0 failed using D3D12 SM6 on an NVIDIA GeForce RTX 4090
- live MCP discovery verified `VisualEvidenceExtensionToolset` and both
  operations through `list_toolsets`, `describe_toolset`, and `call_tool`
- commit-bound ZIP layout, manifest, and SHA-256 checksum
