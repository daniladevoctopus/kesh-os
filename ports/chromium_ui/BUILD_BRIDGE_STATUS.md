# KeshOS OpenFyde build bridge

This file is a lightweight trigger/status marker for the self-hosted build loop.

Current code under test: `a49cc2af92e86d24a1a5ad5b414f6421e5b65015`

Latest resolved GN blockers:
- FydeOS ChromeOS-only switches leaking into non-ChromeOS `google_apis`.
- Blink-only Device USB test dependency while `use_blink=false`.
- Blink-only mojom dependency overrides being rejected when only the C++ mojom variant is generated.

The build target remains `//keshos/ozone:kesh_smoke` before packaging the full KeshOS ISO.
