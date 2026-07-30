# UnrealToolsetsExtension design guidelines

Use these rules for every toolset added to this distributable Unreal plugin.

## 1. Fill verified capability gaps

- Inspect the installed official toolsets before proposing a new API.
- Add a tool only when no official typed operation provides equivalent behavior.
- Prefer composition with an official generic tool over duplicating it.
- Re-audit overlaps for every supported Unreal Engine release.
- When Epic adds an equivalent API, document migration, deprecate the extension API, and remove it in a later major release. Do not shadow an official toolset name.

## 2. Keep ownership separate

- Never modify Engine plugin files, `AllToolsets.uplugin`, or an installed Epic binary.
- Keep extension code in the project or distributed `UnrealToolsetsExtension` plugin.
- Depend directly on `ToolsetRegistry`; do not require the `AllToolsets` aggregator.
- Name domain classes `U<Domain>ExtensionToolset` so future official classes cannot collide.
- Keep one domain toolset per file pair and register each class explicitly in the extension module.
- Keep runtime names independent of any MCP client. The same registered toolset
  must be usable by Claude, Codex, and other compatible MCP clients without
  client-specific code paths or branding.

## 3. Design for unrelated projects

- Do not embed project names, asset paths, Actor labels, coordinates, map layouts, user directories, ports, or private infrastructure.
- Accept explicit targets and typed request values. Never depend on Editor selection as an undocumented target.
- Express distances and heights in centimeters, angles in degrees, and resolutions in vertices or pixels. Put units in field documentation.
- Make policy thresholds caller-configurable. A validation tool may measure a building pad; it must not encode one studio's art direction as universal truth.
- Return empty collections for meaningful no-result states and raise stable errors for invalid or unsafe operations.

## 4. Keep APIs small and typed

- Expose static `AICallable` functions with reflected request and result structs.
- Return structured Unreal values, not JSON-formatted strings.
- Separate inspection, validation, mutation, and save operations when callers benefit from composing them.
- Return a typed result only after postconditions are verified. Raise on failure instead of returning a generic success boolean or error string.
- Reuse shared package-save, target-resolution, transaction, and error helpers without merging unrelated domain behavior.

## 5. Make mutation recoverable

- Validate the target, world, source data, dimensions, formats, permissions, streaming coverage, and destination before calling `Modify()`.
- Save every dirty affected package before difficult-to-reverse changes.
- Use `FScopedTransaction` and capture domain data needed for in-memory rollback.
- Verify representative values and the complete affected data when the cost is bounded.
- Restore and verify the original data after any post-mutation failure.
- Distinguish an undo transaction from a durable source-control checkpoint.
- Refuse partial World Partition writes unless the tool explicitly supports and verifies the requested region.

## 6. Use stable Unreal surfaces

- Prefer public exported APIs and modules.
- Do not call private Editor modes, detail customizations, or Slate implementation classes.
- Keep version-specific compatibility code narrow and explain why it exists.
- If a public declaration is not exported from the Engine DLL, use another public data path or stop; never patch the Engine to make the extension compile.
- Treat Slate automation as a temporary external workflow, not part of a reusable domain toolset.

## 7. Test as a distributed product

For each supported Engine version:

1. Run `BuildPlugin` from a clean source package.
2. Run reflection-contract tests for registration, `AICallable`, static functions, and typed schemas.
3. Run success, empty-state, invalid-target, invalid-input, save-failure, rollback, and streaming tests for every operation.
4. Install the packaged plugin into a disposable project rather than compiling only in the maintainer's project.
5. Discover it through Model Context Protocol and compare the live schemas with the documented contract.
6. Test source installation and packaged binary installation.
7. Record the exact Engine version and target platforms verified for the release.

Do not infer compatibility with a new Engine version from descriptor loading alone.

## 8. Release without personal state

- Package only source, binaries for declared platforms, config, resources, license, and user documentation.
- Exclude `Intermediate`, `Saved`, local build logs, generated host projects, test maps, private assets, and machine-specific configuration.
- Do not update a maintainer's personal marketplace installation as part of packaging.
- Use semantic versions and release notes that distinguish added APIs, behavior changes, deprecations, and verified Engine versions.
- Preserve the repository license and any third-party notices in source and packaged distributions.

## 9. Keep usefulness observation client neutral

- Follow `USEFULNESS_INFORMATION.md` before adding metrics, analytics, ratings,
  consent, or usage-summary behavior.
- Keep consent, event storage, retention, and summaries in an MCP-client
  adapter; do not duplicate them in this Editor plugin.
- Publish only deterministic, non-sensitive observability metadata: exact
  operation identity, operation class, success semantics, and allowlisted
  stable error codes.
- Never inspect or persist prompts, requests, responses, paths, project state,
  Actor or Asset names, credentials, or raw identifiers for usefulness
  analysis.
- Do not infer task boundaries, retries, verification, or failures when the
  required lifecycle signal is unavailable.
- Never upload usefulness information or submit it to an issue automatically.

## Review checklist

- Does an official tool now cover this capability?
- Is the new tool useful without the maintainer's project?
- Are every target, unit, threshold, and persistence choice explicit?
- Can failure leave partially modified or dirty state?
- Does World Partition behavior match the result claim?
- Are all functions static, typed, registered, documented, and tested?
- Can the plugin build without modifying the Engine?
- Can another user install it by copying one packaged plugin folder?
- Does any usefulness observation preserve the client-neutral consent and
  privacy boundary?
