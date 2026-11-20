# UE 5.9 Preview compatibility

Version 0.5.0 includes experimental UE 5.9 Preview support. Its two Win64
packages identify the exact engine patch and changelist in their filenames.

## Preview 2 (CL 60738873)

The 0.5.0 tag builds with Win64 BuildPlugin on UE 5.9.0 Preview 2. The resulting Editor module reports BuildId `60738873`. Use a package built for that CL when upgrading from `60351404`. Preview 2 ships Fab 0.0.23. Its headers retain `SearchFabMyLibrary` and opaque cursor pagination.

## Compatibility contract

- Retain all 40 extension operation names and their typed request/result
  contracts. Fab capability fields may grow additively.
- Keep UE 5.8 support. Build and test it separately from UE 5.9.
- Never alter Engine plugins, AllToolsets, or installed Epic binaries.
- Use a new test level and test-only output folder. Do not migrate an existing
  production level or modify its assets as part of compatibility work.
- Each release ZIP has a matching SHA-256 checksum and manifest identifying
  the source revision and Engine build.

## Registry and access modes

UE 5.9 retains UToolsetDefinition, the existing public include paths, and
UToolsetRegistry::RegisterToolsetClass/UnregisterToolsetClass. The registry's
core/engine/editor split does not itself require changing this extension's
registration or adding an AllToolsets dependency.

Every extension operation now declares AIAccessMode. Discovery, inspection and
validation are Read. Mutation, mutation-capable dry-run APIs and file/package
saves are Write. This metadata is checked against the existing observability
catalog and actual reflected functions. It does not grant client permission
to mutate a project and does not replace the operation's own safety checks.

## MCP protocol and tool names

UE 5.8 uses describe_toolset(toolset_name). UE 5.9 uses
describe_toolsets(toolset_names, include_schemas), with include_schemas false
by default, and adds describe_tools(tool_names). Clients that need argument
schemas must request them explicitly. The connection script detects the
protocol from tools/list and does not assume one engine's response shape
applies to the other. See MCP_COMPATIBILITY.md for exact name discovery and
the official Editor tool migrations.

## Fab: official tools first

On the tested UE 5.9 installation Fab supplies FabToolset.FabToolset, including
SearchFabMyLibrary and SearchFabListings. Use the official tools for library
and catalog search. Their authentication, pagination, errors and asynchronous
completion differ from the legacy extension's local TEDS snapshot.

The old extension snapshot APIs remain present for existing clients. Their
original exact UE 5.8 + Fab 0.0.14 gate remains closed on other versions; it is
not safe to loosen a private-schema gate merely because a new engine builds.
Cache inspection remains an extension feature. Capability discovery can
recommend the official reflected tool without logging in, accessing the
network, starting a download or claiming that the service works. See
FAB_HEADLESS_PROTOTYPE.md for the detailed migration contract.

## Development validation on November 10, 2026

Validated on 2026-11-10 from baseline v0.4.0 commit
`5960b1a49be10adba74dc3d1736ffc1d53a2103d`, branch
`codex/ue59-preview-compat-20261110` (local, uncommitted development changes).

- UE 5.9.0 preview, changelist/BuildId 60351404, Fab 0.0.22: Win64
  BuildPlugin packaging passed.
- UE 5.8.4, changelist 58902052, BuildId 55116800, Fab 0.0.21: Win64
  BuildPlugin packaging passed independently.
- Repository Python suite: 36 tests passed, including real PowerShell against
  temporary MCP mocks, access-mode/catalog checks and Fab migration guards.
- Live MCP in each engine: all 7 extension toolsets and 40 operation schemas
  discovered; observability and Fab capability inspection called successfully.
  The connection helper validated both singular and plural describe protocols.
- UE 5.9's official Fab descriptor and its three search/detail tool schemas
  were inspected. The extension recommended the actual reflected
  `FabToolset.FabToolset.SearchFabMyLibrary` name. No online search was run.
- Both engines' final full automation reports contain 14 Success states,
  0 Fail and 0 NotRun. Of these, 12 tests executed their checks; two Fab tests
  explicitly skipped: live owned-library search requires opt-in, and synthetic
  private TEDS search requires the exact legacy Fab version. These skips are
  not service-integration passes.
- Executed coverage includes landscape create/audit, grass editing, PCG
  operations/audit, placement, reflected contract/access modes, Fab read-only
  inspection/validation, and visual capture plus receipt/tamper verification.
  Automation creates disposable editor worlds; existing production assets
  are not migration fixtures.

### Test-harness corrections and remaining limits

The initial visual fixture captured in the same frame as new world/camera
creation. Its first draw started four lazy asset compilations plus shaders;
the production final guard correctly refused evidence. The test now yields
real Editor ticks, prewarms the camera, requires three compilation-idle draws,
and fails after 120 seconds. The aspect-ratio validation fixture explicitly
uses an unconstrained test camera. Production capture thresholds, minimum
resolution, hashes, two stable frame pairs, state restoration and final
compilation/streaming rejection remain unchanged. The guard now reports its
three counters for diagnosis.

One UE 5.8 D3D12 background-worker access violation occurred while both engine
automation processes ran concurrently. The same final package passed the
complete suite on a standalone retry. The cause is not established; this is
not a claim that concurrent editor rendering is validated or the driver issue
is fixed. Run the two suites serially when reproducing these results.

No unsupported platform, online Fab login/search/download, cooked runtime,
production-level migration, exhaustive operation/rollback coverage, visual
art review, or future UE preview compatibility is implied. The legacy private
Fab search remains unavailable on the tested Fab 21 and 22 installations;
its original exact-version safety gate was not broadened.
