# Usefulness information handling

Status: observability contract v1 and Codex adapter implemented

This document defines how `UnrealToolsetsExtension` should make usefulness
information available without turning the Unreal plugin into a
client-specific telemetry system.

The design borrows the consent, privacy, and outcome-first principles from
`unreal-editor-skills-for-codex`, while preserving this repository's stricter
runtime requirement: every Toolset must remain usable by Codex, Claude, and
other compatible MCP clients.

## Decision

The extension owns an **observability contract**, not a metrics database.

- The extension provides stable Toolset and operation names, operation classes,
  success semantics, safe error codes, and a versioned capability descriptor.
- An MCP-client adapter owns consent, session and turn correlation, local event
  storage, retention, summaries, ratings, and deletion.
- The extension does not upload metrics, call an analytics service, infer
  consent, or write client-specific usage events.
- A user may explicitly copy a shareable aggregate into a support issue. The
  extension and its adapters must not post it automatically.

For Codex, the existing consent-based local metrics implementation in
`unreal-editor-skills-for-codex` is the first adapter. It already observes
Unreal MCP Toolset and operation names, so duplicating collection inside the
Editor would create conflicting consent state and double-count calls.

Other clients may implement the same contract. No client adapter is required
for the extension to function.

## Why collection stays outside the extension

`UnrealToolsetsExtension` cannot reliably observe a client task boundary,
distinguish a retry from an intentional repeated operation, or ask for consent
after the user's current task. An MCP client can observe those facts exactly.

Putting collection in the extension would also introduce several problems:

- an additional consent prompt with no client-neutral post-task boundary;
- duplicate events when a client already has consented local metrics;
- ambiguous session and turn semantics;
- persistent state inside a distributable Editor plugin;
- pressure to inspect tool arguments or results that contain project, Asset,
  Actor, package, and file-path information.

The extension should therefore expose only deterministic, non-sensitive facts
that an authorized observer can classify without reading private payloads.

## Responsibility boundary

| Concern | Extension | Client adapter |
| --- | --- | --- |
| Register and execute typed Toolsets | Owns | Observes |
| Stable operation identity | Owns | Exact-match only |
| Operation class and success semantics | Publishes | Consumes |
| Stable error code | Publishes | May store an allowlisted code |
| Error detail, requests, and results | Returns for the task | Must not store |
| Consent and consent version | Does not own | Owns |
| Session and turn correlation | Does not infer | Owns when deterministic |
| Event storage and retention | Does not own | Local-only |
| Aggregate summary and 1-5 rating | Does not own | Owns |
| Upload or automatic issue submission | Never | Never |

## Signal model

Do not treat invocation count as usefulness and do not collapse the following
dimensions into a single score:

1. **Activation**
   - eligible sessions in which the extension was discovered after consent;
   - sessions with at least one extension operation.
2. **Reliability**
   - explicit operation successes and failures;
   - failure category from an allowlisted stable error code;
   - counts grouped by extension version, Toolset, and operation.
3. **Safety and verification**
   - successful self-verifying mutations;
   - successful calls to mutation-capable operations when the private
     invocation mode cannot be observed safely;
   - workflow-level read or verification calls after a mutation;
   - rollback attempts and verified rollback outcomes when a stable error code
     exposes them.
4. **Efficiency**
   - operation and turn durations;
   - retries only when the client has an exact turn identifier and exact
     operation identity.
5. **Feedback**
   - an optional local 1-5 rating targeted to this extension;
   - no free-text feedback in the metrics event log.

Every summary must show its sample counts. A dimension with no denominator is
reported as unavailable, not as zero.

### Two different verification signals

The extension's mutation contract says that a typed result is returned only
after bounded postconditions are verified. A successful mutation can therefore
count as `internal_postcondition_verified` when its catalog entry declares that
contract.

Some operations, including `ReimportLandscapeData` and
`RebuildStreamingProxies`, can perform either a dry run or a mutation. An
adapter that does not have a dedicated safe mode signal must report these as
`mutation_capable`, not claim that project state changed. It must not inspect
or store the request's `bDryRun` value. This keeps the privacy contract more
important than a more flattering mutation count.

This is not the same as a client performing a later read or verification call.
That separate behavior is `workflow_followup_verification`. Reporting them
separately avoids understating safe extension operations or overstating agent
workflow quality.

## Versioned observability catalog

The implementation packages `Resources/UsefulnessCatalog.json` and exposes
the same data through
`ObservabilityExtensionToolset.GetExtensionObservabilityInfo`. The catalog is
descriptive; reading it does not enable or collect metrics.

