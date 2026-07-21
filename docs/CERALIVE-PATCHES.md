# CeraLive Patch Set

This document is the canonical inventory of the CeraLive-specific changes carried on
top of upstream [Haivision/srt](https://github.com/Haivision/srt) in the
`libsrt1.5-ceralive` runtime fork. It is the "branding" reference for the fork: what
we added, why, and where to find each change.

The C/C++ source of this fork stays as close to upstream as possible (see
[`AGENTS.md`](../AGENTS.md) → SCOPE BOUNDARY). Upstream is absorbed by **true merge**,
never by rebase/replay, so every upstream tag remains fully contained in the fork
history and the merge-base keeps advancing on each catch-up. The most recent sync is
upstream **v1.5.6** (`c63c311`, KMREQ heap-overflow hardening, CVE-2026-55868/55869).

There are exactly **two** functional CeraLive patches to the C/C++ source. Everything
else the fork carries is packaging and CI (documented at the end for completeness).

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

## 2. Deterministic socket teardown

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
rather than silently dropped. Any functional change beyond these two patches is out of
scope for the fork (see [`AGENTS.md`](../AGENTS.md) → SCOPE BOUNDARY).
