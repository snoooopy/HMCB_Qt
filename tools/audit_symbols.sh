#!/bin/bash
# dist 包符号审计：断言包内全部 ELF 的 glibc/libstdc++ 符号版本需求不超阈值。
# 用法：audit_symbols.sh <dist目录> [max_glibc(2.31)] [max_glibcxx(3.4.28)]
# 超阈值即在对应 Ubuntu 老版本上出现 "version GLIBC_x.xx not found" 启动失败。
set -u
DIR="${1:?用法: audit_symbols.sh <dist目录> [max_glibc] [max_glibcxx]}"
MAXG="${2:-2.31}"
MAXC="${3:-3.4.28}"

# ver_le A B：A ≤ B（按版本序）
ver_le() { [ "$(printf '%s\n' "$1" "$2" | sort -V | head -1)" = "$1" ]; }

fail=0
count=0
while IFS= read -r -d '' f; do
    file "$f" | grep -q ELF || continue
    count=$((count+1))
    g=$(objdump -T "$f" 2>/dev/null | grep -oE 'GLIBC_[0-9.]+'  | sort -Vu | tail -1)
    c=$(objdump -T "$f" 2>/dev/null | grep -oE 'GLIBCXX_[0-9.]+' | sort -Vu | tail -1)
    [ -n "$g" ] && ! ver_le "${g#GLIBC_}" "$MAXG"  && { echo "FAIL $f: 需要 ${g} > GLIBC_$MAXG";  fail=1; }
    [ -n "$c" ] && ! ver_le "${c#GLIBCXX_}" "$MAXC" && { echo "FAIL $f: 需要 ${c} > GLIBCXX_$MAXC"; fail=1; }
done < <(find "$DIR" -type f -print0)

if [ $fail -eq 0 ]; then
    echo "OK: $count 个 ELF 全部满足 GLIBC≤$MAXG / GLIBCXX≤$MAXC"
fi
exit $fail
