# HMCB Qt —— qmake 工程文件（Qt Creator 打开本文件即可编辑/编译/调试）
#
# 说明：
# - 主构建系统为 CMake（CMakeLists.txt，原始需求）。本 .pro 为镜像，二者等价。
# - Qt6 下 Qt Creator 直接打开 CMakeLists.txt 体验最佳（调试/运行/套件选择）；
#   若偏好 qmake，打开本 .pro 即可。
# - 源文件用递归通配自动收集，新增 core/ui 文件无需手动登记。

QT += widgets network
CONFIG += c++17 console
CONFIG -= app_bundle
DEFINES += QT UNIX

# ---- 预编译控制库 libmcbctrl ----
LIBDIR = $$PWD/../libMcbctrl_HMCB_20260421
INCLUDEPATH += $$PWD/src $$LIBDIR/src $$LIBDIR/include
LIBS += $$LIBDIR/libmcbctrl.so.1.0.0.131
QMAKE_RPATHDIR += $$LIBDIR   # 运行期定位 .so（与 CMake 的 RPATH 等价）

# ---- 自动收集源码（递归 src/）----
SOURCES += $$files($$PWD/src/*.cpp, true)
HEADERS += $$files($$PWD/src/*.h, true)

TARGET = HMCB_Qt
TEMPLATE = app

# ---- 构建后把 config 拷到可执行目录，便于运行读取 app.ini ----
QMAKE_POST_LINK += $$QMAKE_COPY_DIR $$shell_path($$PWD/config) $$shell_path($$OUT_PWD/config) $$escape_expand(\\n\\t)
