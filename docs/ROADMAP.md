# Roadmap

The roadmap is capability-driven rather than client-driven. A proposed toolset
is accepted only when it fills a verified gap in Epic's typed tools and can be
implemented against public Unreal APIs.

## Available

### LandscapeExtensionToolset

- Landscape inspection and height statistics
- Landscape creation in non-World-Partition worlds
- Heightmap import and guarded reimport
- Weightmap import
- Landscape material assignment
- Building-pad validation
- Landscape package persistence

## Under evaluation

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
