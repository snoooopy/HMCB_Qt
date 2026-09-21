#!/bin/bash
# 兼容包冒烟：干净 ubuntu:20.04 / ubuntu:22.04 容器 + 宿主（24.04）三环境运行 run.sh。
# 干净容器只有 glibc 无任何 GUI 库 —— 等价于验证"包自带一切，仅依赖系统 glibc/libstdc++"。
# 用法：tools/pack_compat/smoke_compat.sh [dist包目录（缺省取最新 dist/HMCB_Qt_<日期>）]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"   # 本脚本位于 tools/pack_compat/ 下
PKG="${1:-$(find "$ROOT/dist" -maxdepth 1 -type d -name 'HMCB_Qt_2*' | sort | tail -1)}"
[ -d "$PKG" ] || { echo "错误：找不到包目录 $PKG" >&2; exit 1; }
echo "冒烟对象：$PKG"

run_case() {  # $1=标签 $2=命令包裹（stdin 脚本）
    local label="$1" script="$2"
    echo "== [$label] =="
    if bash -c "$script"; then echo "== [$label] PASS =="; else echo "== [$label] FAIL =="; return 1; fi
}

FAIL=0
for v in 20.04 22.04; do
    # -i 必须有：否则 stdin 不进容器，bash -s 读 EOF 直接退出（假阳性）
    docker run -i --rm -v "$PKG":/pkg:ro ubuntu:$v bash -s <<'EOS' || FAIL=1
set -u
export QT_QPA_PLATFORM=offscreen
cd /pkg
timeout --signal=KILL 20 ./run.sh >/tmp/smoke.log 2>&1
code=$?
if [ $code -eq 124 ] || [ $code -eq 137 ]; then
    if grep -qiE 'GLIBC.{0,3}not found|GLIBCXX.{0,3}not found|cannot open shared' /tmp/smoke.log; then
        echo "缺失符号/库："; grep -iE 'not found|cannot open shared' /tmp/smoke.log; exit 1
    fi
    echo "ALIVE（20s 存活后被超时杀掉，无符号缺失）"; exit 0
fi
echo "提前退出 exit=$code"; tail -20 /tmp/smoke.log; exit 1
EOS
done

# 宿主 24.04：真实 GUI 库环境（xcb 插件加载路径一并覆盖）
echo "== [host-24.04 offscreen] =="
if (cd "$PKG" && QT_QPA_PLATFORM=offscreen timeout --signal=KILL 20 ./run.sh >/tmp/smoke2404.log 2>&1; code=$?
     if [ $code -eq 124 ] || [ $code -eq 137 ]; then echo "ALIVE"; exit 0; fi
     echo "exit=$code"; tail -20 /tmp/smoke2404.log; exit 1); then :; else FAIL=1; fi

[ $FAIL -eq 0 ] && echo "== 三环境冒烟全部通过 ==" || echo "== 存在失败 =="
exit $FAIL