Each operation entry should contain only:

- exact Toolset name;
- exact operation name;
- operation class: `discovery`, `read`, `validation`, `mutation`,
  `mutation_capable`, or `save`;
- whether success implies internal postcondition verification;
- whether the operation may attempt rollback;
- allowlisted stable error codes and their non-sensitive categories;
- observability-contract version and extension version.

Adapters must exact-match the catalog. They must not guess operation classes
from prompt text, broad command history, request values, or arbitrary response
text. An unknown operation remains `unknown` until the catalog is updated.

The extension identity allowlist is:

- `ObservabilityExtensionToolset`;
- `LandscapeExtensionToolset`;
- `LandscapeGrassExtensionToolset`;
- `PlacementExtensionToolset`;
- `PCGExtensionToolset`;
- `FabExtensionToolset`;
- `VisualEvidenceExtensionToolset`.

Adding a Toolset requires updating the catalog and its contract test in the same
change.

### Initial operation mapping

Contract version 1 encodes the current API explicitly:

| Toolset | Class | Operations |
| --- | --- | --- |
| `ObservabilityExtensionToolset` | discovery | `GetExtensionObservabilityInfo` |
| `LandscapeExtensionToolset` | read | `GetLandscapeInfo`, `GetHeightStatistics` |
| `LandscapeExtensionToolset` | validation | `PreflightReimportLandscapeData`, `ValidateBuildingPad`, `AuditStreamingProxies` |
| `LandscapeExtensionToolset` | mutation | `CreateLandscape`, `ImportHeightmap`, `ReimportHeightmap`, `ImportWeightmap`, `SetLandscapeMaterial` |
| `LandscapeExtensionToolset` | mutation capable | `ReimportLandscapeData`, `RebuildStreamingProxies` |
| `LandscapeExtensionToolset` | save | `SaveLandscape` |
| `PlacementExtensionToolset` | mutation | `SnapActorToGround` |
| `PCGExtensionToolset` | read | `GetWeightedMeshEntries` |
| `PCGExtensionToolset` | validation | `AuditGraphInstance` |
| `PCGExtensionToolset` | mutation | `ReplaceWeightedMeshEntries`, `AddWeightedMeshEntry`, `UpdateWeightedMeshEntry`, `RemoveWeightedMeshEntry`, `ReorderWeightedMeshEntry` |
| `LandscapeGrassExtensionToolset` | read | `GetGrassVarieties` |
| `LandscapeGrassExtensionToolset` | mutation | `ReplaceGrassVarieties`, `AddGrassVariety`, `UpdateGrassVariety`, `RemoveGrassVariety`, `ReorderGrassVariety`, `SetGrassDensityScaling` |
| `FabExtensionToolset` | read | `GetFabIntegrationInfo`, `GetFabCacheSummary`, `ListFabCachedEntries`, `GetFabCacheEntryStatus`, `GetFabImportTypeSupport`, `GetFabOwnedLibrarySearchCapability`, `GetFabOwnedLibrarySnapshotInfo`, `SearchFabOwnedAssets` |
| `FabExtensionToolset` | ephemeral mutation | `RequestFabOwnedLibrarySync` |
| `VisualEvidenceExtensionToolset` | save | `CaptureCameraToPng` |
| `VisualEvidenceExtensionToolset` | validation | `VerifyEvidenceForSupervision` |

The packaged catalog uses one entry per operation; this table groups them only
for readability. PCG and Landscape Grass mutations verify in-memory state and
leave their packages dirty by design, so they are not summarized as durable
saves.

## Safe outcome classification

An adapter may inspect the MCP envelope in memory to determine whether the call
explicitly succeeded or failed, but it must never persist the response.

For failures, it may retain only an exact error code that appears in the
versioned catalog. Error detail is discarded because it can contain paths,
project names, Asset names, Actor names, or other private values.

Error codes should map to one of these stable categories:

- `invalid_request`;
- `invalid_target`;
- `precondition`;
- `unsupported`;
- `environment`;
- `execution`;
- `verification`;
- `save`;
- `rollback`;
- `unknown`.

Code suffixes such as `_ROLLED_BACK` may declare a verified recovery outcome,
but an adapter must not infer rollback success from natural-language detail.

## Adapter event model

The following is a conceptual local event. It is written by a consented client
adapter, never by the extension:

