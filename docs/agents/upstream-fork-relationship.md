<!-- Moved verbatim from AGENTS.md on 2026-10-05 by lean-rules-docs-landing-latam -->

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
| `master` | Haivision v1.5.6 + CeraLive runtime changes | The maintained CeraLive runtime line and source of released device packages |

`reorderfreeze-1.5.5` is neither a branch nor a tag in CERALIVE/srt or
Haivision/srt; do not use it as a checkout base. The CeraLive line was reset at
`66b3609` to Haivision `1e4c908` and retains only the sanctioned
`SRTO_REORDERFREEZE` patch, not the old BELABOX C-source patches (unconditional
reorder-tolerance freeze, periodic-NAK disable, or the
`iMaxReorderTolerance` TTL override).

### ABI baseline

This branch merges Haivision **v1.5.7** (`899348d`) as a true merge, retaining
`SRTO_REORDERFREEZE = 120` and deterministic socket teardown. It brings upstream
handshake, ACK, DROPREQ, FEC, bonding and sample-tool hardening. The published
package remains `1.5.6+ceralive.1` until the separate release cutover; this source
sync does not advance the ABI baseline below.

`.github/workflows/abi.yml` compares proposed builds with the immutable
`srt-v1.5.6+ceralive.1` tag: the latest shipped CeraLive source release. This
tests compatibility against the ABI that device consumers actually received,
without depending on a fork-external or absent ref. Advance this baseline only
after the next CeraLive runtime release is published.

