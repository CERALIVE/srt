<!-- Moved verbatim from AGENTS.md on 2026-10-05 by lean-rules-docs-landing-latam -->

## BASELINE PATCH STATUS (ADR-002 "C is SAFE")

**`SRTO_REORDERFREEZE` is the only CERALIVE patch** on the CeraLive line derived
from Haivision `1e4c908`. No other functional changes were introduced in that
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

