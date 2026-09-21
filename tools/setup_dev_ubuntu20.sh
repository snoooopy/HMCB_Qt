#!/bin/bash
# Ubuntu 20.04 开发环境一键部署（在 HMCB_Qt 仓库内执行）
# 用法：sudo bash tools/setup_dev_ubuntu20.sh
# 做三件事：
#   ① apt 安装编译依赖与 GUI 运行库（已装的秒过）
#   ② 部署 Qt 6.4.3 官方二进制到 /opt/Qt（与打包环境同版本，避免双版本坑）
#   ③ 打印 Qt Creator 套件配置步骤
# Qt 归档（qtbase.7z / icu.7z）已随迁移包携带在 tools/pack_compat/，
# 正常无需联网下载（仅 apt 需要网络）。
set -euo pipefail
cd "$(dirname "$0")/../.."    # HMCB_Qt 仓库根

[ "$(id -u)" = 0 ] || { echo "错误：请用 sudo 运行（apt 与 /opt 需要 root）" >&2; exit 1; }

echo "== 1/3 安装 apt 依赖 =="
export DEBIAN_FRONTEND=noninteractive
apt-get update
# 说明：Qt6 Widgets 的 find_package 需要 OpenGL 开发包（仅运行库不够）；
#       focal 的 xcb-icccm 包名是 libxcb-icccm4；libxcb-xinerama0 是 Qt 官方
#       二进制 xcb 插件的常见缺库；其余为桌面 GUI 运行库（已装则跳过）。
apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git p7zip-full libboost-dev \
    libgl-dev libopengl-dev libglx-dev \
    libxkbcommon0 libxkbcommon-x11-0 libxcb-xinerama0 \
    libxcb1 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
    libxcb-render0 libxcb-render-util0 libxcb-shm0 libxcb-sync1 libxcb-xfixes0 \
    libxcb-xkb1 libxcb-shape0 libx11-6 libxau6 libxdmcp6 \
    libgl1 libegl1 libopengl0 libglx0 \
    libglib2.0-0 libdbus-1-3 libfontconfig1 libfreetype6 libharfbuzz0b \
    libgraphite2-3 libpcre2-16-0 libpcre2-8-0 libdouble-conversion3 libb2-1 \
    libbrotli1 libbz2-1.0 liblzma5 libzstd1 liblz4-1 libpng16-16 libexpat1 \
    libgcrypt20 libgpg-error0 libsystemd0 libcap2 libmd0 libbsd0 \
    qtcreator

echo "== 2/3 部署 Qt 6.4.3 到 /opt/Qt =="
QTDIR=/opt/Qt/6.4.3/gcc_64
if [ -x "$QTDIR/bin/qmake" ]; then
    echo "已存在 $QTDIR，跳过（重装请先 sudo rm -rf /opt/Qt/6.4.3）"
else
    # 归档已在 tools/pack_compat/ 则校验后秒过，否则从清华/USTC 镜像下载
    bash tools/pack_compat/fetch_qt_base.sh
    7z x -y tools/pack_compat/qtbase.7z -o/opt/Qt >/dev/null
    7z x -y tools/pack_compat/icu.7z   -o/opt/Qt >/dev/null
    # 7z 保留归档内 700 权限：不 chmod 则非 root 的 Qt Creator 读不到 → find_package 静默失败
    chmod -R a+rX /opt/Qt
    echo "Qt 6.4.3 部署完成：$QTDIR"
fi

echo "== 3/3 完成。Qt Creator 套件配置（一次性手工操作）=="
cat <<EOF

  1. 启动 Qt Creator（应用程序菜单，或命令 qtcreator）
  2. 菜单 工具(Tools) → 选项(Options) → Kits → Qt Versions 标签 → Add：
         $QTDIR/bin/qmake
  3. Kits 标签 → 选中 Desktop 套件 → Qt Version 改为上面新增的 Qt 6.4.3
     （编译器 GCC、CMake /usr/bin/cmake 一般已自动探测到）
  4. 打开工程：HMCB_Qt/CMakeLists.txt（推荐，调试体验最佳）
     或 HMCB_Qt.pro（qmake 镜像工程，二者等价）

  编译控制库（一条命令，改库源码后重跑）：
     cd libMcbctrl_HMCB_20260421 && ./build.sh

  提示：apt 的 qtcreator 为 4.11（较老，Qt6 CMake 支持一般）。想要新版可从
  清华镜像下载官方自包含包（解压运行 bin/qtcreator.sh）：
     https://mirrors.tuna.tsinghua.edu.cn/qt/official_releases/qtcreator/
EOF
