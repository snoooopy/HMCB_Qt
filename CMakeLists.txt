cmake_minimum_required(VERSION 3.16)
project(HMCB_Qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
add_compile_definitions(QT UNIX)   # 匹配库的 Linux 编译分支与 MCBCtrl.h 的 typedef

option(HMCB_BUILD_TESTS "Build QtTest unit tests" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network Xml)
if(HMCB_BUILD_TESTS)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
endif()

# ---- 链接预编译控制库（无 SONAME，用 IMPORTED 全路径 + RPATH）----
set(MCBCTRL_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libMcbctrl_HMCB_20260421")
set(MCBCTRL_INCLUDE_DIRS
    "${MCBCTRL_LIB_DIR}/src"
    "${MCBCTRL_LIB_DIR}/include")
set(MCBCTRL_LIB_FILE "${MCBCTRL_LIB_DIR}/libmcbctrl.so.1.0.0.131")

add_library(mcbctrl SHARED IMPORTED)
set_target_properties(mcbctrl PROPERTIES
    IMPORTED_LOCATION "${MCBCTRL_LIB_FILE}")
# 构建/运行期定位 .so
set(CMAKE_BUILD_RPATH "${MCBCTRL_LIB_DIR}")
set(CMAKE_INSTALL_RPATH "${MCBCTRL_LIB_DIR};$ORIGIN/../lib")

add_subdirectory(src)

if(HMCB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
