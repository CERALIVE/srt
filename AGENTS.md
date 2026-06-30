# srt

Parent: [`../AGENTS.md`](../AGENTS.md)

## ROLE IN THE GROUP

`srt` is a **build-time vendored libsrt source** — a CERALIVE fork of
[Haivision/srt](https://github.com/Haivision/srt).

- **SONAME:** `libsrt.so.1.5`
- **Consumers at compile time:** `cerastream` (libsrt FFI link) and `srtla`
- **NOT a `.deb`.** NOT in `REPOS`. Reclassified 2026-06-16 from a prior
  (never-implemented) first-party `.deb` claim.
- **Runtime libsrt on device:** the system package `libsrt1.5-openssl` from
  Debian bookworm `main` — installed by the image runtime OS layer. There is no
  CERALIVE libsrt fork `.deb`. `cerastream` declares `Depends: libsrt1.5-openssl`
  directly; this vendored source is build-time only.
- `irl-srt-server` uses system libsrt (deployment-dependent version), not this fork.

Cross-check: `versions.yaml` entry `srt` — `kind: vendored-transport-src`, no
`arch`/`depends`/`provides` keys, `pin: latest`.

## UPSTREAM-FORK RELATIONSHIP

This is upstream Haivision code. **Keep functional changes minimal** and clearly
marked as CERALIVE additions.

- `srt/CONTRIBUTING.md` is MPLv2.0 upstream — MUST NOT be modified.
- Remote: `origin https://github.com/CERALIVE/srt` — do NOT add a remote pointing
  at `Haivision/srt` or any other fork parent (Rule C). Upstream commits are
  fetched transiently by SHA, never via a retained remote.
- No CERALIVE CI badge: no CI workflow exists for this vendored fork. Do not add one.
- No `.github/dependabot.yml`: adding it would churn upstream's action pins and is
  out of scope for a vendored build-time source.

### Branches

| Branch | Base | Purpose |
|--------|------|---------|
| `master` | BELABOX-merge lineage (legacy) | Historical CERALIVE/BELABOX fork; **not** pinned for new builds |
| `reorderfreeze-1.5.5` | Haivision `1e4c908` (v1.5.5 + upstream fixes) | Clean reset to upstream + the single sanctioned CERALIVE patch (`SRTO_REORDERFREEZE`). This is the branch consumed by the cloud receiver build |

`reorderfreeze-1.5.5` deliberately **sheds** the old BELABOX C-source patches
(unconditional reorder-tolerance freeze, periodic-NAK disable, `iMaxReorderTolerance`
TTL override) and re-introduces *only* the reorder-tolerance decay freeze, now as an
opt-in socket option decoupled from `SRTO_NAKREPORT`.

## SANCTIONED CERALIVE PATCH — `SRTO_REORDERFREEZE`

A receiver-side, **default-off**, opt-in socket option that freezes the dynamic
reorder-tolerance **decay**. It exists because SRTLA delivers packets out of order
by design (traffic is balanced across bonded links); the stock adaptive decay drives
the tolerance toward 0 and causes spurious retransmissions on a healthy bonded path.

- **Enum:** `SRTO_REORDERFREEZE = 120` in `srtcore/srt.h` — appended HIGH (never
  gap-filled) to avoid colliding with future upstream option numbers.
- **Config:** `bool CSrtConfig::bReorderFreeze` (default `false`), set via
  `CSrtConfigSetter<SRTO_REORDERFREEZE>` (mirrors `SRTO_LOSSMAXTTL`).
- **Restriction:** `SRTO_R_PRE` (set before connect/listen). Inherited by accepted
  sockets via the wholesale `m_config` copy in the accept path — no `private_default`
  reset entry, so a listener's value propagates to every accepted socket.
- **Effect:** when `true`, gates the two reorder-tolerance **decay** sites in
  `srtcore/core.cpp` (`m_iConsecOrderedDelivery >= 50` and
  `m_iConsecEarlyDelivery >= 10`). Each gated line is tagged `// CERALIVE reorder-freeze`.
- **Scope discipline:** decay-disable ONLY. It does NOT touch `initial_loss_ttl`
  (upstream already initializes reorder tolerance to max), does NOT touch
  `SRTO_NAKREPORT` (orthogonal), and does NOT port BELABOX's
  `initial_loss_ttl = iMaxReorderTolerance` change.
- **Side:** receiver-side only; a no-op on senders.
- **Tests:** `test/test_socket_options.cpp` — `ReorderFreezeFreezesDecay`
  (option on: tolerance holds at max over a clean ordered stream; `SRTO_NAKREPORT`
  keeps its independently-set value) and `ReorderFreezeDefaultDecays`
  (default off: stock decay still reduces the tolerance — truly opt-in).

Any other functional change to the C/C++ source remains out of scope (see SCOPE
BOUNDARY). To bump the libsrt version consumed by `cerastream`/`srtla`, update the
`srt` `pin:` in `versions.yaml` and re-vendor — do not open PRs against upstream C
source for unrelated features.

## BASELINE PATCH STATUS (ADR-002 "C is SAFE")

**`SRTO_REORDERFREEZE` is the only CERALIVE patch** on the `reorderfreeze-1.5.5`
branch. No other functional changes exist in the C/C++ source relative to upstream
Haivision `1e4c908` (v1.5.5 + upstream security/bug fixes).

ADR-002 verdict: **"C is SAFE"** — the C `srtla_rec` receiver is safe to keep
without any additional libsrt patch for baseline parity. No new patch is needed.

### Device-side FEC packet-filter — NOT compiled; deferred

`SRTO_PACKETFILTER` (the SRT packet-filter API that enables Forward Error
Correction) is **not compiled** into the CERALIVE fork. The FEC plugin is an
upstream feature that requires the packet-filter API to be enabled at build time
(`-DENABLE_PACKET_FILTER=ON`); the CERALIVE build does not set this flag.

**Deferral rationale:** FEC compilation is deferred until a FEC mixture is being
actively evaluated for gain (gain-hunt track). There is no current evidence that
FEC overhead would improve the bonded-SRTLA path — ARQ already handles loss
recovery on the bonded links. When a FEC gain-hunt is initiated, the correct
approach is to enable the packet-filter API in the build and evaluate FEC vs ARQ
tradeoffs on the actual bonded path before committing to a compile-time change.

Until then: do NOT enable `SRTO_PACKETFILTER` / `-DENABLE_PACKET_FILTER=ON` in
the CERALIVE build. This is an intentional deferral, not an oversight.

## RECEIVER CAPABILITY RECONCILIATION

Canonical decision record: [`docs/RECEIVER-RECONCILIATION.md`](../docs/RECEIVER-RECONCILIATION.md)

**Baseline patch status confirmed (Task 3, ADR-002 "C is SAFE"):** `SRTO_REORDERFREEZE`
is the only CERALIVE patch on `reorderfreeze-1.5.5`. No additional libsrt patch is
needed for BELABOX-parity baseline. The stock-libsrt substitution (`nakreport=0` +
`lossmaxttl=40`) is authorized by ADR-002 as a safe equivalent.

**Device-side FEC packet-filter — NOT compiled; deferred.** `SRTO_PACKETFILTER` /
`-DENABLE_PACKET_FILTER=ON` is not set in the CERALIVE build. Compilation is deferred
until a FEC mixture is actively being evaluated for gain (gain-hunt track). There is no
current evidence that FEC overhead improves the bonded-SRTLA path. Do NOT enable the
packet-filter flag until a FEC gain-hunt is initiated.

Cross-ref: [`srtla/docs/adr/ADR-002-srt-patch-necessity.md`](../srtla/docs/adr/ADR-002-srt-patch-necessity.md)

## SCOPE BOUNDARY

**No unsanctioned first-party feature work here.** The internals of this repo are
otherwise out of scope for CeraLive development. In-scope edits are limited to
`AGENTS.md` and the explicitly-sanctioned `SRTO_REORDERFREEZE` patch above.

## COMMON TASKS

Task-routing for the handful of legitimate CeraLive operations on this vendored
repo. Anything not listed is upstream Haivision work and out of scope (see SCOPE
BOUNDARY).

| I need to… | Do this |
|------------|---------|
| Bump the libsrt version `cerastream`/`srtla` link against | Edit the `srt` `pin:` in `../versions.yaml` and re-vendor — NOT a PR here |
| Build the library to test it standalone | [BUILD](#build) — `cmake -B build …` |
| Run the unit + bonding test suite | [TEST (ctest)](#test-ctest) |
| Find the source / build config / options | [WHERE TO LOOK](#where-to-look) |
| Touch the reorder-freeze option | See [SANCTIONED CERALIVE PATCH](#sanctioned-ceralive-patch--srto_reorderfreeze) — keep it decay-disable-only and decoupled from NAK |
| Update routing/build/test guidance | Edit this `AGENTS.md` |
| Confirm `srt` is NOT a `.deb` / not in `REPOS` | [ROLE IN THE GROUP](#role-in-the-group); runtime libsrt is system `libsrt1.5-openssl` |

There is **no CI** for this fork (intentional — see UPSTREAM-FORK RELATIONSHIP), so
the build/test commands below are the only gate; run them locally when validating a
re-vendor or a change to the sanctioned patch.

## BUILD

Standard CMake. Consumed by `cerastream` and `srtla` as a sibling checkout at
compile time — do not build standalone unless testing the library itself.

```bash
cmake -B build -DENABLE_TESTING=ON -DENABLE_UNITTESTS=ON -DENABLE_BONDING=ON
cmake --build build -j$(nproc)
```

## TEST (ctest)

The GoogleTest unit + bonding suite is gated on `ENABLE_UNITTESTS` (with
`ENABLE_CXX11`, ON by default); `ENABLE_TESTING` additionally builds the developer
test apps. Both are OFF by default. Run the full suite with:

```bash
cmake -B build -DENABLE_TESTING=ON -DENABLE_UNITTESTS=ON -DENABLE_BONDING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Baseline on `reorderfreeze-1.5.5` (Haivision `1e4c908` + the `SRTO_REORDERFREEZE`
patch): the full gtest suite passes (1 disabled: `CTimer.SleeptoAccuracy`). Note:
`ENABLE_TESTING=ON` alone registers no ctest tests — `ENABLE_UNITTESTS=ON` is what
wires the gtest suite into ctest.

## WHERE TO LOOK

| Need | Location |
|------|----------|
| Library source | `srtcore/` |
| Reorder-freeze option enum | `srtcore/srt.h` → `SRTO_REORDERFREEZE` |
| Reorder-freeze config field / setter | `srtcore/socketconfig.h` / `srtcore/socketconfig.cpp` |
| Reorder-freeze decay gates | `srtcore/core.cpp` → `// CERALIVE reorder-freeze` |
| Build config | `CMakeLists.txt` |
| Build options reference | `docs/build/build-options.md` |
| License | `LICENSE` (MPLv2.0) |
| versions.yaml entry | `../versions.yaml` → `srt:` |
