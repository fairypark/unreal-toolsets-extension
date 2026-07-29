# Contributing

Contributions should fill a verified gap in Epic's public Unreal toolsets
without modifying Engine files or duplicating an existing typed operation.

Before opening a pull request:

1. Check the supported Unreal versions for an equivalent official tool.
2. Open an issue describing the missing capability, intended users, public
   Unreal APIs, safety model, and expected tests.
3. Keep each domain in its own `U<Domain>ExtensionToolset` class.
4. Use static `AICallable` functions with reflected request and result types.
5. Validate targets and inputs before mutation, save recovery packages, and
   verify postconditions.
6. Avoid private Engine headers, UI implementation classes, authentication
   tokens, project-specific paths, and client-specific runtime behavior.
7. Add repository tests and Unreal automation coverage.

Run repository tests:

```powershell
python -m unittest discover -s tests -v
```

Build a clean Unreal package:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\package-unreal-extension.ps1 `
  -EngineRoot "C:\Program Files\Epic Games\UE_5.8" `
  -OutputDirectory "C:\tmp\UnrealToolsetsExtension"
```

See `docs/TOOLSETS_EXTENSION_GUIDELINES.md` for the complete design and review
contract.
