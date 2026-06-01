# srt

Parent: [`../AGENTS.md`](../AGENTS.md)

## ROLE IN THE GROUP

CERALIVE fork of [Haivision SRT](https://github.com/Haivision/srt) with BELABOX patches. VENDORED upstream — minimal modifications only.

Provides `libsrt` at compile time to `ceracoder` and `srtla`. `irl-srt-server` uses system libsrt (deployment-dependent version).

## CAUTION: UPSTREAM CODE

- This is upstream Haivision code. Do NOT make functional changes.
- `srt/CONTRIBUTING.md` is MPLv2.0 upstream — MUST NOT be modified.
- Patches must be minimal and clearly marked as CERALIVE/BELABOX additions.
- Remote: `origin https://github.com/CERALIVE/srt`

## BUILD

Standard CMake. Consumed as a submodule by `ceracoder` and `srtla` — do not build standalone unless testing.

## WHERE TO LOOK

| Need | Location |
|------|----------|
| Library source | `srtcore/` |
| Build config | `CMakeLists.txt` |
| License | `LICENSE` (MPLv2.0) |
