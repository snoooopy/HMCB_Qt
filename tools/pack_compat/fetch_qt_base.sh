#!/bin/bash
# 宿主机下载 Qt 6.4.3 qtbase 官方归档并校验（容器内 aqt 下载会截断损坏，故宿主下载后 COPY 进镜像）
# 幂等：已存在且 hash 正确则直接通过
set -euo pipefail
cd "$(dirname "$0")"

ARCHIVE=6.4.3-0-202303121118qtbase-Linux-RHEL_8_4-GCC-Linux-RHEL_8_4-X86_64.7z
SHA=e55568234e2b39951300fae72683cc8dd657130c7256bc4fab88b75d386e32be

if [ -f qtbase.7z ] && [ "$(sha256sum qtbase.7z | cut -d' ' -f1)" = "$SHA" ]; then
    echo "qtbase.7z 已存在且校验通过"
else
    for base in https://mirrors.tuna.tsinghua.edu.cn/qt https://mirrors.ustc.edu.cn/qtproject; do
        echo "下载：$base/.../$ARCHIVE"
        if curl -fSL --retry 3 -o qtbase.7z \
            "$base/online/qtsdkrepository/linux_x64/desktop/qt6_643/qt.qt6.643.gcc_64/$ARCHIVE" \
           && [ "$(sha256sum qtbase.7z | cut -d' ' -f1)" = "$SHA" ]; then
            echo "下载并校验通过（$base）"
            break
        fi
        rm -f qtbase.7z
    done
    [ -f qtbase.7z ] || { echo "错误：qtbase 两个镜像均下载失败或校验不过" >&2; exit 1; }
fi

# icu 归档（Qt 链接 icu 56，独立于 qtbase 分发；Updates.xml 无 SHA，但归档旁有官方 .sha1，
# 用「tuna 下载归档 + 双镜像各取 .sha1 比对一致 + sha1sum 校验」三重验证）
ICU=6.4.3-0-202303121118icu-linux-Rhel7.2-x64.7z
BASE_T=https://mirrors.tuna.tsinghua.edu.cn/qt/online/qtsdkrepository/linux_x64/desktop/qt6_643/qt.qt6.643.gcc_64
BASE_U=https://mirrors.ustc.edu.cn/qtproject/online/qtsdkrepository/linux_x64/desktop/qt6_643/qt.qt6.643.gcc_64
if [ -f icu.7z ]; then
    echo "icu.7z 已存在（首次下载时已校验），跳过"
else
    curl -fSL --retry 3 -o icu.7z     "$BASE_T/$ICU"      || { echo "错误：icu 归档下载失败（tuna）" >&2; exit 1; }
    curl -fSL --retry 3 -o icu.sha1   "$BASE_T/$ICU.sha1" || { echo "错误：icu sha1 下载失败（tuna）" >&2; exit 1; }
    curl -fSL --retry 3 -o icu2.sha1  "$BASE_U/$ICU.sha1" || { echo "错误：icu sha1 下载失败（ustc）" >&2; exit 1; }
    if ! cmp -s icu.sha1 icu2.sha1; then
        echo "错误：icu 官方 sha1 双镜像不一致" >&2; rm -f icu.7z icu.sha1 icu2.sha1; exit 1
    fi
    want=$(awk '{print $1}' icu.sha1)
    got=$(sha1sum icu.7z | awk '{print $1}')
    if [ "$want" != "$got" ]; then
        echo "错误：icu 归档 sha1 校验失败（期望 $want 实际 $got）" >&2; rm -f icu.7z icu.sha1 icu2.sha1; exit 1
    fi
    rm -f icu2.sha1
    echo "icu.7z 官方 sha1 双镜像比对 + 校验通过（$got）"
fi
