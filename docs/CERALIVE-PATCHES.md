# CeraLive Patch Set

This document is the canonical inventory of the CeraLive-specific changes carried on
top of upstream [Haivision/srt](https://github.com/Haivision/srt) in the
`libsrt1.5-ceralive` runtime fork. It is the "branding" reference for the fork: what
we added, why, and where to find each change.

The C/C++ source of this fork stays as close to upstream as possible (see
[`AGENTS.md`](../AGENTS.md) → SCOPE BOUNDARY). Upstream is absorbed by **true merge**,
never by rebase/replay, so every upstream tag remains fully contained in the fork
history and the merge-base keeps advancing on each catch-up. The most recent sync is
upstream **v1.5.7** (`899348d`, KMREQ and encryption-state validation, ACK and
DROPREQ validation, FEC bounds, bonding BACKUP lifetime safety, and sample-tool
path validation), plus six `master` fixes that landed after that tag and are folded as
a second true merge (#3366 `922a890`, #3371 `73d8cd6`, #3369 `8b852eb`, #3333
`abf708d`, #3330 `03fae0e`, #3351 `cae8f62` — see
[`AGENTS.md`](../AGENTS.md) → POST-TAG UPSTREAM FOLD, which also records why #3380
`ff8ab25` and #3355 `500b1c8` are excluded). It ships as `1.5.7+ceralive.2` (tag
`srt-v1.5.7+ceralive.2`,
package `libsrt1.5-ceralive_1.5.7+ceralive.2_{amd64,arm64}.deb`). The ABI comparison
baseline in `abi.yml` stays on the previously published `srt-v1.5.6+ceralive.1`
until that release is published; `1.5.7+ceralive.2` was checked 100% binary- and
source-compatible against it, with zero problems and zero warnings.

There are **two** sanctioned socket-option patches to the C/C++ source
(`SRTO_REORDERFREEZE`, `SRTO_PERIODICNAKGATE`), plus one teardown correctness fix.
Everything else the fork carries is packaging and CI (documented at the end for
completeness).

### Fork-reserved option value band and collision policy

Both CeraLive socket options live in the enum band **111-120**, far above the last
upstream `SRT_SOCKOPT` value (63) so ordinary upstream growth does not land on them,
and the band is allocated **downward from 120**:

- `SRTO_REORDERFREEZE = 120` (band top, lexically last member)
- `SRTO_PERIODICNAKGATE = 119`
- 111-118: reserved for future fork options, next free value first; do not assign
  any other meaning and never gap-fill upstream's range.

**Why downward, and why the top is frozen.** `SRTO_E_SIZE` is upstream's trailing
auto-valued sentinel (`// Always last element`), and it is part of the public header.
`srt-v1.5.6+ceralive.1` shipped it as `121`. Appending a fork option *above* 120
moves the sentinel, and `abi-compliance-checker` rates that a Medium binary problem
(seven `SRT_SOCKOPT`-typed entry points affected), which fails `abi.yml` against the
published baseline. This was measured, not guessed: the first `1.5.7+ceralive.1`
candidate placed `SRTO_PERIODICNAKGATE = 121` and scored 97.2% binary compatibility
with exactly that one problem; moving it to 119 restored 100%. Adding a member below
the top is recorded as "No effect". So the band top is `120`, `SRTO_E_SIZE` stays
`121` for every future fork option, and the ABI lane keeps passing release after
release without ever being silenced or re-baselined ahead of a publish.

**Collision policy.** If upstream ever assigns a value inside 111-120, the fork
**renumbers its own option** at the next `+ceralive` release rather than carrying a
duplicate value. Renumbering is an API/ABI change for direct FFI consumers
(`cerastream`), so it is announced in this file and in the release notes, and
`abi.yml` is expected to flag it; the upstream value is never shadowed.

### Upstream-proposal status

Neither option has been proposed to Haivision/srt. Both are **deferred** until the
bonded-path convergence campaign produces field evidence (the sender-side
`srtla-send-rs` gain-hunt track) that quantifies their effect on a real bonded
ingest. The fork-reserved band and the per-option scope discipline exist precisely so
each patch stays a self-contained, upstreamable diff if that evidence justifies a
proposal. Until then the options remain fork-only and default-off.

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

**Scope discipline.** Decay-disable only. It does not touch `initial_loss_ttl`, does
not port BELABOX's `initial_loss_ttl = iMaxReorderTolerance` TTL override, and does
not alter NAK scheduling. Sample tools expose it as the PRE bool URI option
`reorderfreeze`.

**Upstream-proposal status:** deferred (see the section above).

---

## 2. `SRTO_PERIODICNAKGATE` — opt-in periodic-NAK reorder-tolerance gate

- **Commits:** `5bd9a237ec83c4006241f9eb68bd32423baf0130` (2026-09-16)
  — *feat(core): add opt-in SRTO_PERIODICNAKGATE socket option and URI rows* (option
  plumbing), and `ca14c8bd06c89d2fd7b69bb3d8eea48dd47c2e3e` (2026-09-17)
  — *feat(core): gate periodic NAK reports by reorder tolerance under
  SRTO_PERIODICNAKGATE* (behavior).
- **Type:** new receiver-side socket option, **default off**
  (`SRTO_PERIODICNAKGATE = 119`; see the band note above for why it sits below
  `SRTO_REORDERFREEZE`).
- **Where:** `srtcore/srt.h` (enum), `srtcore/socketconfig.{h,cpp}`
  (`CSrtConfig::bPeriodicNakGate` + setter, `SRTO_R_PRE` restriction),
  `srtcore/core.cpp` (getter, and the gated block in `checkNAKTimer` tagged
  `// CERALIVE periodic-nak-ttl`), `srtcore/group.cpp` (`getOptDefault` so an empty
  group answers `false`), `apps/socketoptions.hpp` (URI rows `periodicnakgate` and
  `reorderfreeze`), `test/test_socket_options.cpp` (option tests),
  `test/test_periodic_nak_gate.cpp` (wire behavior).

**Mechanism.** Stock libsrt sends a periodic NAK on every NAK-timer tick that
re-reports the **entire** receiver loss list. On a bonded path the loss list is
dominated by packets that are merely late (still inside the reorder-tolerance
window, tracked in `m_FreshLoss`), so each periodic report asks the sender to
retransmit data that is about to arrive anyway. With the option on, `checkNAKTimer`
subtracts the `m_FreshLoss` ranges from the loss array before it builds the periodic
report, so only losses that have **outlived** the packet-count reorder window are
re-reported. The gate uses the existing packet-count window rather than a separate
wall-clock delay, so it needs no new tunable. The immediate-report path, `unlose`,
NAK scheduling and the LiveCC interval floor are unchanged; with the option off the
periodic report still carries the whole loss list, byte-for-byte as upstream.

**Provenance.** This ports the subtraction hunk of onsmith/srt commit `b5690bc`
(2026-07-05) without its bundled `SRTLAPATCHES` build switch. Adaptation to v1.5.7
replaced the lambda comparator with a static function (C++03), and matched the
current `FixedArray<int32_t>` / one-argument `getLossArray` / `m_iMaxDataPayloadSize`
API.

**Scope discipline.** Gates periodic re-reports only. It does not change what is
reported immediately on loss detection, does not change the NAK timer interval, does
not touch reorder-tolerance decay (that is `SRTO_REORDERFREEZE`'s job), and is a
no-op on senders. It is independent of both `SRTO_REORDERFREEZE` and
`SRTO_NAKREPORT`; all eight combinations of the three bools are exercised in the
option tests. Inherited by accepted sockets from the listener exactly like
`SRTO_REORDERFREEZE`.

**Tests.** `test/test_socket_options.cpp`: `PeriodicNakGateDefaultOff`,
`PeriodicNakGateSetGetRoundTrip`, `PeriodicNakGateRefusedAfterConnect`,
`PeriodicNakGateIndependentOfReorderFreezeAndNakReport`.
`test/test_periodic_nak_gate.cpp` (`Wire/PeriodicNakGate.ReorderingAndGenuineLoss`,
parameterised over both bool arms) relays 200 ordered packets through a controlled
wire that holds packet #100 for over 400 ms behind exactly 40 overtaking packets
and discards #150 and all of its retransmissions. With the option **on** no
premature NAK for #100 is emitted; with it **off** the stock periodic report asks
for #100 four times before it arrives (the falsifiability control). Both arms prove
that genuine loss (#150) expires out of the window and is still re-reported
periodically. Run 20x as a flake guard (120 executions, zero failures).

**Upstream-proposal status:** deferred (see the section above).

---

## 3. Deterministic socket teardown

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
added here with its commit SHA, a one-paragraph rationale, its scope discipline, and
its upstream-proposal status; a new socket option takes the next free value BELOW
the last one in the 111-120 band, never above 120. A patch that is
retired (e.g. superseded by an upstream fix) **must** be moved to a "Retired" note
rather than silently dropped. Any functional change beyond these sanctioned entries is out of
scope for the fork (see [`AGENTS.md`](../AGENTS.md) → SCOPE BOUNDARY).
