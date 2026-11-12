#!/usr/bin/env bash
# Builds tinyvmm and runs the unit tests inside a Linux container (useful on macOS).
# Usage: scripts/docker-test.sh [linux/arm64|linux/amd64]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PLATFORM="${1:-}"
TAG="tinyvmm-dev"
platform_args=()
build_dir="build-docker"
if [[ -n "$PLATFORM" ]]; then
  platform_args=(--platform "$PLATFORM")
  TAG="tinyvmm-dev:${PLATFORM#linux/}"
  build_dir="build-${PLATFORM#linux/}"
fi

docker build "${platform_args[@]}" -t "$TAG" -f "$ROOT_DIR/docker/Dockerfile" "$ROOT_DIR/docker"
docker run --rm "${platform_args[@]}" -v "$ROOT_DIR:/src" -w /src "$TAG" bash -c "
  cmake -S . -B $build_dir -G Ninja -DTINYVMM_WERROR=ON &&
  cmake --build $build_dir &&
  ctest --test-dir $build_dir --output-on-failure
"
