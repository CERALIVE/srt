<!-- Moved verbatim from AGENTS.md on 2026-10-05 by lean-rules-docs-landing-latam -->

## CI COMPILER-CACHE COVERAGE

Every GitHub Actions workflow that performs an ordinary C/C++ build restores a
source-aware ccache archive through `actions/cache/restore@v6` before
compilation. Keys separate the workflow purpose, host OS/architecture, and
relevant matrix dimensions; each has a same-surface restore prefix. Successful
non-PR runs persist the bounded archive through `actions/cache/save@v6`; PRs are
restore-only so untrusted code cannot seed executable compiler output. Every
cache is capped at 200 MB, and CMake is wired through
`CMAKE_C_COMPILER_LAUNCHER=ccache` and
`CMAKE_CXX_COMPILER_LAUNCHER=ccache`.
When a build can generate files beneath a hashed source glob, compute the key
once from the clean post-checkout tree and reuse that immutable value for both
restore and save. The Android workflow does this because `build-android`
creates dependency and ABI output beneath its hashed `scripts/**` tree.
Android also uses content-based compiler identity because `setup-ndk`
materializes NDK r23 on each runner; ccache's default mtime identity would turn
identical compiler bytes with fresh mtimes into cross-run misses. Its versioned
cache namespace is part of that policy so an older mtime-keyed archive cannot
block the first content-keyed save.

| Workflow | Coverage |
|----------|----------|
| `runtime-package.yml` | Existing Docker-mounted ccache, normalized to the shared key/size/launcher contract |
| `publish-release.yml` | Host test build and Docker package build share the restored per-architecture cache |
| `abi.yml` | Separate current/base caches prevent concurrent writers while preserving stable restore prefixes |
| `ubuntu-c++03.yml`, `ubuntu-c++11.yml`, `macos.yml` | Native Makefile builds use the CMake launchers (renamed from `cxx03-ubuntu.yaml` / `cxx11-ubuntu.yaml` / `cxx11-macos.yaml` in the upstream v1.5.6 CI restructure; CeraLive ccache carried onto the renamed files) |
| `windows-msvc-noenc.yml` | Uses Ninja with an explicit x64 MSVC developer environment because CMake compiler launchers are supported by Makefile/Ninja generators, not the Visual Studio generator (renamed from `cxx11-win.yaml`) |
| `ubuntu-mingw.yml` | Upstream v1.5.6 addition (MinGW cross-build, `-DENABLE_UNITTESTS=OFF`); intentionally uncached — no CeraLive ccache precedent and it runs no unit tests |
| `android.yaml`, `iOS.yaml`, `s390x-focal.yaml` | Target/matrix-specific cross-build caches; container builds mount a host-restored cache path |

`codeql.yml` is intentionally uncached: its manual C/C++ build must execute and
trace compiler processes to populate the CodeQL database, while a ccache hit can
bypass the compiler. `codespell.yml` and the `publish` job in
`publish-release.yml` do not compile C/C++ and therefore need no compiler cache.

