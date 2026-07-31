# Unreal Toolsets Extension 0.3.0

This beta prerelease targets Unreal Engine 5.8.1 Editor on Win64.

## Added

- exact-version, non-UI owned-library synchronization and search for Fab
  `0.0.14` (`Version` 14)
- typed filters for title, asset id, seller, listing type, source, and optional
  description matching
- capability and snapshot tools that fail closed when Fab's private reflected
  schema or synchronization command changes
- `ModifiedAfterUnixSeconds` on `ListFabCachedEntries` for observing completed
  top-level ZIPs after a user starts a download in the Fab window
- portable Unreal MCP configuration example and a connection diagnostic for
  `list_toolsets`, `describe_toolset`, and `call_tool`

## Download boundary

The release does not initiate Fab downloads, inspect credentials, or expose
signed URLs. Acquisition remains an explicit click in Epic's Fab window. After
that click, clients may poll the public local ZIP cache. Add to Project and
engine/plugin installation pipelines may not create a ZIP and require separate
destination verification.

## Compatibility

- Unreal Engine 5.8.1
- Fab plugin 0.0.14 (`Version` 14)
- Win64 Editor targets
- MCP-client neutral

Other Engine, Fab, and platform versions remain unsupported until their exact
schema and complete validation matrix pass independently.

## Validation

- repository distribution and observability-contract tests: 15 passed
- clean UE 5.8.1 Win64 `BuildPlugin` package: passed
- packaged-plugin Fab automation group: 4 passed, 0 failed; the opt-in live
  account-library case remained disabled
- live MCP connection diagnostic verified `list_toolsets`, `describe_toolset`,
  and `call_tool`
- commit-bound ZIP layout, manifest, and SHA-256 checksum are produced by the
  release workflow
