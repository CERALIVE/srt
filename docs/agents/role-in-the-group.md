<!-- Moved verbatim from AGENTS.md on 2026-10-05 by lean-rules-docs-landing-latam -->

## ROLE IN THE GROUP

`srt` is the CeraLive runtime fork of [Haivision/srt](https://github.com/Haivision/srt).

- **SONAME:** `libsrt.so.1.5`
- **Runtime package:** `libsrt1.5-ceralive` for arm64 and amd64, built by
  `packaging/build-deb.sh` with the GnuTLS backend. Current version
  **`1.5.6+ceralive.1`** (`master` carries upstream v1.5.6, incl. the KMREQ CVE
  fixes). The `+`-bearing version is deliberate — see the packaging contract below.
- **Device use:** image-building-pipeline stages it from apt.ceralive.tv and
  installs it before `cerastream`. It replaces the Debian GnuTLS/OpenSSL flavors
  and provides their virtual package names; GStreamer and cerastream resolve one
  CeraLive `libsrt.so.1.5` ABI in each process.
- **Consumers:** `cerastream` (direct FFI) and GStreamer runtime components.
- `irl-srt-server` uses system libsrt (deployment-dependent version), not this fork.

### Bundled SRT tools (`srt-live-transmit`, `srt-file-transmit`, `srt-tunnel`)

`libsrt1.5-ceralive` also ships the SRT sample command-line tools in `/usr/bin`
(built with `-DENABLE_APPS=ON`). `srt-live-transmit` is the one the device path
needs (SRT↔UDP relay); the other two ride along because they share the same build
and dependencies. They are the single first-party source of `srt-live-transmit` —
no separate `srt-tools`-style package exists (see below for why).

**Single-fork invariant is preserved.** The tools are built with
`-DENABLE_STATIC=OFF -DENABLE_SHARED=ON`, so they dynamically link the *same*
shared `libsrt.so.1.5` shipped in this package (GnuTLS via `USE_ENCLIB=gnutls`).
They pull in **no** OpenSSL and no second libsrt — verified in CI: the package
build asserts `srt-live-transmit`'s `NEEDED` includes `libsrt.so.1.5` and excludes
`libssl`/`libcrypto` (`publish-release.yml`), and `verify-runtime-replacement.sh`
re-checks the installed binary's `ldd`. `package-contract.sh` locks
`ENABLE_APPS=ON` + `ENABLE_STATIC=OFF` so the tools can never silently drop out or
gain a static/second-flavor libsrt.

### Runtime-package version contract (#20)

`packaging/package-contract.sh` is a STATIC gate — it reads the packaging sources
and workflows, so it runs without building. It locks, in addition to the
`ENABLE_APPS`/`ENABLE_STATIC` flags above:

| Locked | Current value |
|--------|---------------|
| `.deb` version | `1.5.6+ceralive.1` (`CERALIVE_SRT_VERSION` default in `build-deb.sh`) |
| `Provides` | `libsrt1.5-gnutls (= 1.5.6)`, `libsrt1.5-openssl (= 1.5.6)` |
| `Conflicts` / `Replaces` | `libsrt1.5-gnutls`, `libsrt1.5-openssl` |
| SONAME | `libsrt.so.1.5` |
| No stale pin | neither `publish-release.yml` nor `runtime-package.yml` may retain a `1.5.5` reference |

The versioned `Provides` is what lets this package *replace* both Debian TLS
flavours rather than co-install beside them — the single-fork invariant depends on
the `(= 1.5.6)` upstream version matching what Debian's flavours would satisfy, so
the package version and the `Provides` version move together but are NOT the same
string (`1.5.6+ceralive.1` vs `1.5.6`).

**The `+` is load-bearing downstream.** apt's https method percent-encodes `+` as
`%2B` in the fetch URL while R2 stores the literal `+`, which 404'd every
`+`-versioned `.deb` on-device until `apt-worker` #23 keyed lookups on the decoded
path. Keep that in mind before assuming a `+` version is "just cosmetic": see
`apt-worker/AGENTS.md` → "R2 keys are matched on the percent-DECODED path".

**Why bundled, not a separate `srt-tools-ceralive` package.** The apt publish path
(`apt-worker/scripts/reindex.sh`) downloads *every* `.deb` in a release tag and
hard-fails (`validate_deb`, G-B) if any package name ≠ the dispatched `COMPONENT`,
and its `VALID_COMPONENTS` allowlist has no tools package. A separate package would
therefore require an apt-worker change (allowlist + per-package reindex) plus an
image-pipeline `FIRST_PARTY_APT_PKGS` entry. Bundling keeps the whole change inside
`srt`: one package, the existing `libsrt1.5-ceralive` component, the existing
release + `apt-reindex` dispatch — all unchanged. The package already shipped
`/usr/bin/srt-ffplay` and dev headers, so it was never a pure shared-object package.
Consequence for the image: once the device installs `libsrt1.5-ceralive`,
`srt-live-transmit` is present with no new package to fetch or pin.

Cross-check: root `versions.yaml` entry `srt`, image
`v2/lib/fetch-debs.sh`, and `cerastream` packaging must all name the same release.

