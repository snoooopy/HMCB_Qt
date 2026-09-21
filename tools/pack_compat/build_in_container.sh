#!/bin/bash
# 容器内构建入口（由 make_compat_package.sh 以 ubuntu:20.04 镜像调起）
#   /src  = 宿主 HMCB_Demo 只读挂载；/out = 宿主 HMCB_Qt/dist 挂载
# 流程：复制源码到 /work（不污染仓库）→ 20.04 重编 libmcbctrl → Qt 6.4.3 构建 + ctest
#       → make_package.sh 收集依赖打包 → 符号审计 → 产物落 /out
set -euo pipefail
SRC=/src
OUT=/out

WORK="$(mktemp -d)"
DEMO="$WORK/HMCB_Demo"
cp -a "$SRC" "$DEMO"
QTDIR=/opt/Qt/6.4.3/gcc_64

# 清掉宿主构建树/产物（CMake 缓存指向宿主路径，容器内必炸；dist 223M 白拷）
rm -rf "$DEMO/HMCB_Qt/build" "$DEMO/HMCB_Qt/build_qmake" "$DEMO/HMCB_Qt/dist"

# ---- 1. libmcbctrl（g++ 9；GCC9 的 std::filesystem 在 libstdc++fs，须显式链接）----
# -Wl,-soname：无 SONAME 时 cmake3.16/gcc9 会在可执行文件 NEEDED 里记相对路径，
# 仅 build 根 cwd 可解析（ctest 从 build/tests 跑即断链）；加 SONAME 后记纯文件名，
# 由 RUNPATH 定位，与宿主（cmake3.28/gcc13）产物一致。
cd "$DEMO/libMcbctrl_HMCB_20260421"
g++ -shared -fPIC -std=c++17 -O2 \
    $(ls src/*.cpp | grep -vE "RequestSocket_qt|StdAfx|ECS_funcnew") \
    -Isrc -Wl,-soname,libmcbctrl.so.1.0.0.131 \
    -o libmcbctrl.so.1.0.0.131 -lpthread -lstdc++fs
echo "== libmcbctrl 编译完成（20.04/g++9）=="

# ---- 2. HMCB_Qt（Qt 6.4.3 官方二进制）----
cd "$DEMO/HMCB_Qt"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$QTDIR"
cmake --build build -j"$(nproc)"
echo "== HMCB_Qt 构建完成 =="

# 注意：focal cmake 3.16 无 ctest --test-dir（3.20 才有），用 cd 形式
(cd build && ctest --output-on-failure)
echo "== ctest 通过 =="

# ---- 3. 打包（递归 ldd 收集本容器[20.04]的依赖库）----
tools/make_package.sh build

# ---- 4. 符号审计：全部 ELF 需求 ≤ GLIBC_2.31 / GLIBCXX_3.4.28（20.04 天花板）----
STAMP="$(date +%Y%m%d)"
PKG="$DEMO/HMCB_Qt/dist/HMCB_Qt_$STAMP"
bash "$SRC/HMCB_Qt/tools/audit_symbols.sh" "$PKG" 2.31 3.4.28

# ---- 5. 产物出容器（tar.gz + 目录均保留）----
cp "$DEMO/HMCB_Qt/dist/HMCB_Qt_$STAMP.tar.gz" "$OUT/"
echo "== 兼容包完成：/out/HMCB_Qt_$STAMP.tar.gz =="
du -sh "$OUT/HMCB_Qt_$STAMP.tar.gz"
