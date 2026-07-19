#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
arch="${DEB_ARCH:-$(dpkg --print-architecture)}"
version="${CERALIVE_SRT_VERSION:-1.5.5+ceralive.1}"
triplet="$(dpkg-architecture -a "${arch}" -qDEB_HOST_MULTIARCH)"
build_dir="${BUILD_DIR:-${root}/build-deb-${arch}}"
stage_dir="${STAGE_DIR:-${root}/stage-deb-${arch}}"
out_dir="${OUT_DIR:-${root}/dist}"

case "${arch}" in
	arm64|amd64) ;;
	*) echo "unsupported Debian architecture: ${arch}" >&2; exit 2 ;;
esac

rm -rf "${build_dir}" "${stage_dir}"
# ENABLE_APPS=ON + ENABLE_STATIC=OFF makes the sample tools link the shared libsrt.so.1.5
# built here, so they load the one CeraLive GnuTLS fork — never a second SRT/TLS flavor.
cmake -S "${root}" -B "${build_dir}" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR="lib/${triplet}" \
	-DENABLE_APPS=ON \
	-DENABLE_SHARED=ON \
	-DENABLE_STATIC=OFF \
	-DENABLE_TESTING=OFF \
	-DENABLE_UNITTESTS=OFF \
	-DUSE_ENCLIB=gnutls
cmake --build "${build_dir}" --parallel
DESTDIR="${stage_dir}" cmake --install "${build_dir}"

libdir="${stage_dir}/usr/lib/${triplet}"
real_lib="$(readlink -f "${libdir}/libsrt.so.1.5")"
ln -s "$(basename "${real_lib}")" "${libdir}/libsrt-gnutls.so.1.5"
ln -s "libsrt-gnutls.so.1.5" "${libdir}/libsrt-gnutls.so"

install -Dm755 "${root}/packaging/postinst" "${stage_dir}/DEBIAN/postinst"
install -Dm755 "${root}/packaging/postrm" "${stage_dir}/DEBIAN/postrm"
cat >"${stage_dir}/DEBIAN/control" <<EOF
Package: libsrt1.5-ceralive
Version: ${version}
Architecture: ${arch}
Maintainer: CERALIVE <contact@ceralive.tv>
Depends: libc6 (>= 2.34), libgnutls30 (>= 3.7.9), libstdc++6 (>= 11)
Provides: libsrt1.5-gnutls (= 1.5.5), libsrt1.5-openssl (= 1.5.5)
Conflicts: libsrt1.5-gnutls, libsrt1.5-openssl
Replaces: libsrt1.5-gnutls, libsrt1.5-openssl
Section: libs
Priority: optional
Homepage: https://github.com/CERALIVE/srt
License: MPL-2.0
Description: CeraLive fork of libsrt with a unified GnuTLS runtime ABI
 The CeraLive streaming stack's single runtime SRT library. It provides both
 Debian TLS-flavor package names so GStreamer and direct FFI consumers load the
 same forked shared object.
 .
 This package also ships the SRT sample command-line tools (srt-live-transmit,
 srt-file-transmit, srt-tunnel), linked against the same GnuTLS libsrt.so.1.5 so
 no second SRT/TLS flavor is introduced, plus the srt-ffplay helper script.
EOF

mkdir -p "${out_dir}"
deb="${out_dir}/libsrt1.5-ceralive_${version}_${arch}.deb"
dpkg-deb --root-owner-group --build "${stage_dir}" "${deb}"
printf '%s\n' "${deb}"
