<!-- Moved verbatim from AGENTS.md on 2026-10-05 by lean-rules-docs-landing-latam -->

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

