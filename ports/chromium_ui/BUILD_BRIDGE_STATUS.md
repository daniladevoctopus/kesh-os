# KeshOS OpenFyde build bridge

This file is a lightweight trigger/status marker for the self-hosted build loop.

Current code under test: `44248c93ece154959bf79797e80506ea147b0356`

Latest resolved GN blockers:
- FydeOS ChromeOS-only switches leaking into non-ChromeOS `google_apis`.
- Blink-only Device USB test dependency while `use_blink=false`.
- Blink-only mojom dependency overrides, visibility and source dependencies being evaluated when the Blink variant is disabled.

The build target remains `//keshos/ozone:kesh_smoke` before packaging the full KeshOS ISO.
