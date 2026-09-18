# srt

Parent: [`../AGENTS.md`](../AGENTS.md)

## ROLE IN THE GROUP

`srt` is the CeraLive runtime fork of [Haivision/srt](https://github.com/Haivision/srt).

- **SONAME:** `libsrt.so.1.5`
- **Runtime package:** `libsrt1.5-ceralive` for arm64 and amd64, built by
  `packaging/build-deb.sh` with the GnuTLS backend. Current source version
  **`1.5.7+ceralive.1`** (upstream v1.5.7 true-merged, incl. the KMREQ, ACK, DROPREQ,
  FEC and bonding hardening); the latest *published* package is `1.5.6+ceralive.1`
  until the owner tags `srt-v1.5.7+ceralive.1`. The `+`-bearing version is
  deliberate — see the packaging contract below.
- **Device use:** image-building-pipeline stages it from apt.ceralive.tv and
  installs it before `cerastream`. It replaces the Debian GnuTLS/OpenSSL flavors
  and provides their virtual package names; GStreamer and cerastream resolve one
  CeraLive `libsrt.so.1.5` ABI in each process.
- **Consumers:** `cerastream` (direct FFI) and GStreamer runtime components.
- `irl-srt-server` uses system libsrt (deployment-dependent version), not this fork.

### Bundled SRT tools (`srt-live-transmit`, `srt-file-transmit`, `srt-tunnel`)

`libsrt1.5-ceralive` also ships the SRT sample command-line tools in `/usr/bin`
(built with `-DENABLE_APPS=ON`). `srt-live-transmit` is the one the device path
needs (SRT↔UDP relay); the other two ride along because they share the same build
and dependencies. They are the single first-party source of `srt-live-transmit` —
no separate `srt-tools`-style package exists (see below for why).

**Single-fork invariant is preserved.** The tools are built with
`-DENABLE_STATIC=OFF -DENABLE_SHARED=ON`, so they dynamically link the *same*
shared `libsrt.so.1.5` shipped in this package (GnuTLS via `USE_ENCLIB=gnutls`).
They pull in **no** OpenSSL and no second libsrt — verified in CI: the package
build asserts `srt-live-transmit`'s `NEEDED` includes `libsrt.so.1.5` and excludes
`libssl`/`libcrypto` (`publish-release.yml`), and `verify-runtime-replacement.sh`
re-checks the installed binary's `ldd`. `package-contract.sh` locks
`ENABLE_APPS=ON` + `ENABLE_STATIC=OFF` so the tools can never silently drop out or
gain a static/second-flavor libsrt.

### Runtime-package version contract (#20)

`packaging/package-contract.sh` is a STATIC gate — it reads the packaging sources
and workflows, so it runs without building. It locks, in addition to the
`ENABLE_APPS`/`ENABLE_STATIC` flags above:

| Locked | Current value |
|--------|---------------|
| `.deb` version | `1.5.7+ceralive.1` (`CERALIVE_SRT_VERSION` default in `build-deb.sh`) |
| `Provides` | `libsrt1.5-gnutls (= 1.5.7)`, `libsrt1.5-openssl (= 1.5.7)` |
| `Conflicts` / `Replaces` | `libsrt1.5-gnutls`, `libsrt1.5-openssl` |
| SONAME | `libsrt.so.1.5` |
| No stale pin | neither `publish-release.yml` nor `runtime-package.yml` may retain a `1.5.5` or `1.5.6` reference |

The versioned `Provides` is what lets this package *replace* both Debian TLS
flavours rather than co-install beside them — the single-fork invariant depends on
the `(= 1.5.7)` upstream version matching what Debian's flavours would satisfy, so
the package version and the `Provides` version move together but are NOT the same
string (`1.5.7+ceralive.1` vs `1.5.7`). The release version is decided once per
cutover and recorded in the sender repo's `docs/evidence/bpc/srt-release.json`
(`srt_release_version`, `srt_tag`, `deb_assets`); packaging and docs read from that
record rather than inventing a number.

**The `+` is load-bearing downstream.** apt's https method percent-encodes `+` as
`%2B` in the fetch URL while R2 stores the literal `+`, which 404'd every
`+`-versioned `.deb` on-device until `apt-worker` #23 keyed lookups on the decoded
path. Keep that in mind before assuming a `+` version is "just cosmetic": see
`apt-worker/AGENTS.md` → "R2 keys are matched on the percent-DECODED path".

**Why bundled, not a separate `srt-tools-ceralive` package.** The apt publish path
(`apt-worker/scripts/reindex.sh`) downloads *every* `.deb` in a release tag and
hard-fails (`validate_deb`, G-B) if any package name ≠ the dispatched `COMPONENT`,
and its `VALID_COMPONENTS` allowlist has no tools package. A separate package would
therefore require an apt-worker change (allowlist + per-package reindex) plus an
image-pipeline `FIRST_PARTY_APT_PKGS` entry. Bundling keeps the whole change inside
`srt`: one package, the existing `libsrt1.5-ceralive` component, the existing
release + `apt-reindex` dispatch — all unchanged. The package already shipped
`/usr/bin/srt-ffplay` and dev headers, so it was never a pure shared-object package.
Consequence for the image: once the device installs `libsrt1.5-ceralive`,
`srt-live-transmit` is present with no new package to fetch or pin.

Cross-check: root `versions.yaml` entry `srt`, image
`v2/lib/fetch-debs.sh`, and `cerastream` packaging must all name the same release.

## UPSTREAM-FORK RELATIONSHIP

This is upstream Haivision code. **Keep functional changes minimal** and clearly
marked as CERALIVE additions.

- `srt/CONTRIBUTING.md` is MPLv2.0 upstream — MUST NOT be modified.
- Remote: `origin https://github.com/CERALIVE/srt` — do NOT add a remote pointing
  at `Haivision/srt` or any other fork parent (Rule C). Upstream commits are
  fetched transiently by SHA, never via a retained remote.
- `runtime-package.yml` validates the Debian artifact and its real GStreamer
  replacement behavior on every packaging/source PR.
- No `.github/dependabot.yml`: adding it would churn upstream's action pins and is
  out of scope for a vendored build-time source.

### Branches

| Branch | Base | Purpose |
|--------|------|---------|
| `master` | Haivision v1.5.7 + CeraLive runtime changes | The maintained CeraLive runtime line and source of released device packages |

`reorderfreeze-1.5.5` is neither a branch nor a tag in CERALIVE/srt or
Haivision/srt; do not use it as a checkout base. The CeraLive line was reset at
`66b3609` to Haivision `1e4c908` and retains only the sanctioned socket-option
patches below, not the old BELABOX C-source patches (unconditional
reorder-tolerance freeze, periodic-NAK disable, or the
`iMaxReorderTolerance` TTL override).

### ABI baseline

`1.5.7+ceralive.1` merges Haivision **v1.5.7** (`899348d`) as a true merge
(`379d129`), retaining `SRTO_REORDERFREEZE = 120`, deterministic socket teardown,
and adding `SRTO_PERIODICNAKGATE = 119`. It brings upstream handshake, ACK, DROPREQ,
FEC, bonding and sample-tool hardening.

`.github/workflows/abi.yml` compares proposed builds with the immutable
`srt-v1.5.6+ceralive.1` tag: the latest *published* CeraLive source release. This
tests compatibility against the ABI that device consumers actually received,
without depending on a fork-external or absent ref. `1.5.7+ceralive.1` was checked
against it (workflow-equivalent Debug/BONDING/PKTINFO/MAXREXMITBW build, abi-dumper
1.2, abi-compliance-checker 2.3): 100% binary and source compatible, zero problems.
Advance this baseline only after `srt-v1.5.7+ceralive.1` is published, in its own
PR; never in the release PR itself, and never by silencing the lane. If the lane
ever fails because of a fork option, fix the option.

## SANCTIONED CERALIVE PATCHES — `SRTO_REORDERFREEZE`, `SRTO_PERIODICNAKGATE`

Two receiver-side, **default-off**, opt-in socket options. Both exist because SRTLA
delivers packets out of order by design (traffic is balanced across bonded links),
and stock libsrt's loss handling treats that reordering as loss: the adaptive
reorder-tolerance decay drives the tolerance toward 0, and the periodic NAK
re-reports every late packet as if it were lost. Each option fixes exactly one of
those two mechanisms and nothing else. Canonical inventory with commit SHAs:
[`docs/CERALIVE-PATCHES.md`](docs/CERALIVE-PATCHES.md).

**Fork-reserved value band 111-120, allocated DOWNWARD from 120.**
`SRTO_REORDERFREEZE = 120` is the band top and stays the lexically last member;
`SRTO_PERIODICNAKGATE = 119`; 111-118 are reserved for future fork options (next
free value below the last). The top is frozen because `SRTO_E_SIZE` is upstream's
auto-valued public sentinel and `srt-v1.5.6+ceralive.1` shipped it as `121`:
appending above 120 moves it, which `abi-compliance-checker` rates a Medium binary
problem and fails `abi.yml` (measured on the first `1.5.7+ceralive.1` candidate at
`= 121`: 97.2%, one problem; at `= 119`: 100%). Never place a fork option above
120 and never gap-fill upstream's range. **Collision policy:** if upstream ever
assigns a value inside 111-120, the fork renumbers its own option at the next
`+ceralive` release (announced in `docs/CERALIVE-PATCHES.md` and the release notes;
`abi.yml` is expected to flag it). The upstream value is never shadowed.

**Upstream-proposal status:** neither option has been proposed to Haivision/srt;
both are deferred until the bonded-path convergence campaign yields field evidence
on a real bonded ingest. The scope discipline below is what keeps each patch a
self-contained, upstreamable diff.

### First sanctioned option — `SRTO_REORDERFREEZE`

Freezes the dynamic reorder-tolerance **decay**.

- **Enum:** `SRTO_REORDERFREEZE = 120` in `srtcore/srt.h`.
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

### Second sanctioned option — `SRTO_PERIODICNAKGATE`

Gates **periodic** NAK re-reports by the packet-count reorder window.

- **Enum:** `SRTO_PERIODICNAKGATE = 119` in `srtcore/srt.h`; URI `periodicnakgate`
  (the sibling URI row `reorderfreeze` was added in the same commit).
- **Config:** `bool CSrtConfig::bPeriodicNakGate` (default `false`), setter in
  `socketconfig.cpp`, getter in `core.cpp`, plus `group.cpp::getOptDefault`
  (`RD(false)`) so an empty group answers the default.
- **Restriction:** `SRTO_R_PRE`; inherited by accepted sockets exactly like
  `SRTO_REORDERFREEZE`.
- **Effect:** when `true`, `checkNAKTimer` (`srtcore/core.cpp`, block tagged
  `// CERALIVE periodic-nak-ttl`) subtracts the `m_FreshLoss` ranges from the loss
  array before building the periodic report, so only losses that have outlived the
  reorder window are re-reported. Ports onsmith/srt `b5690bc` (2026-07-05) without
  its bundled `SRTLAPATCHES` switch; a static comparator replaces the upstream lambda
  to keep C++03.
- **Scope discipline:** periodic-report gating ONLY. It does NOT change the
  immediate loss report, `unlose`, NAK scheduling or the LiveCC interval floor; does
  NOT add a wall-clock delay or a new tunable; does NOT touch reorder-tolerance decay
  (that is `SRTO_REORDERFREEZE`); default-off still reports the whole loss list,
  byte-for-byte as upstream. Independent of both `SRTO_REORDERFREEZE` and
  `SRTO_NAKREPORT` (all eight bool combinations tested).
- **Side:** receiver-side only; a no-op on senders.
- **Tests:** `test/test_socket_options.cpp` — `PeriodicNakGateDefaultOff`,
  `PeriodicNakGateSetGetRoundTrip`, `PeriodicNakGateRefusedAfterConnect`,
  `PeriodicNakGateIndependentOfReorderFreezeAndNakReport`.
  `test/test_periodic_nak_gate.cpp` — `Wire/PeriodicNakGate.ReorderingAndGenuineLoss`
  over both bool arms: a 400 ms wire hold of packet #100 at reorder distance 40, the
  option-off premature-NAK control (4 early NAKs for #100), and genuine-loss (#150)
  expiry plus periodic re-reporting in both modes. Flake-guarded 20x.

Any other functional change to the C/C++ source remains out of scope (see SCOPE
BOUNDARY). To bump the libsrt version consumed by `cerastream`/`srtla`, update the
`srt` `pin:` in `versions.yaml` and re-vendor — do not open PRs against upstream C
source for unrelated features.

## BASELINE PATCH STATUS (ADR-002 "C is SAFE")

**`SRTO_REORDERFREEZE` was the only CERALIVE patch at the reset** of the CeraLive
line to Haivision `1e4c908`. No other functional changes were introduced in that
reset relative to upstream v1.5.5 plus its security/bug fixes.

ADR-002 verdict: **"C is SAFE"** — the C `srtla_rec` receiver is safe to keep
without any additional libsrt patch for baseline parity. No new patch is needed.

### Device-side FEC packet-filter — compiled-in by default; catalog deferred

`SRTO_PACKETFILTER` (the SRT packet-filter API that enables Forward Error
Correction) **is compiled-in by default** in all libsrt builds, including the
CERALIVE fork. The FEC plugin is an upstream feature; `srtcore/filelist.maf`
lists `fec.cpp` and `packetfilter.cpp` unconditionally. There is no
`-DENABLE_PACKET_FILTER=ON` CMake flag in libsrt — the packet-filter API is
always present.

**Deferral rationale:** The operator-facing receiver-capability catalog (which
lists available FEC mixtures) is deferred until a FEC mixture is being actively
evaluated for gain (gain-hunt track). There is no current evidence that FEC
overhead would improve the bonded-SRTLA path — ARQ already handles loss recovery
on the bonded links. When a FEC gain-hunt is initiated, the correct approach is
to evaluate FEC vs ARQ tradeoffs on the actual bonded path before committing a
mixture to the operator catalog.

**Evidence:** Empirical probe (`srtla/test-results/fec-capability-probe.json`,
Task A1) confirms FEC is compiled-in: system libsrt (83 FEC symbols, runtime
`srt_setsockopt(SRTO_PACKETFILTER,"fec")==0` OK), vanilla Haivision v1.5.5 (83
symbols, OK), CERALIVE patched (84 symbols, OK). The real deferral lever is the
operator-facing catalog, not a compile flag.

## RECEIVER CAPABILITY RECONCILIATION

Canonical decision record: [`docs/RECEIVER-RECONCILIATION.md`](../docs/RECEIVER-RECONCILIATION.md)

**Baseline patch status confirmed (Task 3, ADR-002 "C is SAFE"):**
`SRTO_REORDERFREEZE` was the only CERALIVE patch at the `1e4c908` reset.
No additional libsrt patch is needed for BELABOX-parity baseline. The
stock-libsrt substitution (`nakreport=0` + `lossmaxttl=40`) is authorized by
ADR-002 as a safe equivalent.

**Device-side FEC packet-filter — compiled-in by default; catalog deferred.**
`SRTO_PACKETFILTER` is compiled-in by default in all libsrt builds (no
`-DENABLE_PACKET_FILTER=ON` flag exists in libsrt CMake). The operator-facing
receiver-capability catalog (which lists available FEC mixtures) is deferred until
a FEC mixture is actively being evaluated for gain (gain-hunt track). There is no
current evidence that FEC overhead improves the bonded-SRTLA path. The catalog
remains empty until gain-hunt evidence passes the pre-registered decision gate.

Cross-ref: [`srtla/docs/adr/ADR-002-srt-patch-necessity.md`](../srtla/docs/adr/ADR-002-srt-patch-necessity.md)

## SCOPE BOUNDARY

**No unsanctioned C/C++ feature work here.** Packaging, release automation, and
documentation changes are in scope when they preserve the runtime ABI contract.

## COMMON TASKS

Task-routing for the handful of legitimate CeraLive operations on this vendored
repo. Anything not listed is upstream Haivision work and out of scope (see SCOPE
BOUNDARY).

| I need to… | Do this |
|------------|---------|
| Build the device runtime package (library + bundled SRT tools) | `packaging/build-deb.sh` (outputs `dist/libsrt1.5-ceralive_*.deb`) |
| Use / find `srt-live-transmit` on device | It ships in `libsrt1.5-ceralive` at `/usr/bin/srt-live-transmit` (see [Bundled SRT tools](#bundled-srt-tools-srt-live-transmit-srt-file-transmit-srt-tunnel)) |
| Verify replacement behavior + tool linkage | `packaging/verify-runtime-replacement.sh <deb>` |
| Build the library to test it standalone | [BUILD](#build) — `cmake -B build …` |
| Run the unit + bonding test suite | [TEST (ctest)](#test-ctest) |
| Find the source / build config / options | [WHERE TO LOOK](#where-to-look) |
| Touch the reorder-freeze or periodic-NAK-gate option | See [SANCTIONED CERALIVE PATCHES](#sanctioned-ceralive-patches--srto_reorderfreeze-srto_periodicnakgate) — keep each to its one mechanism, decoupled from `SRTO_NAKREPORT` and from each other |
| Update routing/build/test guidance | Edit this `AGENTS.md` |
| Confirm the runtime contract | [ROLE IN THE GROUP](#role-in-the-group); the device uses `libsrt1.5-ceralive` |

`runtime-package.yml` runs the package contract and GStreamer replacement gate. Run
the commands below locally before changing the fork or its package recipe.

## CI COMPILER-CACHE COVERAGE

Every GitHub Actions workflow that performs an ordinary C/C++ build restores a
source-aware ccache archive through `actions/cache/restore@v6` before
compilation. Keys separate the workflow purpose, host OS/architecture, and
relevant matrix dimensions; each has a same-surface restore prefix. Successful
non-PR runs persist the bounded archive through `actions/cache/save@v6`; PRs are
restore-only so untrusted code cannot seed executable compiler output. Every
cache is capped at 200 MB, and CMake is wired through
`CMAKE_C_COMPILER_LAUNCHER=ccache` and
`CMAKE_CXX_COMPILER_LAUNCHER=ccache`.
When a build can generate files beneath a hashed source glob, compute the key
once from the clean post-checkout tree and reuse that immutable value for both
restore and save. The Android workflow does this because `build-android`
creates dependency and ABI output beneath its hashed `scripts/**` tree.
Android also uses content-based compiler identity because `setup-ndk`
materializes NDK r23 on each runner; ccache's default mtime identity would turn
identical compiler bytes with fresh mtimes into cross-run misses. Its versioned
cache namespace is part of that policy so an older mtime-keyed archive cannot
block the first content-keyed save.

| Workflow | Coverage |
|----------|----------|
| `runtime-package.yml` | Existing Docker-mounted ccache, normalized to the shared key/size/launcher contract |
| `publish-release.yml` | Host test build and Docker package build share the restored per-architecture cache |
| `abi.yml` | Separate current/base caches prevent concurrent writers while preserving stable restore prefixes |
| `ubuntu-c++03.yml`, `ubuntu-c++11.yml`, `macos.yml` | Native Makefile builds use the CMake launchers (renamed from `cxx03-ubuntu.yaml` / `cxx11-ubuntu.yaml` / `cxx11-macos.yaml` in the upstream v1.5.6 CI restructure; CeraLive ccache carried onto the renamed files) |
| `windows-msvc-noenc.yml` | Uses Ninja with an explicit x64 MSVC developer environment because CMake compiler launchers are supported by Makefile/Ninja generators, not the Visual Studio generator (renamed from `cxx11-win.yaml`) |
| `ubuntu-mingw.yml` | Upstream v1.5.6 addition (MinGW cross-build, `-DENABLE_UNITTESTS=OFF`); intentionally uncached — no CeraLive ccache precedent and it runs no unit tests |
| `android.yaml`, `iOS.yaml`, `s390x-focal.yaml` | Target/matrix-specific cross-build caches; container builds mount a host-restored cache path |

`codeql.yml` is intentionally uncached: its manual C/C++ build must execute and
trace compiler processes to populate the CodeQL database, while a ccache hit can
bypass the compiler. `codespell.yml` and the `publish` job in
`publish-release.yml` do not compile C/C++ and therefore need no compiler cache.

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

On `1.5.7+ceralive.1` (Haivision v1.5.7 plus `SRTO_REORDERFREEZE` and
`SRTO_PERIODICNAKGATE`), the full gtest suite passes (1 disabled:
`CTimer.SleeptoAccuracy`). On a sandboxed host, 11 bind/reuse cases (`ReuseAddr.*`,
`Transmission.FileUpload`, `SocketData.PeerName`) can fail for environment reasons
unrelated to the source; running the same binaries inside an anonymous
`unshare --user --map-root-user --net` namespace with `lo` up and a dummy
`192.0.2.1/24` interface makes them pass. Note: `ENABLE_TESTING=ON` alone registers no ctest
tests — `ENABLE_UNITTESTS=ON` is what wires the gtest suite into ctest.

Socket tests keep one owner per handle. A `UniqueSocket` must not be bypassed by
raw-closing its handle, and an explicit owner close releases that handle while
still asserting the real `srt_close` result. `srt_close` preserves its existing
idempotent-close contract when a valid socket is retired by the garbage collector
between the public state check and internal close acquisition. For connected
pairs, register readiness before launching a fast peer, synchronize the worker,
and close every owner explicitly; finite transfers consume their known byte count
instead of using peer shutdown as an end marker. These lifecycle assertions are
required gates and must not be relaxed or retried away.
Connection-timeout tests enforce their upper timing bound against the async
connect-failure callback timestamp, not elapsed time after the waiting thread is
rescheduled, and also assert the epoll result, callback error, socket state, and
rejection reason.

## WHERE TO LOOK

| Need | Location |
|------|----------|
| Library source | `srtcore/` |
| Reorder-freeze option enum | `srtcore/srt.h` → `SRTO_REORDERFREEZE` |
| Reorder-freeze config field / setter | `srtcore/socketconfig.h` / `srtcore/socketconfig.cpp` |
| Reorder-freeze decay gates | `srtcore/core.cpp` → `// CERALIVE reorder-freeze` |
| Periodic-NAK-gate option enum | `srtcore/srt.h` → `SRTO_PERIODICNAKGATE` |
| Periodic-NAK-gate config field / setter / getter | `srtcore/socketconfig.h` / `srtcore/socketconfig.cpp` / `srtcore/core.cpp` |
| Periodic-NAK-gate report subtraction | `srtcore/core.cpp` → `checkNAKTimer`, `// CERALIVE periodic-nak-ttl` |
| Periodic-NAK-gate wire test | `test/test_periodic_nak_gate.cpp` |
| Release version record | sender repo `docs/evidence/bpc/srt-release.json` |
| Build config | `CMakeLists.txt` |
| Build options reference | `docs/build/build-options.md` |
| License | `LICENSE` (MPLv2.0) |
| versions.yaml entry | `../versions.yaml` → `srt:` |
