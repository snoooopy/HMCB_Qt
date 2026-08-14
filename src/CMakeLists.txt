qt_standard_project_setup()

# 逐步加入 core/* 与 ui/* 源文件。
set(APP_SOURCES
    main.cpp
    core/McbTypes.h
    core/McbTypes.cpp
    core/IniFile.h
    core/IniFile.cpp
    core/IMcbBackend.h
    core/MockMcbBackend.h
    core/MockMcbBackend.cpp
    core/RealMcbBackend.h
    core/RealMcbBackend.cpp
)

qt_add_executable(HMCB_Qt ${APP_SOURCES})
target_include_directories(HMCB_Qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MCBCTRL_INCLUDE_DIRS})
target_link_libraries(HMCB_Qt PRIVATE Qt6::Widgets Qt6::Network mcbctrl)
set_target_properties(HMCB_Qt PROPERTIES
    WIN32_EXECUTABLE OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
# 运行期在可执行目录提供 config
add_custom_command(TARGET HMCB_Qt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../config" "$<TARGET_FILE_DIR:HMCB_Qt>/config")