```json
{
  "schema_version": 1,
  "event": "extension_operation_finished",
  "timestamp": "2026-07-30T00:00:00Z",
  "observer": "codex_hook",
  "extension": "UnrealToolsetsExtension",
  "extension_version": "0.1.0",
  "session_id": "<local salted hash>",
  "turn_id": "<local salted hash>",
  "toolset": "LandscapeExtensionToolset",
  "operation": "ReimportLandscapeData",
  "operation_class": "mutation",
  "outcome": "failure",
  "error_code": "LANDSCAPE_EXTENSION_WEIGHT_SAVE_FAILED_ROLLED_BACK",
  "failure_category": "save",
  "rollback": "verified",
  "duration_ms": 1234
}
```

Fields that are unavailable from deterministic APIs are omitted. In
particular, another client must not fabricate `turn_id`, retry, duration, or
rollback values.

## Consent and privacy requirements for every adapter

An adapter must preserve these rules:

- preference states are `unset`, `enabled`, and `disabled`;
- collection begins only after affirmative consent;
- silence, installation, plugin discovery, and continued use are not consent;
- disabling stops new collection without deleting existing data;
- deletion is a separate explicit action and retains the preference;
- material changes to fields, storage, retention, or transmission increment
  `consent_version` and require consent again;
- storage is local-only, capped at 10,000 events, and retained for at most
  90 days;
- Hook or metrics failures never block or alter an Unreal operation.

Never store:

- prompts, messages, transcripts, or transcript paths;
- working directories, project names, map names, or file paths;
- Actor, Asset, component, object, package, graph, or node names;
- tool arguments or tool response content;
- raw session, turn, tool-call, machine, account, or installation identifiers;
- credentials, tokens, Fab authentication state, or marketplace ownership.

Generic Toolset names, operation names, cataloged error codes, extension and
Engine versions, platform class, timestamps, counts, durations, salted local
hashes, and explicit ratings are allowed.

## Codex adapter changes

The first integration extends `unreal-editor-skills-for-codex` rather than
adding collection code here:

1. After existing metrics consent, treat discovery of one of the exact
   extension Toolset names as an `extension_eligible` session.
2. Group existing `tool_finished` events by extension, Toolset, and operation.
3. Consume the versioned observability catalog instead of using operation-name
   prefixes for extension-specific classification.
4. Keep `internal_postcondition_verified` and
   `workflow_followup_verification` separate.
5. Add an optional 1-5 rating target for `UnrealToolsetsExtension`.
6. Expose an extension-only aggregate summary without exposing raw events.

Toolset and operation names are already collected by that adapter. Adding
extension or Engine version, allowlisted error code, or targeted feedback is a
field expansion and must be reviewed against its consent-version policy before
release.

## Local and shareable summaries

The local summary may show:

- eligible and active extension sessions;
- calls and explicit success rate by Toolset and operation;
- failure-category distribution;
- successful self-verifying mutations;
- successful mutation-capable calls, kept separate from confirmed mutations;
- workflow follow-up verification rate;
- rollback attempt and verified recovery counts;
- median and 95th-percentile duration;
- exact-turn retry counts when available;
- rating count and average.

A shareable summary is separately generated only on explicit request. It:

- contains aggregates, never raw events or identifiers;
- suppresses per-operation rows with fewer than five samples;
- rounds percentages and duration percentiles;
- states the collection window, observer, schema version, and sample counts;
- is returned to the user for review and is never uploaded automatically.

## Implementation phases

### Phase 1: contract only — complete

- Add the versioned observability catalog and reflection tests.
- Add a read-only extension information operation for version and catalog
  discovery.
- Audit stable error codes and assign safe categories.
- Make no local storage or network changes in this repository.

### Phase 2: Codex adapter — complete

- Add exact extension eligibility and extension-only summaries to
  `unreal-editor-skills-for-codex`.
- Add privacy canary tests proving request and response values never enter the
  event log.
- Re-run that project's consent-version review.

### Phase 3: other adapters — contract available

- Document the same contract for other MCP clients that request it.
- Do not add an in-Editor collector merely to obtain cross-client totals.
- Revisit the decision only if Unreal exposes a deterministic, client-neutral
  consent and invocation lifecycle API.

## Validation gates

Every usefulness-information change must verify:

- no collection or side effect occurs by installing or loading this extension;
- all catalog entries match reflected registered `AICallable` operations;
- every stored error code is cataloged and no error detail is retained;
- canary paths, project names, Actor names, Asset names, arguments, and response
  values cannot appear in adapter logs;
- unknown operations and failures remain `unknown` rather than being guessed;
- successful mutation semantics match the implementation's postcondition
  checks;
- metrics failure cannot change an operation result or Editor state;
- disabling and deletion preserve their separate semantics in each adapter;
- no automatic network transmission exists.
