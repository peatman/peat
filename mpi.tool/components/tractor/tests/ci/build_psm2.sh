#!/bin/bash
# Build libpsm2 (Cornelis Networks opa-psm2) from source for CI:
#
#   ./build_psm2.sh <version> <prefix>        e.g. ./build_psm2.sh 12.0.1 $HOME/psm2
#
# Installs <prefix>/usr/lib64/libpsm2.so* and <prefix>/usr/include/psm2*.h; use PSM2PATH=<prefix>/usr.
# Why not the distro package: Ubuntu 24.04's libpsm2 11.2.185 fails psm2_init ("PSM Unresolved internal error") on a
# machine without an Omni-Path HFI even with PSM2_DEVICES=self,shm; 12.0.1 runs shm-only there.
# Two build fixes for current toolchains: its private static strlcat() clashes with glibc >= 2.38's, so it is renamed;
# newer gcc warnings would stop the -Werror build, so WERROR is cleared. Needs libnuma-dev.

set -euo pipefail

ver=${1:?usage: build_psm2.sh <version> <prefix>}
prefix=${2:?prefix}
work=${BUILD_PSM2_WORKDIR:-${RUNNER_TEMP:-$(mktemp -d)}}

mkdir -p "$work" && cd "$work"
rm -rf "opa-psm2-PSM2_$ver"
curl -sSfL --retry 3 -o "psm2-$ver.tar.gz" "https://github.com/cornelisnetworks/opa-psm2/archive/refs/tags/PSM2_$ver.tar.gz"
tar -xzf "psm2-$ver.tar.gz"
cd "opa-psm2-PSM2_$ver"
sed -i 's/\bstrlcat\b/psmi_strlcat/g' ptl_ips/ips_proto_dump.c
make -j"$(nproc)" WERROR= > make.log 2>&1 || { tail -n 40 make.log; exit 1; }
make install WERROR= DESTDIR="$prefix" LIBDIR=/usr/lib64 > install.log 2>&1 || { tail -n 40 install.log; exit 1; }
echo "installed libpsm2 $ver in $prefix/usr (PSM2PATH)"
