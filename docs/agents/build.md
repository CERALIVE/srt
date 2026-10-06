<!-- Moved verbatim from AGENTS.md on 2026-10-05 by lean-rules-docs-landing-latam -->

## BUILD

Standard CMake. Consumed by `cerastream` and `srtla` as a sibling checkout at
compile time — do not build standalone unless testing the library itself.

```bash
cmake -B build -DENABLE_TESTING=ON -DENABLE_UNITTESTS=ON -DENABLE_BONDING=ON
cmake --build build -j$(nproc)
```

