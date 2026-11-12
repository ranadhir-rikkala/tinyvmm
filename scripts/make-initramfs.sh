#!/usr/bin/env bash
# Builds a one-file initramfs whose /init prints the boot marker and reboots the guest.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${1:-artifacts}"
CC="${CC:-cc}"

if [[ "$(uname -m)" != "x86_64" && -z "${ALLOW_HOST_ARCH:-}" ]]; then
  echo "make-initramfs.sh builds a native /init; run it on an x86_64 host" >&2
  exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

mkdir -p "$OUT_DIR" "$work/root"
"$CC" -static -Os -s -o "$work/root/init" "$ROOT_DIR/guest/init.c"
(cd "$work/root" && echo init | cpio --quiet -o -H newc --owner=0:0) > "$OUT_DIR/initramfs.cpio"
echo "$OUT_DIR/initramfs.cpio"
