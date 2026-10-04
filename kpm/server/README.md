# KPM repository v2

The registry publishes immutable signed KEA v2 packages from this tree:

```text
index-v2.json
keys/root-public.json
packages/<package-id>/<version>/<package-id>.kea
```

The private root key is intentionally kept outside the repository. On the current development machine it is stored at `~/.config/keshos/kpm-root-private.json` with mode `0600`.

Create a new trust root:

```sh
python3 tools/kpm-repo.py keygen \
  --private ~/.config/keshos/kpm-root-private.json \
  --trust-header kpm/trusted_keys.c
```

Pack a signed release:

```sh
KPM_SIGNING_KEY=~/.config/keshos/kpm-root-private.json \
python3 tools/kea-pack.py --elf build/apps/notepad.elf \
  --out release/notepad.kea --name Notepad --ver 1.0.0
```

Publish all packages from a release directory:

```sh
python3 tools/kpm-repo.py publish --source release --repository kpm/server \
  --key ~/.config/keshos/kpm-root-private.json
```

The publisher verifies every package before copying it and regenerates the repository index from signed package metadata. The API accepts only normalized package IDs and versions, resolves downloads through the index and rejects size mismatches.

Create and publish a signed system update:

```sh
python3 tools/kesh-update.py --image build/kernel.elf \
  --out kpm/server/system/current.ksu --release 2026.09.30 \
  --signing-key ~/.config/keshos/kpm-root-private.json
```

KSU bundles use the same pinned root key as KEA packages. KPM stages them into the inactive A/B slot and records a pending boot with a bounded attempt counter.
