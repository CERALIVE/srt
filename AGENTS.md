# srt

Parent: [workspace rules](https://github.com/CERALIVE/ceralive/blob/master/AGENTS.md)

<!-- workspace-hard-rules:begin -->
## Workspace hard rules (identical in every CeraLive AGENTS.md)
- Commits and PRs carry the human author only: no Co-authored-by, no AI attribution.
- Start from the updated canonical branch; rebase to update; never `reset --hard` or discard others' work.
- One focused PR per repo, opened against CERALIVE/<repo>; the root policy PR merges first.
- A repo is self-contained: no path above its root; consume @ceralive packages from the registry, never link:/file:.
- Never delete, skip or weaken a test; every behavior change ships with a test.
- A user-visible change updates docs.ceralive.tv in English and Spanish (es-419), and any ceralive.tv claim it touches, in the same release.
- AGENTS.md holds rules and routing only, within budget; contracts and history live in docs/agents/.
- Full canon: https://github.com/CERALIVE/ceralive/blob/master/AGENTS.md
<!-- workspace-hard-rules:end -->

## ROLE

CeraLive libsrt runtime fork; device consumers resolve one libsrt ABI. Canonical branch: `master`.

## STRUCTURE

`srtcore/`, `haicrypt/`, `common/`, `apps/`, `test/`, `testing/`, `packaging/`, `scripts/`, `docs/`, `.github/`.

## COMMANDS

```bash
bash packaging/package-contract.sh
cmake -S . -B _build -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_COMPILE_WARNING_AS_ERROR=ON -DUSE_CXX_STD=11 -DENABLE_STDCXX_SYNC=ON -DENABLE_ENCRYPTION=ON -DENABLE_UNITTESTS=ON -DENABLE_BONDING=ON -DENABLE_TESTING=ON -DENABLE_EXAMPLES=ON -DENABLE_DEBUG=ON -DENABLE_CODE_COVERAGE=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build _build
ctest --test-dir _build --extra-verbose
```

## WHERE TO LOOK

| Task / code surface | Contract |
|---|---|
| Before changing anything else here, open docs/agents/README.md and read the contract for the subsystem you touch | [Contract index](docs/agents/README.md) |
| Overview | [overview.md](docs/agents/overview.md) |
| ROLE IN THE GROUP | [role-in-the-group.md](docs/agents/role-in-the-group.md) |
| UPSTREAM-FORK RELATIONSHIP | [upstream-fork-relationship.md](docs/agents/upstream-fork-relationship.md) |
| SANCTIONED CERALIVE PATCH — `SRTO_REORDERFREEZE` | [sanctioned-ceralive-patch-srto-reorderfreeze.md](docs/agents/sanctioned-ceralive-patch-srto-reorderfreeze.md) |
| BASELINE PATCH STATUS (ADR-002 "C is SAFE") | [baseline-patch-status-adr-002-c-is-safe.md](docs/agents/baseline-patch-status-adr-002-c-is-safe.md) |
| RECEIVER CAPABILITY RECONCILIATION | [receiver-capability-reconciliation.md](docs/agents/receiver-capability-reconciliation.md) |
| SCOPE BOUNDARY | [scope-boundary.md](docs/agents/scope-boundary.md) |
| COMMON TASKS | [common-tasks.md](docs/agents/common-tasks.md) |
| CI COMPILER-CACHE COVERAGE | [ci-compiler-cache-coverage.md](docs/agents/ci-compiler-cache-coverage.md) |
| BUILD | [build.md](docs/agents/build.md) |
| TEST (ctest) | [test-ctest.md](docs/agents/test-ctest.md) |
| WHERE TO LOOK | [where-to-look.md](docs/agents/where-to-look.md) |

## HARD RULES

- Canonical release branch is `master`; versions use upstream-style `1.5.x+ceralive.N`, not CalVer.
- `libsrt1.5-ceralive` is the sole device runtime provider of `libsrt.so.1.5`; it replaces both Debian TLS-flavor packages.
- Bundled tools dynamically link that same GnuTLS libsrt; never introduce a static or second-flavor runtime.
- Keep versioned Provides aligned with the upstream ABI version; preserve the SONAME and runtime replacement gate.
- Never modify upstream `CONTRIBUTING.md` or retain fork-parent remotes.
- Preserve option enum IDs and default-off reorder-freeze semantics; do not couple decay-freeze to NAK reporting.
- Advance the immutable ABI baseline only after the next CeraLive runtime release is published.
- Do not relax or retry away socket ownership, teardown, or connection-timeout test assertions.
- No unsanctioned C/C++ feature work; packaging changes must preserve the runtime ABI.
