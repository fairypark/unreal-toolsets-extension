# GitHub distribution for UnrealToolsetsExtension

Distribute the Unreal runtime extension through a client-neutral GitHub
repository and GitHub Releases. The recommended repository name is
`unreal-toolsets-extension`.

Claude, Codex, and other MCP users should all use this same Unreal plugin
repository and its release assets.

## Repository boundary

The standalone repository should contain:

- `UnrealToolsetsExtension.uplugin`
- `Source/`
- `Config/`
- `.github/workflows/`
- `scripts/`
- `docs/`
- `LICENSE`
- release notes and packaging scripts

Do not include:

- Epic Engine or official plugin files
- `Intermediate`, `Saved`, Derived Data Cache, or host projects
- a private `.uproject`, `.mcp.json`, user paths, or MCP credentials
- binaries for an Engine version or platform that was not built and tested
- client marketplace metadata or client-specific runtime naming

Keep the Unreal plugin identifier, module, schemas, errors, archives, and
release titles client-neutral. Client-specific setup belongs in separate
documentation sections.

## Version and tag policy

- Plugin semantic version: `0.3.0`
- Git tag: `unreal-toolsets-v0.3.0`
- Release title: `Unreal Toolsets Extension 0.3.0`
- Binary asset:
  `UnrealToolsetsExtension-0.3.0-UE5.8.1-Win64.zip`
- Checksum:
  `UnrealToolsetsExtension-0.3.0-UE5.8.1-Win64.zip.sha256`

Use a new plugin version when reflected schemas or runtime behavior change.
Rebuilding the same source for another Engine or platform may add a distinctly
named asset to the same release after that combination passes the complete
validation matrix.

## Prepare release assets

Run from a clean checkout at the exact commit that the Git tag will reference:

```powershell
$commit = git rev-parse HEAD

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\build-unreal-extension-github-release.ps1 `
  -EngineRoot "C:\Program Files\Epic Games\UE_5.8" `
  -OutputDirectory "C:\release\unreal-toolsets-v0.3.0" `
  -SourceCommit $commit `
  -EngineVersionLabel "UE5.8.1" `
  -TargetPlatforms "Win64"
```

The script refuses to overwrite an output or staging directory. It produces a
ZIP with a top-level `UnrealToolsetsExtension` folder, a SHA-256 file, and a
machine-readable manifest that binds the asset to the source commit, plugin
version, Engine label, and target platform.

## Release gate

Before publishing:

1. Confirm the source commit is clean, reviewed, and tagged.
2. Run repository tests and official Skill/Plugin validation where applicable.
3. Run Unreal `BuildPlugin` from a clean staging package.
4. Install the binary package into a disposable project.
5. Run the complete `AI.UnrealToolsetsExtension` automation group.
6. Confirm the packaged plugin registers `LandscapeExtensionToolset`,
   `LandscapeGrassExtensionToolset`, `PlacementExtensionToolset`, and
   `PCGExtensionToolset`, `FabExtensionToolset`, and
   `ObservabilityExtensionToolset`.
7. Discover all six toolsets through a live Unreal MCP server.
8. Exercise Landscape inspection and mutations on disposable test data,
   including invalid source, invalid resolution, rollback, save failure, and
   World Partition refusal cases.
9. Exercise negative-height ground snap, self-collision exclusion, accepted
   hit filtering, and no-hit failure behavior on disposable Actors.
10. Exercise weighted PCG mesh entry CRUD and audit at least one generated
    non-empty HISM result against a disposable Landscape.
11. Exercise Landscape Grass variety CRUD and density-scaling changes on a
    disposable asset, including invalid mesh, index, and range cases.
12. Exercise Fab integration, empty and populated cache inspection, invalid
    cache ids, and registered/unregistered import-type queries. Confirm that
    responses expose no cookies, tokens, authentication state, entitlements,
    marketplace responses, or download controls.
13. Exercise the observability information query and confirm its version,
    operation catalog, safe-error catalog, and client-neutral privacy boundary.
14. Compare the ZIP hash with its `.sha256` file.
15. Confirm the ZIP contains no `Intermediate`, `Saved`, private paths, project
    assets, or official Epic plugin files.

Do not advertise an Engine or platform as supported until all applicable gates
pass for that combination.

## Publish with GitHub CLI

Create the release only after GitHub authentication and repository ownership
have been confirmed:

```powershell
gh release create unreal-toolsets-v0.3.0 `
  --repo <owner>/unreal-toolsets-extension `
  --title "Unreal Toolsets Extension 0.3.0" `
  --notes-file .\docs\releases\unreal-toolsets-v0.3.0.md `
  C:\release\unreal-toolsets-v0.3.0\*.zip `
  C:\release\unreal-toolsets-v0.3.0\*.sha256 `
  C:\release\unreal-toolsets-v0.3.0\release-manifest.json
```

For a first public release, create it as a draft, inspect the rendered notes and
assets, then publish it. Do not upload from a dirty checkout or silently replace
an existing binary under the same asset name.

## GitHub Actions

Repository-only tests can run on GitHub-hosted runners. Unreal binary packaging
must run on a controlled Windows runner that has the declared Unreal Engine
version and toolchain installed.

Never run untrusted pull-request code on that self-hosted Unreal runner. Limit
binary release jobs to protected tags or manually approved workflows, use a
GitHub Environment for release approval, and grant `contents: write` only to
the release job.

The included workflows implement that split:

- `.github/workflows/validate.yml` runs repository tests on
  `windows-latest` and never invokes Unreal.
- `.github/workflows/release-unreal-toolsets.yml` runs only when manually
  dispatched with a `unreal-toolsets-v*` tag selected. It requires the
  `unreal-toolsets-release` Environment, a self-hosted runner labeled
  `Windows`, `X64`, and `unreal-engine-5.8`, and a repository variable named
  `UE_5_8_ROOT`.

Configure required reviewers on the `unreal-toolsets-release` Environment.
The workflow creates a draft GitHub Release so a maintainer can inspect its
notes, ZIP, checksum, and manifest before publication.
