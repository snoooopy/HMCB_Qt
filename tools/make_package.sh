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

# ---- 1. 依赖收集：对可执行/控制库/Qt 插件递归 ldd，排除系统基础库 ----
# 系统名单（目标 Ubuntu 机必有）：加载器/libc/libm/libstdc++/libgcc/pthread 等已并入 libc
SYS_RE='^(linux-vdso|ld-linux|libc\.|libm\.|libstdc\+\+|libgcc_s|libpthread|libdl\.|librt\.)'
# Qt 插件也要进闭包（真实事故：libqxcb 依赖的 libQt6XcbQpa/libQt6OpenGL 不收集，
# 目标机系统 Qt 版本不同时进程混载两版 Qt 直接 abort）
PLUGINS=()
for pl in platforms/libqxcb.so platforms/libqoffscreen.so platforms/libqminimal.so \
          imageformats/libqpng.so; do
    [ -f "$QT_PLUGINS/$pl" ] && PLUGINS+=("$QT_PLUGINS/$pl")
done
declare -A SEEN=()
MISSING=()
QUEUE=("$EXE" "$MCBCTRL" "${PLUGINS[@]}")
while [ "${#QUEUE[@]}" -gt 0 ]; do
    NEXT=()
    for f in "${QUEUE[@]}"; do
        [ -n "$f" ] || continue
        while read -r name arrow path _rest; do
            [ "$arrow" = "=>" ] || continue
            if [ "$path" = "not" ]; then   # "libX => not found"：闭包缺口必须暴露
                [[ "$name" =~ $SYS_RE ]] || MISSING+=("$name <- $f")
                continue
            fi
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
if [ "${#MISSING[@]}" -gt 0 ]; then
    echo "错误：依赖闭包缺口（解析 not found，打包中止）：" >&2
    printf '  %s\n' "${MISSING[@]}" >&2
    exit 1
fi

rm -rf "$PKG"
mkdir -p "$PKG/bin" "$PKG/lib" "$PKG/plugins/platforms" "$PKG/plugins/imageformats" "$ROOT/dist"

# ---- 2. 主程序 + 控制库 + 依赖 ----
install -m 755 "$EXE" "$PKG/bin/HMCB_Qt"
install -m 644 "$MCBCTRL" "$PKG/lib/"
for p in "${!SEEN[@]}"; do
    install -m 644 "$p" "$PKG/lib/"
done

# Qt 插件：xcb（桌面）+ offscreen/minimal（无头调试）+ png（qrc 图标）
for src in "${PLUGINS[@]}"; do
    rel="${src#"$QT_PLUGINS"/}"
    install -m 644 "$src" "$PKG/plugins/$rel"
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

# ---- 4. 闭包自检：包内全部 ELF 在仅挂包 lib 的条件下不得有 not found ----
# （模拟目标机"包自带一切"；白名单系统库由宿主/目标机提供，可解析到即不报）
SELFTEST_FAIL=0
while IFS= read -r -d '' f; do
    bad=$(LD_LIBRARY_PATH="$PKG/lib" ldd "$f" 2>/dev/null | grep "=> not found" || true)
    if [ -n "$bad" ]; then
        echo "自检失败：$f" >&2; printf '%s\n' "$bad" >&2
        SELFTEST_FAIL=1
    fi
done < <(find "$PKG/bin" "$PKG/lib" "$PKG/plugins" -type f -print0)
[ $SELFTEST_FAIL -eq 0 ] && echo "闭包自检通过：包内 ELF 无 not found"
[ $SELFTEST_FAIL -eq 0 ] || exit 1

# ---- 5. 打 tar.gz（顶层目录带版本日期）----
tar -C "$ROOT/dist" -czf "$ROOT/dist/HMCB_Qt_$STAMP.tar.gz" "HMCB_Qt_$STAMP"
echo "打包完成：$ROOT/dist/HMCB_Qt_$STAMP.tar.gz"
du -sh "$ROOT/dist/HMCB_Qt_$STAMP.tar.gz" "$PKG"
