# ---- test_liblink：验证库头可编译 + SetMCBCommand 可链接 + 帧通道数拷贝 ----
qt_add_executable(test_liblink test_liblink.cpp
    ${CMAKE_SOURCE_DIR}/src/core/RealMcbBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/core/McbTypes.cpp)
target_link_libraries(test_liblink PRIVATE Qt6::Test mcbctrl)
target_include_directories(test_liblink PRIVATE ${CMAKE_SOURCE_DIR}/src ${MCBCTRL_INCLUDE_DIRS})
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

# ---- test_controller：McbController 跨线程回调→Qt 信号 ----
qt_add_executable(test_controller test_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/core/McbController.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MockMcbBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/core/RealMcbBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/core/McbTypes.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MeasurementEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/IniFile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FpgaBinChecker.cpp
    ${CMAKE_SOURCE_DIR}/src/core/XmlGainWriter.cpp)
target_link_libraries(test_controller PRIVATE Qt6::Test Qt6::Xml mcbctrl)
target_include_directories(test_controller PRIVATE ${CMAKE_SOURCE_DIR}/src ${MCBCTRL_INCLUDE_DIRS})
add_test(NAME controller COMMAND test_controller)

# ---- test_measurement：MeasurementEngine Zero/Noise 纯逻辑 ----
qt_add_executable(test_measurement test_measurement.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MeasurementEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PosDataWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/OriginDataWriter.cpp)
target_link_libraries(test_measurement PRIVATE Qt6::Test)
target_include_directories(test_measurement PRIVATE ${CMAKE_SOURCE_DIR}/src ${MCBCTRL_INCLUDE_DIRS})
add_test(NAME measurement COMMAND test_measurement)

# ---- test_imagedata：Save Image Data 三格式写盘（ORI/IMG/CSV+附加文件）----
qt_add_executable(test_imagedata test_imagedata.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ImageDataWriter.cpp)
target_link_libraries(test_imagedata PRIVATE Qt6::Test)
target_include_directories(test_imagedata PRIVATE ${CMAKE_SOURCE_DIR}/src ${MCBCTRL_INCLUDE_DIRS})
add_test(NAME imagedata COMMAND test_imagedata)

# ---- test_fndmpara：FNDM 参数组包/提取纯逻辑 ----
qt_add_executable(test_fndmpara test_fndmpara.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FndmParaBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FndmScanWriter.cpp)
target_link_libraries(test_fndmpara PRIVATE Qt6::Test)
target_include_directories(test_fndmpara PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME fndmpara COMMAND test_fndmpara)

# ---- test_csvwriter：测量结果 csv/txt 写出 ----
qt_add_executable(test_csvwriter test_csvwriter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CsvResultWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MeasurementEngine.cpp)
target_link_libraries(test_csvwriter PRIVATE Qt6::Test)
target_include_directories(test_csvwriter PRIVATE ${CMAKE_SOURCE_DIR}/src ${MCBCTRL_INCLUDE_DIRS})
add_test(NAME csvwriter COMMAND test_csvwriter)

# ---- test_xmlwriter：增益写回 DIASConfig.xml ----
qt_add_executable(test_xmlwriter test_xmlwriter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/XmlGainWriter.cpp)
target_link_libraries(test_xmlwriter PRIVATE Qt6::Test Qt6::Xml)
target_include_directories(test_xmlwriter PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME xmlwriter COMMAND test_xmlwriter)

# ---- test_datagrid：数据网格生命周期/几何/表头/列宽（offscreen QWidget）----
qt_add_executable(test_datagrid test_datagrid.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/DataGridWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MeasurementEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/McbTypes.cpp)
target_link_libraries(test_datagrid PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_datagrid PRIVATE ${CMAKE_SOURCE_DIR}/src ${MCBCTRL_INCLUDE_DIRS})
add_test(NAME datagrid COMMAND test_datagrid)
set_tests_properties(datagrid PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# ---- test_fpgabin：INITIAL 前 DB FPGA bin 预检（继承链/缺失检测）----
qt_add_executable(test_fpgabin test_fpgabin.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FpgaBinChecker.cpp
    ${CMAKE_SOURCE_DIR}/src/core/IniFile.cpp)
target_link_libraries(test_fpgabin PRIVATE Qt6::Test)
target_include_directories(test_fpgabin PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME fpgabin COMMAND test_fpgabin)

# ---- test_setipdialog：Set New IP 对话框文案/回读/IPv4 验证器（offscreen QWidget）----
qt_add_executable(test_setipdialog test_setipdialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/SetIpDialog.cpp)
target_link_libraries(test_setipdialog PRIVATE Qt6::Test Qt6::Widgets)
target_include_directories(test_setipdialog PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME setipdialog COMMAND test_setipdialog)
set_tests_properties(setipdialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
