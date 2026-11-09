# MCC upstream extraction and validation

This branch is reconstructed from the completed refactor snapshot listed below,
on OpenCE main `fc9deb0b10eaae1d62caedb0710968b4e77e6e16`.
The exact source snapshot is `3d26678bf837335fb66a225c9c2fe1ad8626409c`.
The earlier [refactor ledger](mcc_refactor_validation.md) retains historical
format evidence and bounded runs on that source branch. Those runs do not
certify this extraction.

## Scope and ownership

MCC loaders, tags, script adapters, package discovery, rendering conversions,
menus, campaign manifests, sharing and browser storage remain independently
owned. Xbox `maps`, CE `custom_maps` and MCC `mcc_maps` remain separate.
The contribution omits Gulch's Modern/stylised menus, keyboard glyphs, optional
shoulder camera, lighting and unrelated fork changes. Existing upstream
controller glyph support is retained; an optional texture-release callback
allows MCC-owned glyph textures to be released before their bitmap headers.

OpenCE network agreement increases from 41 to 42. There is one protocol for
all map families, not a second MCC compatibility envelope. Join requests carry
the version before authentication state can change. Shared settings include
MCC identities (zero for legacy maps), with exact-size reassembly. Checkpoint
epochs, object recovery and cinematic poses are agreed shared state.
MCC transfers retain host authority, joined-peer admission, content identities,
bounds and both download opt-ins. See [NETCODE](../port/linux/NETCODE.md).

Shared fixes carried from the refactor correct existing contracts: initialize
all boat controls, preserve console syntax until its runtime consumers finish,
recover co-op state across checkpoints, synchronize scripted player poses,
retain the caller's GL texture binding when allocating a render target, and
join owned networking workers during shutdown. They are not map-name patches.
Minimal shared MCC hooks are necessary where the engine owns dispatch, tag
loading, rendering, widget lifetime or network message delivery; the actual
compatibility policies stay in MCC modules.

The stock selector reuses existing difficulty indicators for MCC completion.
The journal cache is keyed by profile and full mission identity, expires after
one second, and retains no widget/catalog pointer. Folder and MP entries do
not show campaign awards. Generated Back handlers unwind MCC selection state.

## Local evidence so far

Windows portable release, PGO/LTO disabled, clang 23, original Workshop inputs:

- Xbox b30 and Ruby's Rebalanced b30: two native peers, natural opening,
  61-second paired gameplay windows (122 position comparisons each), no judged
  gameplay divergences/problems, then engine-confirmed checkpoint and revert.
  Both processes stopped; original map hashes were unchanged. This does not
  establish complete mission or campaign compatibility.
- The earlier run labelled `upstream_ruby_b30_natural` actually staged Ruby a10;
  its old helper ignored the mission argument. Its drift-only result is not
  used as paired b30 evidence. The helper now records the real level and rejects
  zero paired position samples.
- Broad MCC suite: 2,502 passed, 13 skipped, one transient sharing fixture
  failure before the tested admission-revision mutation. Added failure-state
  diagnostics; all 69 sharing cases and 100 isolated repetitions subsequently
  passed. The original failure is retained and is not claimed fixed without
  a reproduced cause. A final broad run remains required.
- Stock menu, journal, texture ownership and preview checks pass. The glyph
  ownership test demonstrated a cache-hit release-order defect before the fix
  and passes after retaining the MCC callback on reused glyphs.
- Windows and web builds passed during integration. Seventy browser JS tests
  passed. Real-browser and final platform validation are still in progress.
- The full harness initially encountered Windows CRT deprecation errors under
  `-Werror`; the Windows harness now defines `_CRT_SECURE_NO_WARNINGS`, matching
  the game build, without changing the functions being tested.

## Xbox a10 baseline assertion

The client assertion predates this refactor. It reproduces on the untouched
pre-Stage-1 build (`db1697c9b0c46289596f22213a5bef8ee327be24`, executable SHA-256
`f1ec0dca876aa3cf746a4095ac9115cc026902f4679d1c3941deb20928c85d56`) and pristine
OpenCE `fc9deb0` (executable SHA-256
`7f3106e7b69cf637e8cffaf99783685acfe1646c289ac5aeccdf928336d71605`).
Both used the same Xbox a10 SHA-256
`f7461e5b6eaf0314c4f03a1c6e43752b06ae3e4782b70b240a8c15e5f634b3ce`.

A pristine diagnostic build observed `tick=3362 bsp_index=8 cluster=2 count=2`
at `players_compute_combined_pvs`'s activating-cluster branch. Cluster 2 is
outside that BSP's two-cluster range. This is the activating-cluster field,
not the player's ordinary cached cluster. `objects_port_set_activating_cluster`
checks the range when setting it, but does not retain BSP identity. A stale
activation across a BSP transition is a plausible explanation; the exact
last setter/transition ordering has not been established. Upstream commit
`8e6b9a211e27e7439186c64cea77e1041b30f05a` introduced this path; it has not been
proved to be the introducing commit of the assertion.

A later attempted diagnostic linked pristine sources with modified objects;
its failed replication makes it invalid baseline evidence and it is excluded.
No production workaround was added. Under the agreed gate (fix if introduced
by this work), this is an upstream follow-up, not a passing a10 co-op result.

## Remaining completion gates

Final combined review, Linux/Windows/Android/web CI, the final focused suites,
real-browser import/storage and native/browser gameplay/sharing validation,
artifact cleanup, final-commit Windows package and reproducible line counts.
