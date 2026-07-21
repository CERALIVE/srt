#!/usr/bin/env bash
set -euo pipefail

# Package-contract test for libsrt1.5-ceralive. Asserts the release metadata
# statically (no build needed) so a stale version/Provides/Conflicts/Replaces or
# a workflow default drifting behind the release is caught before publish.
#
# Single source of truth for the release version:
readonly EXPECT_DEB_VERSION="1.5.6+ceralive.1"
# The versioned virtual-package the fork provides for both Debian TLS flavors.
# Tracks the upstream libsrt release the runtime is built from (v1.5.6).
readonly EXPECT_TLS_VERSION="1.5.6"

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
builder="${root}/packaging/build-deb.sh"
publish="${root}/.github/workflows/publish-release.yml"
runtime="${root}/.github/workflows/runtime-package.yml"

fail() { printf 'package-contract: FAIL: %s\n' "$1" >&2; exit 1; }

bash -n "${builder}"

# --- Single-fork build invariant (GnuTLS, shared, bundled apps) -------------
grep -q 'USE_ENCLIB=gnutls' "${builder}"     || fail "build-deb.sh must set USE_ENCLIB=gnutls"
grep -q 'ENABLE_APPS=ON' "${builder}"        || fail "build-deb.sh must set ENABLE_APPS=ON"
grep -q 'ENABLE_STATIC=OFF' "${builder}"     || fail "build-deb.sh must set ENABLE_STATIC=OFF"
grep -q 'libsrt-gnutls.so.1.5' "${builder}"  || fail "build-deb.sh must ship libsrt-gnutls.so.1.5 alias"

# --- Runtime SONAME present (libsrt.so.1.5) ---------------------------------
grep -qF 'libsrt.so.1.5' "${builder}"        || fail "build-deb.sh must reference the libsrt.so.1.5 runtime SONAME"

# --- Deb version default is the current release -----------------------------
grep -qF "version=\"\${CERALIVE_SRT_VERSION:-${EXPECT_DEB_VERSION}}\"" "${builder}" \
	|| fail "build-deb.sh default version must be ${EXPECT_DEB_VERSION}"

# --- Provides/Conflicts/Replaces for BOTH Debian TLS-flavor packages --------
grep -qF "Provides: libsrt1.5-gnutls (= ${EXPECT_TLS_VERSION}), libsrt1.5-openssl (= ${EXPECT_TLS_VERSION})" "${builder}" \
	|| fail "build-deb.sh Provides must declare both TLS flavors at (= ${EXPECT_TLS_VERSION})"
grep -qF 'Conflicts: libsrt1.5-gnutls, libsrt1.5-openssl' "${builder}" \
	|| fail "build-deb.sh Conflicts must name both TLS flavors"
grep -qF 'Replaces: libsrt1.5-gnutls, libsrt1.5-openssl' "${builder}" \
	|| fail "build-deb.sh Replaces must name both TLS flavors"

# --- Release workflow default version tracks the release (no stale 1.5.5) ---
grep -qF "default: ${EXPECT_DEB_VERSION}" "${publish}" \
	|| fail "publish-release.yml version input default must be ${EXPECT_DEB_VERSION}"
if grep -qE '1\.5\.5' "${publish}"; then
	fail "publish-release.yml still references stale 1.5.5"
fi

# --- runtime-package.yml verify path references the current deb, not 1.5.5 ---
grep -qF "libsrt1.5-ceralive_${EXPECT_DEB_VERSION}_amd64.deb" "${runtime}" \
	|| fail "runtime-package.yml verify-runtime-replacement path must use ${EXPECT_DEB_VERSION}"
if grep -qE '1\.5\.5' "${runtime}"; then
	fail "runtime-package.yml still references stale 1.5.5"
fi

printf 'package-contract: OK (%s / Provides (= %s))\n' "${EXPECT_DEB_VERSION}" "${EXPECT_TLS_VERSION}"
