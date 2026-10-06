# Agent contract index

Every original AGENTS.md block is preserved below. Read the relevant contract before editing.

| Original heading | Contract | Governed paths / tasks |
|---|---|---|
| Overview | [overview.md](overview.md) | `../AGENTS.md` |
| ROLE IN THE GROUP | [role-in-the-group.md](role-in-the-group.md) | `packaging/build-deb.sh`, `/usr/bin`, `/`, `publish-release.yml`, `verify-runtime-replacement.sh`, `package-contract.sh`, `packaging/package-contract.sh`, `build-deb.sh` |
| UPSTREAM-FORK RELATIONSHIP | [upstream-fork-relationship.md](upstream-fork-relationship.md) | `srt/CONTRIBUTING.md`, `origin https://github.com/CERALIVE/srt`, `Haivision/srt`, `runtime-package.yml`, `.github/dependabot.yml`, `.github/workflows/abi.yml` |
| SANCTIONED CERALIVE PATCH — `SRTO_REORDERFREEZE` | [sanctioned-ceralive-patch-srto-reorderfreeze.md](sanctioned-ceralive-patch-srto-reorderfreeze.md) | `srtcore/srt.h`, `srtcore/core.cpp`, `// CERALIVE reorder-freeze`, `test/test_socket_options.cpp`, `/` |
| BASELINE PATCH STATUS (ADR-002 "C is SAFE") | [baseline-patch-status-adr-002-c-is-safe.md](baseline-patch-status-adr-002-c-is-safe.md) | `srtcore/filelist.maf`, `fec.cpp`, `packetfilter.cpp`, `srtla/test-results/fec-capability-probe.json` |
| RECEIVER CAPABILITY RECONCILIATION | [receiver-capability-reconciliation.md](receiver-capability-reconciliation.md) | `docs/RECEIVER-RECONCILIATION.md`, `srtla/docs/adr/ADR-002-srt-patch-necessity.md` |
| SCOPE BOUNDARY | [scope-boundary.md](scope-boundary.md) | SCOPE BOUNDARY |
| COMMON TASKS | [common-tasks.md](common-tasks.md) | `packaging/build-deb.sh`, `dist/libsrt1.5-ceralive_*.deb`, `/usr/bin/srt-live-transmit`, `packaging/verify-runtime-replacement.sh <deb>`, `AGENTS.md`, `runtime-package.yml` |
| CI COMPILER-CACHE COVERAGE | [ci-compiler-cache-coverage.md](ci-compiler-cache-coverage.md) | `actions/cache/restore@v6`, `actions/cache/save@v6`, `scripts/**`, `runtime-package.yml`, `publish-release.yml`, `abi.yml`, `ubuntu-c++03.yml`, `ubuntu-c++11.yml` |
| BUILD | [build.md](build.md) | BUILD |
| TEST (ctest) | [test-ctest.md](test-ctest.md) | TEST (ctest) |
| WHERE TO LOOK | [where-to-look.md](where-to-look.md) | `srtcore/`, `srtcore/srt.h`, `srtcore/socketconfig.h`, `srtcore/socketconfig.cpp`, `srtcore/core.cpp`, `// CERALIVE reorder-freeze`, `docs/build/build-options.md`, `../versions.yaml` |
