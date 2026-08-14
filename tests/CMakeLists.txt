# ---- test_liblink：验证库头可编译 + SetMCBCommand 可链接 ----
qt_add_executable(test_liblink test_liblink.cpp)
target_link_libraries(test_liblink PRIVATE Qt6::Test mcbctrl)
target_include_directories(test_liblink PRIVATE ${MCBCTRL_INCLUDE_DIRS})
add_test(NAME liblink COMMAND test_liblink)
