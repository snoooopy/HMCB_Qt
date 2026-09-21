#!/bin/bash
# 导出「Ubuntu 20.04 开发迁移包」到 ../HMCB_Ubuntu20/
# 内容：
#   HMCB_Qt/                    —— git 仓库（保留 .git 全部历史 + 当前未提交修改，
#                                 如 config/app.ini 的 Mode=real），排除构建产物
#   libMcbctrl_HMCB_20260421/   —— git 仓库 + 当前 .so（排除 .orig/.v3 等历史备份）
#   README_开始开发.md          —— 新机器三步开始说明
# Qt 6.4.3 离线归档（qtbase.7z/icu.7z）随 tools/pack_compat/ 一起带走，
# 新机器 setup_dev_ubuntu20.sh 无需联网下载 Qt。
# 可重复执行（rsync --delete 增量同步）。
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"    # HMCB_Qt
DEMO="$(cd "$ROOT/.." && pwd)"              # HMCB_Demo
DEST="$DEMO/HMCB_Ubuntu20"

echo "== 导出 HMCB_Qt → $DEST/HMCB_Qt =="
mkdir -p "$DEST"
rsync -a --delete \
    --exclude build/ --exclude build_qmake/ --exclude build-qmake/ \
    --exclude dist/ --exclude .cache/ --exclude .qt/ --exclude .qtcreator/ \
    --exclude '*.user' --exclude '*.user.*' --exclude '*.autosave' \
    --exclude compile_commands.json --exclude '*.stash' \
    --exclude 'CMakeFiles/' --exclude 'CMakeCache.txt' --exclude 'Testing/' \
    --exclude '*_autogen/' \
    "$ROOT/" "$DEST/HMCB_Qt/"

echo "== 导出 libMcbctrl → $DEST/libMcbctrl_HMCB_20260421 =="
rsync -a --delete \
    --exclude '*.orig' --exclude '*.orig2' \
    --exclude '*.v3' --exclude '*.v4' --exclude '*.v5' \
    --exclude '*.fix' --exclude '*.fix2' --exclude '*.new' --exclude '*.bak' \
    --exclude '*.aps' --exclude '*.clw' --exclude '*.pcap' \
    "$DEMO/libMcbctrl_HMCB_20260421/" "$DEST/libMcbctrl_HMCB_20260421/"

QT_VER="$(git -C "$ROOT" rev-parse --short HEAD)"
LIB_VER="$(git -C "$DEMO/libMcbctrl_HMCB_20260421" rev-parse --short HEAD)"
STAMP="$(date +%Y-%m-%d)"
cat > "$DEST/README_开始开发.md" <<EOF
# HMCB_Ubuntu20 —— Ubuntu 20.04 开发迁移包

导出于 $STAMP（HMCB_Qt @ $QT_VER，libmcbctrl @ $LIB_VER）。
两目录必须保持同级（CMake 与 .pro 都按 \`../libMcbctrl_HMCB_20260421\` 相对引用）。

## 新机器三步开始

1. 安装依赖 + 部署 Qt 6.4.3（Qt 归档已随包携带，无需联网下载）：

       sudo bash HMCB_Qt/tools/setup_dev_ubuntu20.sh

2. 编译控制库（一条命令；包内已带预编译 .so，可先跳过直接开发）：

       cd libMcbctrl_HMCB_20260421 && ./build.sh

3. Qt Creator 打开 HMCB_Qt/CMakeLists.txt（或 HMCB_Qt.pro），
   套件(Qt Version)指向 /opt/Qt/6.4.3/gcc_64/bin/qmake（setup 末尾有图文步骤）。

## 说明

- 两个目录都是完整 git 仓库：在新机器继续用 git 提交/回溯；导出时连未提交
  修改（如 config/app.ini 的 Mode=real）一并带上了。
- 设备连接：config/MCBCtrl.ini 的 [CONNECTION] IPADDR（注释必须独立成行）；
  后端模式在 config/app.ini [BACKEND] Mode = mock / real。
- 出多版本兼容发布包：HMCB_Qt/tools/pack_compat/make_compat_package.sh（需 Docker）。
- apt 的 qtcreator(4.11) 较老，推荐清华镜像的官方自包含包：
  https://mirrors.tuna.tsinghua.edu.cn/qt/official_releases/qtcreator/
EOF

echo "== 导出完成 =="
du -sh "$DEST/HMCB_Qt" "$DEST/libMcbctrl_HMCB_20260421" "$DEST"
