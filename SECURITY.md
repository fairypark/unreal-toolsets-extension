# Security policy

## Reporting

Do not disclose suspected vulnerabilities, credential exposure, arbitrary file
writes, unsafe package deletion, or destructive Editor behavior in a public
issue. Use GitHub's private vulnerability reporting feature for this
repository.

Include the affected plugin version, Unreal Engine version, platform,
reproduction steps, and whether the issue can mutate project or Engine files.
Do not include Fab, Epic, GitHub, MCP, or other service tokens in a report.

## Supported versions

Security fixes are provided for the latest published plugin release. Engine and
platform combinations are supported only when a matching GitHub Release asset
and compatibility statement exist.

## Trust boundary

This plugin executes inside Unreal Editor and can expose mutating operations
through compatible MCP servers. Treat MCP access as local privileged Editor
access. Bind servers only to intended interfaces, review every mutating tool,
and use source control before bulk changes.

The plugin must never collect or expose marketplace credentials, browser
cookies, refresh tokens, payment information, or private MCP configuration.
