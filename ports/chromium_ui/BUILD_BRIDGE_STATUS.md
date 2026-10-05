# KeshOS OpenFyde build bridge

This file is a lightweight trigger/status marker for the self-hosted build loop.

Current code under test: `31893aa3955713fe387da939e54453a3639aaa41`

Latest resolved GN blockers:
- FydeOS ChromeOS-only switches leaking into non-ChromeOS `google_apis`.
- Blink-only Device USB test dependency while `use_blink=false`.
- Blink-only mojom dependency overrides, visibility and source dependencies being evaluated when the Blink variant is disabled.
- Lens importing `//chrome` in the no-Blink UI-only configuration.

The build target remains `//keshos/ozone:kesh_smoke` before packaging the full KeshOS ISO.
