# srt

Parent: [`../AGENTS.md`](../AGENTS.md)

## ROLE IN THE GROUP

`srt` is a **build-time vendored libsrt source** — a CERALIVE fork of
[Haivision/srt](https://github.com/Haivision/srt) with BELABOX patches.

- **SONAME:** `libsrt.so.1.5`
- **Consumers at compile time:** `cerastream` (libsrt FFI link) and `srtla`
- **NOT a `.deb`.** NOT in `REPOS`. Reclassified 2026-06-16 from a prior
  (never-implemented) first-party `.deb` claim.
- **Runtime libsrt on device:** the system package `libsrt1.5-openssl` from
  Debian bookworm `main` — installed by the image runtime OS layer. There is no
  CERALIVE libsrt fork `.deb`. `cerastream` declares `Depends: libsrt1.5-openssl`
  directly; this vendored source is build-time only.
- `irl-srt-server` uses system libsrt (deployment-dependent version), not this fork.

Cross-check: `versions.yaml` entry `srt` — `kind: vendored-transport-src`, no
`arch`/`depends`/`provides` keys, `pin: latest`.

## UPSTREAM-FORK RELATIONSHIP

This is upstream Haivision code. **Do NOT make functional changes** to the libsrt
C/C++ source. Patches must be minimal and clearly marked as CERALIVE/BELABOX
additions.

- `srt/CONTRIBUTING.md` is MPLv2.0 upstream — MUST NOT be modified.
- Remote: `origin https://github.com/CERALIVE/srt` — do NOT add a remote pointing
  at `Haivision/srt` or any other fork parent (Rule C).
- No CERALIVE CI badge: no CI workflow exists for this vendored fork. Do not add one.
- No `.github/dependabot.yml`: adding it would churn upstream's action pins and is
  out of scope for a vendored build-time source.

## SCOPE BOUNDARY

**No first-party feature work here.** The internals of this repo are out of scope
for CeraLive development. The only file in scope for CeraLive-initiated changes is
`AGENTS.md` itself.

If you need to update the libsrt version consumed by `cerastream` or `srtla`, update
the `srt` `pin:` in `versions.yaml` and re-vendor the source — do not open PRs
against this repo's C source.

## BUILD

Standard CMake. Consumed by `cerastream` and `srtla` as a sibling checkout at
compile time — do not build standalone unless testing the library itself.

```bash
cmake -B build -DENABLE_TESTING=ON -DENABLE_UNITTESTS=ON -DENABLE_BONDING=ON
cmake --build build -j$(nproc)
```

## TEST (ctest)

The GoogleTest unit + bonding suite is gated on `ENABLE_UNITTESTS` (with
`ENABLE_CXX11`, ON by default); `ENABLE_TESTING` additionally builds the developer
test apps. Both are OFF by default. Run the full suite with:

```bash
cmake -B build -DENABLE_TESTING=ON -DENABLE_UNITTESTS=ON -DENABLE_BONDING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Baseline: **286/286 passed** (1 disabled: `CTimer.SleeptoAccuracy`). Note:
`ENABLE_TESTING=ON` alone registers no ctest tests — `ENABLE_UNITTESTS=ON` is what
wires the gtest suite into ctest.

## WHERE TO LOOK

| Need | Location |
|------|----------|
| Library source | `srtcore/` |
| Build config | `CMakeLists.txt` |
| Build options reference | `docs/build/build-options.md` |
| License | `LICENSE` (MPLv2.0) |
| versions.yaml entry | `../versions.yaml` → `srt:` |
