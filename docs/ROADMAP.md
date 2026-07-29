# Roadmap

The roadmap is capability-driven rather than client-driven. A proposed toolset
is accepted only when it fills a verified gap in Epic's typed tools and can be
implemented against public Unreal APIs.

## Available

### LandscapeExtensionToolset

- Landscape inspection and height statistics
- Landscape creation in non-World-Partition worlds
- Heightmap import and guarded reimport
- atomic heightmap plus layer-map preflight, dry-run, backup, and reimport
- Weightmap import
- Landscape material assignment
- Building-pad validation
- read-only streaming-proxy audit
- guarded, explicitly confirmed streaming-proxy grid rebuild
- Landscape package persistence

### PlacementExtensionToolset

- diagnostic ground snapping with self and attached-Actor exclusion
- negative-world-height support
- optional Landscape preference or Landscape-only filtering
- Actor-bounds-bottom alignment and structured hit diagnostics

### PCGExtensionToolset

- typed weighted Static Mesh Spawner entry read/replace/add/update/remove/reorder
- index, mesh, weight, collision profile, cull-distance, and navigation validation
- read-only graph-instance audit over managed ISM/HISM output
- per-mesh instance count and sampled Landscape coverage/grounding diagnostics
- explicit empty-output findings without graph execution or cleanup

### LandscapeGrassExtensionToolset

- typed Landscape Grass variety read/replace/add/update/remove/reorder
- mesh, density, jitter, cull-distance, LOD, and scale-range validation
- transactional density-scaling control
- package-dirty mutation without implicit asset saves

## Under evaluation

### Landscape and placement follow-ups

- disposable World Partition fixtures for full proxy-rebuild rollback coverage
- RAW16/RAW8 reimport fixtures for exact batch-write and rollback tests
- source-control refusal and post-save failure injection
- live MCP schema and mutation verification on each supported Engine build
- generated PCG fixture coverage for non-empty HISM output and Landscape ratios
- Landscape material Grass Output binding and shader-recompile fixtures

### FabExtensionToolset

The first viable Fab scope is read-only integration and cache inspection:

- `GetFabIntegrationInfo`
- `GetFabCacheSummary`
- `ListFabCachedEntries`
- `GetFabCacheEntryStatus`
- `GetFabImportTypeSupport`

Marketplace search, My Library synchronization, ownership checks, signed URL
generation, Add to Project, and plugin installation are not planned until Epic
provides stable public service interfaces. The extension will not include
private Fab headers, reflect private browser bridges, collect tokens or
cookies, or represent a caller-supplied URL as an ownership-verified download.

Cache deletion may be considered later with explicit expected-path,
expected-size, and expected-entry-count guards.

## Candidate domains

- richer World Partition validation and streaming diagnostics
- deterministic level-layout validation
- asset provenance and dependency auditing through public Asset Registry APIs
- missing typed import and reimport operations that do not duplicate
  Interchange or AssetTools capabilities

Every Engine release requires a fresh overlap audit. When Epic provides an
equivalent official tool, the extension API should be deprecated and migrated.
