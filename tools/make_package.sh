#!/bin/bash
# HMCB_Qt 便携包打包脚本：自包含 tar.gz（程序 + 全部非系统依赖 + Qt 插件 + config + 启动脚本）
# 用法：tools/make_package.sh [构建目录]（缺省 build/）
# 产物：dist/HMCB_Qt_<日期>.tar.gz —— 解包后 ./run.sh 运行，目标机仅需 Ubuntu x86-64 桌面基础库
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build}"
EXE="$BUILD/HMCB_Qt"
MCBCTRL="$ROOT/../libMcbctrl_HMCB_20260421/libmcbctrl.so.1.0.0.131"
STAMP="$(date +%Y%m%d)"
PKG="$ROOT/dist/HMCB_Qt_$STAMP"
QT_PLUGINS="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || echo /usr/lib/x86_64-linux-gnu/qt6/plugins)"

[ -x "$EXE" ] || { echo "错误：找不到 $EXE（先构建）" >&2; exit 1; }
[ -f "$MCBCTRL" ] || { echo "错误：找不到控制库 $MCBCTRL" >&2; exit 1; }

# ---- 1. 依赖收集：对可执行与控制库递归 ldd，排除系统基础库 ----
# 系统名单（目标 Ubuntu 机必有）：加载器/libc/libm/libstdc++/libgcc/pthread 等已并入 libc
SYS_RE='^(linux-vdso|ld-linux|libc\.|libm\.|libstdc\+\+|libgcc_s|libpthread|libdl\.|librt\.)'
declare -A SEEN=()
QUEUE=("$EXE" "$MCBCTRL")
while [ "${#QUEUE[@]}" -gt 0 ]; do
    NEXT=()
    for f in "${QUEUE[@]}"; do
        [ -n "$f" ] || continue
        while read -r name arrow path _rest; do
            [ "$arrow" = "=>" ] || continue
            [ -f "$path" ] || continue
            [[ "$name" =~ $SYS_RE ]] && continue
            [ -n "${SEEN[$path]:-}" ] && continue
            SEEN[$path]=1
            NEXT+=("$path")
        done < <(LD_LIBRARY_PATH="$ROOT/../libMcbctrl_HMCB_20260421" ldd "$f" 2>/dev/null || true)
    done
    if [ "${#NEXT[@]}" -eq 0 ]; then break; fi
    QUEUE=("${NEXT[@]}")
done

rm -rf "$PKG"
mkdir -p "$PKG/bin" "$PKG/lib" "$PKG/plugins/platforms" "$PKG/plugins/imageformats" "$ROOT/dist"

# ---- 2. 主程序 + 控制库 + 依赖 ----
install -m 755 "$EXE" "$PKG/bin/HMCB_Qt"
install -m 644 "$MCBCTRL" "$PKG/lib/"
for p in "${!SEEN[@]}"; do
    install -m 644 "$p" "$PKG/lib/"
done

# Qt 插件：xcb（桌面）+ offscreen/minimal（无头调试）+ png（qrc 图标）
for pl in platforms/libqxcb.so platforms/libqoffscreen.so platforms/libqminimal.so \
          imageformats/libqpng.so; do
    [ -f "$QT_PLUGINS/$pl" ] && install -m 644 "$QT_PLUGINS/$pl" "$PKG/plugins/$pl"
done

# ---- 3. 配置与启动脚本 ----
cp -r "$ROOT/config" "$PKG/config"
cat > "$PKG/run.sh" <<'EOS'
#!/bin/bash
# HMCB_Qt 启动脚本：优先使用包内库与插件
DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$DIR/plugins/platforms"
exec "$DIR/bin/HMCB_Qt" "$@"
EOS
chmod 755 "$PKG/run.sh"

cat > "$PKG/README.txt" <<EOS
HMCB_Qt（HM MCB Test Program，Ubuntu Qt6 移植版）便携包 $STAMP

运行：./run.sh（需 Ubuntu x86-64 桌面环境；无显示调试可用
      QT_QPA_PLATFORM=offscreen ./run.sh）

配置：config/app.ini 切换 mock/仿真 与 real/真实 后端；
      config/MCBCtrl.ini 配设备 IP（[CONNECTION] IPADDR，注释须独立成行）；
      config/MCB_TEST.ini 测量参数与保存开关。

日志：库日志写 <config目录>/HMCBCtrl.log。
EOS

# ---- 4. 打 tar.gz（顶层目录带版本日期）----
tar -C "$ROOT/dist" -czf "$ROOT/dist/HMCB_Qt_$STAMP.tar.gz" "HMCB_Qt_$STAMP"
echo "打包完成：$ROOT/dist/HMCB_Qt_$STAMP.tar.gz"
du -sh "$ROOT/dist/HMCB_Qt_$STAMP.tar.gz" "$PKG"
