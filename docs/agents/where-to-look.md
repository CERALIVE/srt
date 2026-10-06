<!-- Moved verbatim from AGENTS.md on 2026-10-05 by lean-rules-docs-landing-latam -->

## WHERE TO LOOK

| Need | Location |
|------|----------|
| Library source | `srtcore/` |
| Reorder-freeze option enum | `srtcore/srt.h` → `SRTO_REORDERFREEZE` |
| Reorder-freeze config field / setter | `srtcore/socketconfig.h` / `srtcore/socketconfig.cpp` |
| Reorder-freeze decay gates | `srtcore/core.cpp` → `// CERALIVE reorder-freeze` |
| Build config | `CMakeLists.txt` |
| Build options reference | `docs/build/build-options.md` |
| License | `LICENSE` (MPLv2.0) |
| versions.yaml entry | `../versions.yaml` → `srt:` |
