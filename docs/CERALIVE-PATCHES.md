# CeraLive Patch Set

This document is the canonical inventory of the CeraLive-specific changes carried on
top of upstream [Haivision/srt](https://github.com/Haivision/srt) in the
`libsrt1.5-ceralive` runtime fork. It is the "branding" reference for the fork: what
we added, why, and where to find each change.

The C/C++ source of this fork stays as close to upstream as possible (see
[`AGENTS.md`](../AGENTS.md) → SCOPE BOUNDARY). Upstream is absorbed by **true merge**,
never by rebase/replay, so every upstream tag remains fully contained in the fork
history and the merge-base keeps advancing on each catch-up. The most recent sync is
upstream **v1.5.7** (`899348d`, KMREQ and encryption-state validation, ACK and DROPREQ
validation, FEC bounds, bonding BACKUP lifetime safety, and sample-tool path
validation). All CeraLive patches below are retained. The published package and the
immutable ABI comparison baseline remain `srt-v1.5.6+ceralive.1` pending the separate
release cutover.

The functional CeraLive changes to the C/C++ source are the three socket options
(`SRTO_REORDERFREEZE = 120`, `SRTO_PERIODICNAKGATE = 119`, `SRTO_SRTLAPATCHES = 118`)
and the deterministic socket-teardown fix. Everything else the fork carries is
packaging and CI (documented at the end for completeness).

---

## 1. `SRTO_REORDERFREEZE` — opt-in reorder-tolerance decay freeze

- **Commit:** `66b3609cc004e6a4c485e0adc11149025e782083` (2026-06-24)
  — *feat(core): reset to upstream 1e4c908; add opt-in SRTO_REORDERFREEZE decoupled
  from NAKREPORT*
- **Type:** new receiver-side socket option, **default off** (`SRTO_REORDERFREEZE = 120`).
- **Where:** `srtcore/srt.h` (enum), `srtcore/socketconfig.{h,cpp}`
  (`CSrtConfig::bReorderFreeze` + setter), `srtcore/core.cpp` (the two decay gates,
  each tagged `// CERALIVE reorder-freeze`), `test/test_socket_options.cpp` (tests).

**Rationale.** SRTLA delivers packets out of order **by design** — bonded traffic is
balanced across multiple links, so a healthy bonded path is naturally reordered. Stock
libsrt runs an adaptive reorder-tolerance *decay* that drives the tolerance toward 0 on
a clean ordered stream; over a bonded path that decay causes spurious retransmissions.
`SRTO_REORDERFREEZE` freezes only the **decay** (it does not touch `initial_loss_ttl`,
is orthogonal to `SRTO_NAKREPORT`, and is a no-op on senders), so a receiver on a
bonded ingest can hold reorder tolerance at max without any of BELABOX's broader C
patches. It is opt-in and inherited by accepted sockets from the listener, so enabling
it on the receive listener propagates to every accepted connection. ADR-002 records
this as the **only** patch needed for BELABOX-parity baseline ("C is SAFE").

---

## 2. `SRTO_PERIODICNAKGATE` — periodic loss-report tri-state

- **Type:** new receiver-side socket option, **default off**
  (`SRTO_PERIODICNAKGATE = 119`).
- **Where:** `srtcore/srt.h` (enum), `srtcore/socketconfig.{h,cpp}`
  (`CSrtConfig::iPeriodicNakGate` + setter), `srtcore/core.cpp` (the NAK-timer
  decision, tagged `// CERALIVE periodic-NAK gate`),
  `test/test_periodic_nak_gate.cpp` (tests).

**Rationale.** Stock libsrt emits a `UMSG_LOSSREPORT` on every due NAK timer,
reporting the whole receiver loss list. On a deliberately-reordered bonded ingest
this re-reports sequences that are merely late. The option is a tri-state:
`0` = off (stock), `1` = filter (subtract ranges still inside their reorder TTL,
report the rest), `2` = suppress (send nothing from this site; the NAK timer
still advances — behaviourally identical to `irlserver/srt`'s `SRTLAPATCHES`
suppression). Out-of-range values are rejected with `SRT_EINVPARAM`. Which arm
ships as the compat default is decided by the D10 A/B; until then the compat shim
below installs `2`.

---

## 3. `SRTO_SRTLAPATCHES` — irlserver compat enumerator

- **Type:** new bool-like compatibility socket option, **default off**
  (`SRTO_SRTLAPATCHES = 118`).
- **Where:** `srtcore/srt.h` (enum), `srtcore/socketconfig.{h,cpp}` (setter that
  maps onto `bReorderFreeze` + `iPeriodicNakGate`, plus the
  `SRTLA_PATCHES_DEFAULT_NAKGATE` default), `srtcore/core.cpp` (the conjunctive
  getter), `apps/socketoptions.hpp` (`srtlapatches` URI row),
  `test/test_srtlapatches.cpp` (tests).

**Rationale.** Owns the "one switch reproduces `irlserver/srt`'s `SRTLAPATCHES`"
contract without a second code path: non-zero sets `SRTO_REORDERFREEZE = true`
and `SRTO_PERIODICNAKGATE` to `SRTLA_PATCHES_DEFAULT_NAKGATE` (initially `2`),
zero clears both. The getter is `bReorderFreeze && iPeriodicNakGate != 0`, so
writing either underlying option afterwards overrides it (last write wins). Only
`0`/non-zero are meaningful. The `SRTLA_PATCHES_DEFAULT_NAKGATE` default is set
by the D10 A/B (plan `upstream-rebase-hard-fork` todo 38).

---

## 4. Deterministic socket teardown

- **Commit:** `293ae6f45bf116c56d056b3a25312b2aade7dade` (2026-07-13)
  — *fix(core): make socket teardown deterministic*
- **Type:** correctness/reliability fix to socket close semantics and the test harness
  that guards them.
- **Where:** `srtcore/srt_c_api.cpp` (idempotent public close), plus the socket
  lifecycle test suite (`test/test_epoll.cpp`, `test_file_transmission.cpp`,
  `test_main.cpp`, `test_reuseaddr.cpp`).

**Rationale.** Makes the public `srt_close` path **idempotent across the bounded
garbage-collector retirement race** — when a valid socket is retired by the GC between
the public state check and the internal close acquisition, close still returns its
existing idempotent-close result instead of racing. It also hardens the test harness to
match: single-owner cleanup (no raw-closing a `UniqueSocket`'s handle), readiness
registered before peer activity, and synchronized file-transfer / IPv6-reuse teardown.
These lifecycle assertions are **required gates** — they must not be relaxed or retried
away (see [`AGENTS.md`](../AGENTS.md) → TEST). The determinism matters on the device
because `cerastream` opens and closes SRT sockets across stream start/stop cycles; a
non-deterministic teardown surfaces as flaky reconnects.

---

## Packaging & CI (non-source CeraLive additions)

These do not change the SRT protocol or the library ABI; they exist so the fork ships
as the device runtime package. Listed for completeness — they are **not** functional
C/C++ patches.

- **Device runtime package** — `feat(packaging): ship CeraLive SRT runtime` (`c590cce`)
  and follow-ups: `packaging/build-deb.sh` produces `libsrt1.5-ceralive` (GnuTLS
  backend, SONAME `libsrt.so.1.5`) which replaces the Debian TLS-flavor packages and
  provides their virtual names so one CeraLive `libsrt.so.1.5` ABI loads per process.
- **Bundled SRT sample tools** — `feat(packaging): bundle SRT sample tools into
  libsrt1.5-ceralive` (`9b02dc7`): ships `srt-live-transmit` and siblings in the same
  package, dynamically linked against the one shipped `libsrt.so.1.5` (single-fork
  invariant preserved; verified in `runtime-package.yml` / `publish-release.yml`).
- **CI** — `runtime-package.yml` (package contract + GStreamer replacement gate),
  `publish-release.yml` (release + apt dispatch), and bounded ccache coverage across
  the upstream test workflows (`ubuntu-c++03.yml`, `ubuntu-c++11.yml`, `macos.yml`,
  `windows-msvc-noenc.yml`). See [`AGENTS.md`](../AGENTS.md) → CI COMPILER-CACHE
  COVERAGE.

---

## Maintenance rule

When syncing upstream, keep this file current: a new CeraLive C/C++ patch **must** be
added here with its commit SHA and a one-paragraph rationale, and a patch that is
retired (e.g. superseded by an upstream fix) **must** be moved to a "Retired" note
rather than silently dropped. Any functional change beyond these patches is out of
scope for the fork (see [`AGENTS.md`](../AGENTS.md) → SCOPE BOUNDARY).
