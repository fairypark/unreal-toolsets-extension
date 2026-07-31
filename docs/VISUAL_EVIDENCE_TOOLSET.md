# VisualEvidenceExtensionToolset

Status: implemented for Unreal Engine 5.8.1 in plugin version 0.4.0

## Why it was added

Automated level and environment work needs review images, but an ordinary
screenshot path is not evidence that the image came from the intended camera
or project state. It also does not show whether compilation and streaming had
settled, whether editor overlays were excluded, whether the viewport was
restored, or whether the bytes changed before an independent supervisor read
them.

`VisualEvidenceExtensionToolset` was added as a separate, client-neutral
Toolset to close that provenance and integrity gap. The capability applies to
many level-design workflows, so it does not belong to the Fab, Landscape, PCG,
or any particular MCP-client integration. It uses Unreal Editor C++ APIs
because the required viewport client, render flush, camera view,
compilation/streaming state, screenshot pixels, atomic file operations, and
hashing are not available as one safe typed operation elsewhere.

The Toolset is deliberately not a visual-quality judge. Composition, lighting,
art direction, and acceptance remain the responsibility of a person or an
independent visual supervisor that receives the verified PNG.

## Workflow

1. Create and retain a real `CameraActor` in the current Editor world.
2. Call `CaptureCameraToPng` with a stable camera role and a unique relative
   `.png` path beneath Project `Saved`.
3. Retain both SHA-256 values returned by the capture receipt outside the
   generated files.
4. Immediately before supervision, call `VerifyEvidenceForSupervision` with
   the same persistent camera, role, relative path, and both retained hashes.
5. Hand off the verified PNG only after that call succeeds.

## Capture guarantees

- rejects null, transient, wrong-world, or non-persistent cameras;
- rejects traversal, absolute paths, non-PNG targets, and overwrite attempts;
- requires a perspective level viewport and an invariant 1280x720 minimum;
- applies the camera component's complete view and post-process state;
- waits for asset compilation, shader compilation, and streaming to settle;
- performs warmup draws and proves two consecutive stable frame comparisons;
- disables common editor overlays for the capture and restores prior viewport
  state on every exit path;
- writes a unique PNG and `.evidence.json` receipt atomically beneath Project
  `Saved` and verifies the committed pair before returning.

The receipt binds the evidence hash and byte count, dimensions, camera role and
identity, world identity, complete camera view-state digest, capture policy,
timestamps, UI-clean flags, and render-settlement observations. Timestamp
ordering allows only a two-second filesystem-precision tolerance; exact
recorded modification-time and byte-hash matches are still required.

## Verification guarantees

`VerifyEvidenceForSupervision` re-reads the committed pair and fails closed if
the externally retained PNG or receipt hash differs, the PNG is malformed, the
camera or role differs, the current camera/view state changed, dimensions fall
below policy, timestamps are inconsistent, or the receipt no longer proves the
required settlement and UI-clean contract.

## Boundaries

The Toolset does not:

- score visual quality or approve its own output;
- upload, share, or display evidence outside Unreal;
- automate Slate or desktop UI input;
- capture arbitrary operating-system windows;
- overwrite existing evidence;
- write outside the current project's `Saved` directory;
- make a cryptographic claim about GPU authenticity or defend against a fully
  compromised Editor process.

The two returned SHA-256 values are trust anchors only when the caller retains
them separately from the generated evidence pair until verification.
