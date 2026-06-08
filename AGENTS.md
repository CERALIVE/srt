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

## TEST (ctest)

The GoogleTest unit + bonding suite is gated on `ENABLE_UNITTESTS` (with `ENABLE_CXX11`, ON by default); `ENABLE_TESTING` additionally builds the developer test apps. Both are OFF by default. Run the full suite with:

```bash
cmake -B build -DENABLE_TESTING=ON -DENABLE_UNITTESTS=ON -DENABLE_BONDING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Baseline: **286/286 passed** (1 disabled: `CTimer.SleeptoAccuracy`). CI runs this via `.github/workflows/ctest.yaml`. Note: `ENABLE_TESTING=ON` alone registers no ctest tests — `ENABLE_UNITTESTS=ON` is what wires the gtest suite into ctest.

## WHERE TO LOOK

| Need | Location |
|------|----------|
| Library source | `srtcore/` |
| Build config | `CMakeLists.txt` |
| License | `LICENSE` (MPLv2.0) |
