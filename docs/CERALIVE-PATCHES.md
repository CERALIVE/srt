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
validation). All CeraLive patches below are retained. That sync ships as the
`1.5.7+ceralive.2` release described under [Releases](#releases) below; the immutable
ABI comparison baseline used by `.github/workflows/abi.yml` stays at
`srt-v1.5.6+ceralive.1` (the previously shipped release) and is advanced deliberately,
one release behind, never in the same change that cuts a release.

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
suppression, `f2297192:srtcore/core.cpp:12018-12029`). Out-of-range values are
rejected with `SRT_EINVPARAM`.

Which arm ships as the compat default was decided by the **D10 A/B**, now measured:
**the winner is `2`** (suppress, upstream-exact parity). See
[Releases](#releases) → `1.5.7+ceralive.2` for the campaign shape and the
verdict. The option itself remains explicitly settable to
`1` by any consumer that wants the filter arm; the A/B only fixed what the
`SRTO_SRTLAPATCHES` compat shim installs.

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
and `SRTO_PERIODICNAKGATE` to `SRTLA_PATCHES_DEFAULT_NAKGATE` (**`2`**, fixed by
the D10 A/B), zero clears both. The getter is `bReorderFreeze && iPeriodicNakGate
!= 0`, so writing either underlying option afterwards overrides it (last write
wins). Only `0`/non-zero are meaningful.

Upstream `irlserver/srt` numbers its own `SRTLAPATCHES` **120** — the number
CeraLive already uses for `SRTO_REORDERFREEZE`. The names match, the numbers do
not, and numbers are ABI: never renumber any of the three.

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

## Releases

Package name `libsrt1.5-ceralive`; Debian version `<haivision-base>+ceralive.<n>`;
git tag `srt-v<debian-version>`; GitHub release name `CeraLive SRT <debian-version>`.
The Debian revision `<n>` counts CeraLive releases and is independent of the
Haivision base, so `1.5.7+ceralive.2` is the second CeraLive release, not a second
patch on `1.5.7`. `dpkg --compare-versions 1.5.7+ceralive.2 gt 1.5.6+ceralive.1` is
true, so an apt upgrade from the previous release is ordinary.

### 1.5.7+ceralive.2 — the SRTLA option set

- **Haivision base:** **v1.5.7** (`899348d`), absorbed by true merge (`7a6cc86`).
- **Tag:** `srt-v1.5.7+ceralive.2`. **Packages:**
  `libsrt1.5-ceralive_1.5.7+ceralive.2_{arm64,amd64}.deb`.
- **ABI:** additive only. Three new `SRT_SOCKOPT` enumerators and one new
  `CSrtConfig` field; no existing symbol, struct layout, or option number changed.
  SONAME stays `libsrt.so.1.5`. `abi.yml` compares against the previous release
  `srt-v1.5.6+ceralive.1`.

**Socket options — exact numbers and semantics.**

| Number | Enumerator | Type | Semantics | URI key |
|---|---|---|---|---|
| `118` | `SRTO_SRTLAPATCHES` | bool-like (`0` / non-zero) | **Compat shim** carrying `irlserver/srt`'s option name. Non-zero ⇒ `SRTO_REORDERFREEZE = true` **and** `SRTO_PERIODICNAKGATE = SRTLA_PATCHES_DEFAULT_NAKGATE` (**`2`**). Zero ⇒ both cleared. Getter is the conjunction `bReorderFreeze && iPeriodicNakGate != 0`. | `srtlapatches` |
| `119` | `SRTO_PERIODICNAKGATE` | `int32_t`, tri-state `[0,2]` | `0` off (stock Haivision: the periodic `UMSG_LOSSREPORT` is always sent). `1` filter (subtract ranges still inside their reorder TTL; report the remainder). `2` suppress (send nothing from this site; the NAK timer still advances). Anything else ⇒ `SRT_EINVPARAM`. | `periodicnakgate` |
| `120` | `SRTO_REORDERFREEZE` | bool | Freeze `m_iReorderTolerance` at `iMaxReorderTolerance` — no decay on ordered/early runs. Pre-existing CeraLive option, unchanged by this release. | *none* — API only, or reached via `srtlapatches` |

All three are receiver-side, **default off**, `SRTO_R_PRE`, and inherited by accepted
sockets from the listener. `118` deliberately does **not** add a third code path: it
is a pure write-through onto `120` + `119`, so setting either of those *after* it
overrides it (last write wins).

`apps/socketoptions.hpp` carries URI rows for `srtlapatches` and `periodicnakgate`
only. `SRTO_REORDERFREEZE` has never had one, so `srt://…?reorderfreeze=1` is not a
supported spelling in the bundled sample tools; set it through the C API, or turn it
on together with the NAK gate via `?srtlapatches=1`.

**Equivalence with upstream `irlserver/srt` `SRTLAPATCHES=1`.** Upstream gates four
sites behind one flag; CeraLive splits the same behaviour across `120` + `119`.
Verified by reading `f2297192:srtcore/core.cpp` against this tree.

| # | Upstream gate site | CeraLive equivalent | Verdict |
|---|---|---|---|
| 1 | `initial_loss_ttl = srtlaPatches ? iMaxReorderTolerance : m_iReorderTolerance` | `initial_loss_ttl = m_iReorderTolerance`, ungated | **Exact.** Every write to `m_iReorderTolerance` was enumerated: it initialises to the max, its only decrements are sites 2 and 3, and its only other write is an increase capped at the max. So while `SRTO_REORDERFREEZE` is on, `m_iReorderTolerance == iMaxReorderTolerance` identically. |
| 2 | 50-consecutive-ordered decay, gated `!srtlaPatches` | same decay, gated `!bReorderFreeze` | **Exact.** |
| 3 | 10-consecutive-early decay, gated `!srtlaPatches` | same decay, gated `!bReorderFreeze` | **Exact.** |
| 4 | periodic NAK: `if (!srtlaPatches) sendCtrl(UMSG_LOSSREPORT)` — suppressed outright (`f2297192:srtcore/core.cpp:12018-12029`) | `SRTO_PERIODICNAKGATE`: `2` = suppress, `1` = filter | **Exact at the shipped default.** `SRTLA_PATCHES_DEFAULT_NAKGATE = 2`, so `SRTO_SRTLAPATCHES=1` reproduces upstream site 4 bit-for-bit. `1` is the deliberate CeraLive divergence and is opt-in only. |

Sites 1-3 hold **only while `SRTO_REORDERFREEZE` is on** — site 1's equivalence is a
consequence of sites 2 and 3 being frozen, not an independent property. A future
ungated decay path would silently break it toward shorter loss TTLs.

**The site-4 default was measured, not chosen.** The D10 A/B ran arm A
(`periodicnakgate=1`, filter) against arm B (`periodicnakgate=2`, suppress) on the
`srtla` compat harness under a pre-registered, hash-frozen rule: 4 netem cells
(`loss` 0.5 %/2 % × `reorder` 10 %/25 %), N = 3 per arm per cell, 24/24 rows valid
with no retries. Primary metric is viewer-observed loss
(`pktRcvDropTotal / (pktRecvUniqueTotal + pktRcvDropTotal)`), with a goodput guard.
A won loss on **1 of 4** cells where the rule requires ≥ 3, and the goodput guard
held on all 4, so **WINNER = 2**. Ties inside the pre-registered 0.100 pp noise band
were not broken in either arm's favour. Evidence: `docs/evidence/ab-periodic-nak/`
in `CERALIVE/srtla` (`rows.json`, `verdict.json`, `rule_sha256`
`3afffaf6743175a065855d4216791d82afc5a0612c30db84f251a63932d49fea`).

**Also in this release** (from the v1.5.7 absorb): upstream KMREQ and
encryption-state validation, ACK and DROPREQ validation, FEC bounds checks, bonding
BACKUP lifetime safety, and sample-tool path validation.

### 1.5.6+ceralive.1 — previous release

Haivision base v1.5.6. `SRTO_REORDERFREEZE = 120` and the deterministic
socket-teardown fix (sections 1 and 4 above). Remains the immutable `abi.yml`
comparison baseline.

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

Every cut release **must** gain a [Releases](#releases) subsection in the same change
that tags it, naming the Haivision base commit, the tag, the two `.deb` filenames, and
whether the ABI is additive. A change to any of the three option numbers or to
`SRTLA_PATCHES_DEFAULT_NAKGATE` **must** restate the equivalence table above, because
that table is what downstream consumers rely on when they set `srtlapatches=1`.
