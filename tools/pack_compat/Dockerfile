# HMCB_Qt 多版本 Ubuntu 兼容构建镜像（最低目标 = Ubuntu 20.04）
# 原理：全部产物（程序 / libmcbctrl / Qt 及第三方依赖）在 20.04 (glibc 2.31) 环境构建，
#       符号需求自然 ≤ 2.31 / GLIBCXX ≤ 3.4.28，向上兼容 22.04 / 24.04+。
# Qt 来源：官方 6.4.3 qtbase 归档（RHEL8 构建，glibc 目标 2.28）。
#   注意：容器内 aqtinstall 下载会截断损坏（宿主下载正常），故先在宿主跑
#   fetch_qt_base.sh 下载并校验 qtbase.7z，再 COPY 进本镜像解压。
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# apt 换清华源并安装工具链 + 依赖收集源（运行库包：make_package.sh 递归 ldd 从本机收集）
RUN sed -i -e 's|archive.ubuntu.com|mirrors.tuna.tsinghua.edu.cn|g' \
           -e 's|security.ubuntu.com|mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list \
 && apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build git ca-certificates p7zip-full \
      libboost-dev file binutils patchelf \
      libxkbcommon0 libxkbcommon-x11-0 \
      libxcb1 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
      libxcb-render0 libxcb-render-util0 libxcb-shm0 libxcb-sync1 libxcb-xfixes0 \
      libxcb-xkb1 libxcb-shape0 \
      libx11-6 libxau6 libxdmcp6 libgl1 libegl1 libopengl0 libglx0 \
      libgl-dev libopengl-dev libglx-dev \
      libglib2.0-0 libdbus-1-3 libfontconfig1 libfreetype6 libharfbuzz0b libgraphite2-3 \
      libpcre2-16-0 libpcre2-8-0 libicu66 libdouble-conversion3 libb2-1 \
      libbrotli1 libbz2-1.0 liblzma5 libzstd1 liblz4-1 libpng16-16 libexpat1 \
      libgcrypt20 libgpg-error0 libsystemd0 libcap2 libmd0 libbsd0 \
 && rm -rf /var/lib/apt/lists/*

# Qt 6.4.3 官方 qtbase + icu56（qtbase 不含 icu，Qt 二进制 NEEDED libicui18n.so.56 等）
COPY qtbase.7z icu.7z /tmp/
RUN 7z x /tmp/qtbase.7z -o/opt/Qt > /dev/null \
 && 7z x /tmp/icu.7z -o/opt/Qt > /dev/null \
 && rm /tmp/qtbase.7z /tmp/icu.7z \
 && chmod -R a+rX /opt/Qt \
 && ln -s /opt/Qt/6.4.3/gcc_64/bin/qmake /usr/local/bin/qmake6

ENV PATH=/opt/Qt/6.4.3/gcc_64/bin:$PATH
