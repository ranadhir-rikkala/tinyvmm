#!/usr/bin/env bash
# Downloads a prebuilt x86_64 guest kernel from the Firecracker CI bucket and verifies its checksum.
set -euo pipefail

OUT_DIR="${1:-artifacts}"
KERNEL_KEY="${KERNEL_KEY:-firecracker-ci/v1.11/x86_64/vmlinux-5.10.225-no-acpi}"
KERNEL_SHA256="${KERNEL_SHA256:-3aef288d1707aff40ba21b262fddc0bf217a7b5cb5337da06b65782ec2984328}"
BUCKET_URL="https://s3.amazonaws.com/spec.ccfc.min"

mkdir -p "$OUT_DIR"
dest="$OUT_DIR/$(basename "$KERNEL_KEY")"

if [[ ! -f "$dest" ]]; then
  echo "fetching $KERNEL_KEY" >&2
  curl -fsSL --retry 3 -o "$dest.tmp" "$BUCKET_URL/$KERNEL_KEY"
  mv "$dest.tmp" "$dest"
fi

if [[ -n "$KERNEL_SHA256" ]]; then
  if command -v sha256sum >/dev/null; then
    echo "$KERNEL_SHA256  $dest" | sha256sum -c - >&2
  else
    echo "$KERNEL_SHA256  $dest" | shasum -a 256 -c - >&2
  fi
fi

ln -sf "$(basename "$dest")" "$OUT_DIR/vmlinux"
echo "$OUT_DIR/vmlinux"
