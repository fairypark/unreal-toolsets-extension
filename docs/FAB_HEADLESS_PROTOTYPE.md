# Headless Fab owned-library prototype

## Scope

The prototype searches products already returned by the authenticated Fab
plugin's My Library synchronization path without opening or controlling the Fab
window.

It is intentionally restricted to:

- Unreal Engine 5.8;
- Fab plugin `VersionName` `0.0.14`;
- Fab plugin descriptor `Version` 14;
- a normal interactive Editor process;
- an active Fab login session in the current Editor process.

It does not run in commandlets because the Fab module does not initialize its
authentication and browser integration there.

In Fab 0.0.14, persistent-login restoration is initiated from Fab's private tab
creation path rather than module startup. The prototype cannot initialize that
session through a public headless API. A user who is not already logged in must
use Epic's own login flow; after the session is active, synchronization and
search require no Fab-window navigation or Slate automation.

## Tool sequence

1. `GetFabOwnedLibrarySearchCapability`
2. `RequestFabOwnedLibrarySync`
3. Poll `GetFabOwnedLibrarySnapshotInfo` until `EntryCount` stabilizes.
4. `SearchFabOwnedAssets`

The synchronization request replaces an ephemeral TEDS snapshot and returns
immediately. Fab's internal command does not publish a completion event, total
expected row count, authentication status, or structured failure result.
Consequently:

- `bCompletenessKnown` is always false;
- a zero-row snapshot can mean an empty library, an incomplete request, or a
  missing/expired active login session;
- rows can be partial while Fab follows pagination;
- a stabilized count is a practical observation, not a completeness proof.

## Data and security boundary

The result includes only product metadata that Fab places in its TEDS table:

- asset id and namespace;
- title and description;
- listing type;
- seller;
- source;
- listing URL.

The extension does not read, return, persist, or log:

- access or refresh credentials;
- cookies or browser storage;
- Epic account identifiers;
- payment data;
- signed or presigned download URLs.

The implementation includes no Fab private header. It finds the exact reflected
column types only after verifying the supported Fab plugin version. Any missing
type or property fails closed.

## Manual download handoff

Download initiation remains an explicit user action in Epic's Fab window. The
extension supports the safe, non-UI parts around that action:

1. Search the owned-library snapshot and identify the intended listing.
2. Record the current Unix timestamp.
3. Ask the user to click Download in the Fab window.
4. Poll `ListFabCachedEntries` with that value in
   `ModifiedAfterUnixSeconds`.
5. Treat a returned item only as a completed top-level ZIP cache entry; do not
   infer project installation or entitlement from its filename alone.

`ModifiedAfterUnixSeconds` is a strict lower bound and zero disables the
filter. `GetFabCacheEntryStatus` can then re-check a returned cache base name.
Some Unreal-native Fab products use an Add to Project or install pipeline and
do not produce a top-level ZIP in this cache. Those products require separate
destination-project verification.

## Why automated download initiation is still excluded

Fab's public `FFabDownloadRequest` can transfer a file only after another
component supplies a download URL and destination. The owned-library TEDS
snapshot does not provide a download URL, artifact selection contract, or a
public entitlement-to-download operation. Fab's current signed-URL and
Add-to-Project bridge is private and tied to the web frontend.

Passing a transient URL through an MCP tool is not an acceptable workaround:

- the URL may act as a bearer credential;
- it can appear in tool arguments, logs, traces, or client history;
- a caller-supplied URL does not prove ownership;
- generic destinations introduce arbitrary-write and cache-integrity risks.

## Acceptable future download design

A safe download tool requires Epic to export an operation equivalent to:

```text
RequestOwnedAssetDownload(
  asset_id,
  engine_version,
  artifact_or_format,
  quality
) -> opaque_download_handle
```

The Fab module must perform entitlement validation and signed-URL resolution
internally. The extension should then expose:

- `GetFabOwnedAssetDownloadOptions`;
- `StartFabOwnedAssetDownload`;
- `GetFabOwnedAssetDownloadStatus`;
- `CancelFabOwnedAssetDownload`.

Downloads should be restricted to Fab's own cache. Importing or adding content
to a project must remain a separate, explicitly requested operation with
destination validation, source-control guidance, postcondition checks, and
format-specific handling.

## Download path evaluation

| Candidate | Current assessment | Reason |
| --- | --- | --- |
| Epic-exported owned-library/download service | Preferred | Preserves entitlement checks and keeps transient credentials inside Fab. |
| Public `FFabDownloadRequest` with an MCP-supplied URL | Rejected | The caller must already possess a sensitive transient URL and ownership is unverified. |
| Direct calls to Fab's undocumented web endpoints | Rejected for distribution | Requires private authentication state and creates a fragile service contract. |
| Private `UFabBrowserApi` or web-frontend callback reflection | Rejected | Tied to the Fab window lifecycle and violates the headless and private-bridge boundaries. |
| Epic Games Launcher/Vault automation | Research only | No supported client-neutral CLI or entitlement/download contract is currently exposed. |
| Manual Fab-window start plus cache observation | Implemented | Keeps entitlement and signed-URL handling inside Fab while allowing non-UI polling of completed top-level ZIPs. |
| Cache-only import | Feasible follow-up | Uses already-downloaded local data; format-specific provenance and destination checks are still required. |
| External companion using a future official OAuth/API | Potential fallback | Acceptable only if Epic publishes the scopes and download service; credentials must stay in the companion's secure store. |

The next independently useful Fab addition can therefore be cache provenance
and guarded import readiness rather than pretending to provide acquisition.
For example, a tool could correlate a searched owned asset id with a verified
Fab cache entry and report whether a public workflow factory supports its
format. It must not copy arbitrary files or infer that a cache filename alone
proves entitlement.

## Verification record

The UE 5.8.1/Fab 0.0.14 prototype is verified with:

- a clean Win64 `BuildPlugin` package;
- repository distribution and observability-contract tests;
- packaged-plugin Editor automation for capability, empty-result, validation,
  and synthetic TEDS search;
- a synthetic reflected Fab row covering title, asset id, seller, listing URL,
  case-insensitive filters, and bounded results;
- a gated live-library test that retries synchronization, waits for a stable
  non-zero snapshot, and checks one owned result without logging its metadata.

The gated live test correctly fails in a fresh unattended Editor that has no
active Fab login session. Fab reports that My Library data cannot be retrieved
because the user is not logged in. This confirms the cold-start authentication
boundary described above; it is not treated as evidence that a persistent
credential is absent or invalid.

## Upgrade and removal

The prototype is an adapter to an implementation detail, not a public Fab API.
Every Engine or Fab plugin update requires:

1. exact descriptor-version review;
2. reflected schema verification;
3. clean package build;
4. live synchronization against a disposable Editor project;
5. MCP schema inspection;
6. empty, partial, filtered, and large-library search tests.

When Epic exposes an owned-library service, migrate to it and remove the
console-command/reflection adapter.
