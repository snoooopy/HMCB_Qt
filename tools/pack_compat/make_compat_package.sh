#!/bin/bash
# HMCB_Qt 多版本 Ubuntu 兼容打包入口（宿主机执行）
# 产出 dist/HMCB_Qt_<日期>.tar.gz —— 兼容 Ubuntu 20.04 / 22.04 / 24.04+（x86-64）
# 用法：tools/pack_compat/make_compat_package.sh [--no-cache]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"   # HMCB_Qt（本脚本位于 tools/pack_compat/ 下）
DEMO="$(cd "$ROOT/.." && pwd)"                # HMCB_Demo
IMAGE=hmcb-qt-build:focal

BUILD_ARGS=()
[ "${1:-}" = "--no-cache" ] && BUILD_ARGS+=(--no-cache)

# 宿主下载并校验 Qt qtbase 归档（容器内下载会损坏，见 Dockerfile 注释）
bash "$ROOT/tools/pack_compat/fetch_qt_base.sh"

docker build -t "$IMAGE" "${BUILD_ARGS[@]}" "$ROOT/tools/pack_compat"

docker run --rm \
    -v "$DEMO":/src:ro \
    -v "$ROOT/dist":/out \
    -u "$(id -u):$(id -g)" \
    "$IMAGE" bash /src/HMCB_Qt/tools/pack_compat/build_in_container.sh
