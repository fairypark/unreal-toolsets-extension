# Unreal Toolsets Extension 0.2.0

This is a beta prerelease for Unreal Engine 5.8 Editor targets on Win64.

This release adds a client-neutral observability contract without adding an
analytics database, consent state, or network transmission to the Unreal
plugin.

## Added

- `ObservabilityExtensionToolset.GetExtensionObservabilityInfo`
- versioned `Resources/UsefulnessCatalog.json`
- exact classifications for all 34 registered `AICallable` operations
- allowlisted categories for all 55 stable error codes
- internal postcondition, persistence, dry-run-capability, and rollback
  semantics
- reflection and distribution tests that keep the catalog synchronized with
  the registered API and raised errors

## Privacy boundary

The Toolset returns only generic plugin, Engine, operation, and error-contract
metadata. It does not inspect or store prompts, MCP request or response
payloads, paths, project names, Actor or Asset names, raw identifiers,
credentials, or usage events.

Consent, local retention, summaries, ratings, and deletion remain the
responsibility of an optional MCP-client adapter. No information is uploaded
or submitted automatically.

## Compatibility

- Unreal Engine 5.8.1
- Win64 Editor targets
- MCP-client neutral

Other Engine versions and platforms remain unverified until a separately
named release asset passes the complete validation matrix.

## Validation

- repository contract and distribution tests: 14 passed
- UE 5.8.1 Win64 `BuildPlugin`: passed
- packaged-plugin automation group: 9 passed, 0 failed
- all six extension Toolsets registered from the packaged DLL
- ZIP layout, commit-bound manifest, and SHA-256 checksum: verified
