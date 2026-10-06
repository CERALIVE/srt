<!-- Moved verbatim from AGENTS.md on 2026-10-05 by lean-rules-docs-landing-latam -->

## RECEIVER CAPABILITY RECONCILIATION

Canonical decision record: [`docs/RECEIVER-RECONCILIATION.md`](https://github.com/CERALIVE/ceralive/blob/master/docs/RECEIVER-RECONCILIATION.md)

**Baseline patch status confirmed (Task 3, ADR-002 "C is SAFE"):**
`SRTO_REORDERFREEZE` is the only CERALIVE patch on the post-`1e4c908` CeraLive
line. No additional libsrt patch is needed for BELABOX-parity baseline. The
stock-libsrt substitution (`nakreport=0` + `lossmaxttl=40`) is authorized by
ADR-002 as a safe equivalent.

**Device-side FEC packet-filter — compiled-in by default; catalog deferred.**
`SRTO_PACKETFILTER` is compiled-in by default in all libsrt builds (no
`-DENABLE_PACKET_FILTER=ON` flag exists in libsrt CMake). The operator-facing
receiver-capability catalog (which lists available FEC mixtures) is deferred until
a FEC mixture is actively being evaluated for gain (gain-hunt track). There is no
current evidence that FEC overhead improves the bonded-SRTLA path. The catalog
remains empty until gain-hunt evidence passes the pre-registered decision gate.

Cross-ref: [`srtla/docs/adr/ADR-002-srt-patch-necessity.md`](https://github.com/CERALIVE/srtla/blob/main/docs/adr/ADR-002-srt-patch-necessity.md)

