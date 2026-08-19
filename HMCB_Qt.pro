# HMCB Qt —— qmake 工程文件（Qt Creator 打开本文件即可编辑/编译/调试）
#
# 说明：
# - 主构建系统为 CMake（CMakeLists.txt，原始需求）。本 .pro 为镜像，二者等价。
# - Qt6 下 Qt Creator 直接打开 CMakeLists.txt 体验最佳（调试/运行/套件选择）；
#   若偏好 qmake，打开本 .pro 即可。
# - 源文件用递归通配自动收集，新增 core/ui 文件无需手动登记。

QT += widgets network xml
CONFIG += c++17 console
CONFIG -= app_bundle
DEFINES += QT UNIX

# ---- 预编译控制库 libmcbctrl ----
LIBDIR = $$PWD/../libMcbctrl_HMCB_20260421
INCLUDEPATH += $$PWD/src $$LIBDIR/src $$LIBDIR/include
LIBS += $$LIBDIR/libmcbctrl.so.1.0.0.131
QMAKE_RPATHDIR += $$LIBDIR   # 运行期定位 .so（与 CMake 的 RPATH 等价）

# ---- 自动收集源码（core/ui 两层 + main.cpp；不递归以避开构建器生成物，
#      CMake AUTOMOC 曾在 src/ 下生成 *_autogen/ 导致 moc 符号重复定义）----
SOURCES += $$PWD/src/main.cpp $$files($$PWD/src/core/*.cpp) $$files($$PWD/src/ui/*.cpp)
HEADERS += $$files($$PWD/src/core/*.h) $$files($$PWD/src/ui/*.h)
RESOURCES += $$PWD/src/ui/app.qrc

TARGET = HMCB_Qt
TEMPLATE = app

# ---- 每次构建把 config 拷到可执行目录，便于运行读取 app.ini ----
# （不用 QMAKE_POST_LINK：仅改配置时目标不重链、配置不同步）
config_sync.target = sync_config
config_sync.commands = $$QMAKE_COPY_DIR $$shell_path($$PWD/config) $$shell_path($$OUT_PWD/config)
QMAKE_EXTRA_TARGETS += config_sync
POST_TARGETDEPS += sync_config
