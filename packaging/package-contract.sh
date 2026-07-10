#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
builder="${root}/packaging/build-deb.sh"

bash -n "${builder}"
grep -q 'USE_ENCLIB=gnutls' "${builder}"
grep -q 'libsrt-gnutls.so.1.5' "${builder}"
grep -q 'Provides: libsrt1.5-gnutls (= 1.5.5), libsrt1.5-openssl (= 1.5.5)' "${builder}"
grep -q 'Conflicts: libsrt1.5-gnutls, libsrt1.5-openssl' "${builder}"
