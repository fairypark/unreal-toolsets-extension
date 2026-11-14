# Unreal Toolsets Extension 0.5.0

This beta release provides two Win64 Editor packages. Select the ZIP that
matches your Editor's version and changelist.

| Unreal Editor | Changelist | Bundled Fab |
| --- | --- | --- |
| 5.8.4 | 58902052 | 0.0.21 |
| 5.9.0 Preview | 60351404 | 0.0.22 |

## Changes

- MCP discovery recognizes the singular UE 5.8 and plural UE 5.9 description
  calls. All 40 extension operations declare their read or write access mode.
- Fab capability inspection reports the official owned-library search when
  the host exposes it. The legacy snapshot adapter still requires UE 5.8
  with Fab 0.0.14.
- Visual capture failures report outstanding compilation and streaming work.
  The automation fixture waits for idle draws before capturing the test camera.
- Each ZIP carries the installed Engine patch and changelist in its filename.
  Its matching manifest identifies the Git commit and SHA-256. Packaging
  checks for the requested Editor binaries and excludes Windows debug symbols.

Extract the matching ZIP under your project's `Plugins` directory and restart
the Editor. The 5.9 package targets the preview changelist listed above.
Online Fab account operations and packaged games are outside this release's
validation scope.
