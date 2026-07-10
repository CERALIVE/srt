#!/usr/bin/env bash
set -euo pipefail

deb="$(readlink -f "${1:?usage: verify-runtime-replacement.sh <libsrt1.5-ceralive.deb>}")"
[[ -f "${deb}" ]] || { printf 'package not found: %s\n' "${deb}" >&2; exit 2; }

apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
	dpkg-dev gstreamer1.0-plugins-bad
plugin="/usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/gstreamer-1.0/libgstsrt.so"
apt-get install -y -qq "${deb}"
apt-get check
dpkg-query -W -f='${Package} ${Version}\n' libsrt1.5-ceralive
! dpkg-query -W libsrt1.5-gnutls >/dev/null 2>&1
ldd "${plugin}" | grep -F 'libsrt-gnutls.so.1.5'
ldconfig -p | grep -E 'libsrt(-gnutls)?\.so\.1\.5'
