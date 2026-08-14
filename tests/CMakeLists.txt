# ---- test_liblink：验证库头可编译 + SetMCBCommand 可链接 ----
qt_add_executable(test_liblink test_liblink.cpp)
target_link_libraries(test_liblink PRIVATE Qt6::Test mcbctrl)
target_include_directories(test_liblink PRIVATE ${MCBCTRL_INCLUDE_DIRS})
add_test(NAME liblink COMMAND test_liblink)

# ---- test_mcbtypes：状态解码 + 帧值对象 ----
qt_add_executable(test_mcbtypes test_mcbtypes.cpp
    ${CMAKE_SOURCE_DIR}/src/core/McbTypes.cpp)
target_link_libraries(test_mcbtypes PRIVATE Qt6::Test)
target_include_directories(test_mcbtypes PRIVATE ${CMAKE_SOURCE_DIR}/src ${MCBCTRL_INCLUDE_DIRS})
add_test(NAME mcbtypes COMMAND test_mcbtypes)

# ---- test_inifile：INI 读写（兼容库 CIniFile）----
qt_add_executable(test_inifile test_inifile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/IniFile.cpp)
target_link_libraries(test_inifile PRIVATE Qt6::Test)
target_include_directories(test_inifile PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME inifile COMMAND test_inifile)

# ---- test_mockbackend：仿真后端 状态序列 + 数据帧 ----
qt_add_executable(test_mockbackend test_mockbackend.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MockMcbBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/core/McbTypes.cpp)
target_link_libraries(test_mockbackend PRIVATE Qt6::Test)
target_include_directories(test_mockbackend PRIVATE ${CMAKE_SOURCE_DIR}/src ${MCBCTRL_INCLUDE_DIRS})
add_test(NAME mockbackend COMMAND test_mockbackend)
