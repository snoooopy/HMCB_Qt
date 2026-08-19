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
    core/McbController.h
    core/McbController.cpp
    core/MeasurementEngine.h
    core/MeasurementEngine.cpp
    core/CsvResultWriter.h
    core/CsvResultWriter.cpp
    core/PosDataWriter.h
    core/PosDataWriter.cpp
    core/FndmParaBuilder.h
    core/FndmParaBuilder.cpp
    core/OriginDataWriter.h
    core/OriginDataWriter.cpp
    core/XmlGainWriter.h
    core/XmlGainWriter.cpp
    ui/MainWindow.h
    ui/MainWindow.cpp
    ui/StatusSidebar.h
    ui/StatusSidebar.cpp
    ui/MessageLog.h
    ui/MessageLog.cpp
    ui/DataGridWidget.h
    ui/DataGridWidget.cpp
    ui/WaveformWidget.h
    ui/WaveformWidget.cpp
    ui/GraphView.h
    ui/GraphView.cpp
    ui/OneModuleView.h
    ui/OneModuleView.cpp
    ui/GainCalibResultDialog.h
    ui/GainCalibResultDialog.cpp
    ui/SetIpDialog.h
    ui/SetIpDialog.cpp
)

qt_add_executable(HMCB_Qt ${APP_SOURCES} ui/app.qrc)
target_include_directories(HMCB_Qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MCBCTRL_INCLUDE_DIRS})
target_link_libraries(HMCB_Qt PRIVATE Qt6::Widgets Qt6::Network Qt6::Xml mcbctrl)
set_target_properties(HMCB_Qt PROPERTIES
    WIN32_EXECUTABLE OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
# 运行期在可执行目录提供 config。
# 注意不能挂 POST_BUILD：仅改配置文件（无代码变化）时目标不重链、配置不
# 同步；custom_target 挂 ALL 每次构建都拷（copy_directory 开销可忽略）。
add_custom_target(sync_config ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../config" "$<TARGET_FILE_DIR:HMCB_Qt>/config")
add_dependencies(HMCB_Qt sync_config)
