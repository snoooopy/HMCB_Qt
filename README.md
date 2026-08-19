# HMCB_Qt —— HM MCB Test Program（Ubuntu Qt6 移植版）

HMCB 探测器测试软件的 Linux/Qt6 移植。原版为 Windows MFC 程序
`HMCB-H_DLL_TEST V1.4.2.4_20260319_HMCB4`，本工程按其菜单、测量流程与
文件格式逐功能对照移植，控制链路复用同一套控制库 `libmcbctrl` 的 Linux 版
（`libMcbctrl_HMCB_20260421/libmcbctrl.so.1.0.0.131`）。

- **开发周期**： 2026-08-13 ～ 2026-08-19，分四个 Phase 交付（48 次提交）
- **测试**： 12/12 ctest 全绿；CMake（主）+ qmake `.pro`（Qt Creator 镜像）双构建

## 1. 目录结构

```
HMCB_Qt/
├── CMakeLists.txt            # 主构建（顶层）
├── HMCB_Qt.pro               # qmake 镜像（Qt Creator 直接打开）
├── config/                   # 运行期配置（构建时自动拷到可执行目录）
│   ├── app.ini               #   后端模式切换（mock/仿真 real/真实）
│   ├── MCB_TEST.ini          #   测量参数（Windows 同名 ini 的对应节）
│   └── MCBCtrl.ini           #   控制库读的 ini（连接/几何/增益档）
├── src/
│   ├── main.cpp
│   ├── core/                 # 无 UI 依赖的核心层
│   │   ├── McbTypes.*        #   状态解码/McbFrame/FrameEx/IPv4 解析
│   │   ├── IniFile.*         #   INI 读写（剥行内注释）
│   │   ├── IMcbBackend.h     #   后端接口（命令+数据/状态回调）
│   │   ├── MockMcbBackend.*  #   仿真后端（状态机+合成帧，单测/无硬件联调）
│   │   ├── RealMcbBackend.*  #   真实后端（封装 libmcbctrl SetMCBCommand）
│   │   ├── McbController.*   #   QObject 控制器（回调→Qt 信号，测量分派）
│   │   ├── MeasurementEngine.* # 帧环形缓存+Zero/Noise/X-DIF/INL 计算
│   │   ├── CsvResultWriter.* #   测量结果 csv/txt
│   │   ├── PosDataWriter.*   #   Save Position Code Data
│   │   ├── OriginDataWriter.*#   Save Origin Data（.oridat）
│   │   ├── ImageDataWriter.* #   Save Image Data（ORI/IMG/CSV+附加文件）
│   │   ├── FndmParaBuilder.* #   FNDM 参数组包纯函数
│   │   ├── FndmScanWriter.*  #   中子谱 txt/csv 存档
│   │   └── XmlGainWriter.*   #   增益写回 DIASConfig.xml
│   └── ui/                   # Qt Widgets 界面层
│       ├── MainWindow.*      #   主窗（菜单/工具栏/状态栏，对照 MCB_TEST.rc）
│       ├── DataGridWidget.*  #   大数据网格（分页 ADM，Vzero/Vnoise/INL 显示）
│       ├── WaveformWidget.*  #   波形页
│       ├── OneModuleView.*   #   单模块页
│       ├── GraphView.*       #   INL 曲线
│       ├── StatusSidebar.*   #   右栏板卡状态
│       ├── MessageLog.*      #   日志（含 NUCTECH logo）
│       ├── GainCalibResultDialog.*、SetIpDialog.*、FndmParaDialog.*
│       └── icons/ app.qrc    #   工具栏图标（19×17，切自 Toolbar.bmp）
├── tests/                    # 12 个 qtest 目标（见 §7）
└── docs/                     # 开发记录文档
```

## 2. 环境要求

| 组件 | 要求 |
|---|---|
| OS | Ubuntu（x86-64），GCC ≥ 13 |
| Qt | ≥ 6.4（命令行 6.4.2 与 Qt Creator ≥ 6.5 套件均通过；代码带 `QT_VERSION` 兼容分支） |
| 构建 | CMake ≥ 3.16 或 qmake6 任一 |
| 控制库 | `../libMcbctrl_HMCB_20260421/`（相对本工程目录，含头与 .so） |

## 3. 构建与运行

```bash
# CMake（主构建系统）
cmake -B build -S . && cmake --build build -j
./build/HMCB_Qt

# qmake（Qt Creator 打开 HMCB_Qt.pro 等价）
qmake6 HMCB_Qt.pro -o build_qmake/Makefile && make -C build_qmake -j
```

- 每次构建自动把 `config/` 同步到可执行目录（改配置也会生效）。
- 后端模式由 `config/app.ini [BACKEND] Mode` 决定：
  `mock`/`仿真`（默认，无硬件即可全流程联调）、`real`/`真实`（连设备）。
- 无头冒烟：`QT_QPA_PLATFORM=offscreen HMCB_AUTOSTART=1 HMCB_SNAPSHOT=/tmp/snap.png ./build/HMCB_Qt`
  （自动开始采集并在 3.5s 截图，配合 timeout 使用）。

## 4. 架构

四层结构，UI 与硬件彻底解耦：

```
MainWindow / 各视图（GUI 线程）
        │ Qt signals/slots
McbController（QObject：命令分派、测量状态机、结果汇总）
        │ IMcbBackend 接口
MockMcbBackend（仿真）   RealMcbBackend（封装 libmcbctrl）
                                  │ SetMCBCommand / PFCALLBACK（库工作线程）
                               探测器硬件
```

- **线程模型**： 库回调在工作线程触发；McbController 直接 emit 信号，
  带 receiver context 的连接自动 Queued 到 GUI 线程
  （仅需 `qRegisterMetaType<Mcb::McbFrame>`）。
- **测量触发链**： `measure(fn,N)` → 有限采集 → 库回调 `MCB_STOPOK`
  →（`QTimer::singleShot(0)` 延迟到 Controller 线程，防自 join）
  → `handleStopok()` 按分派计算（Zero/Noise/X-DIF/INL/增益）→ `measurementReady(fn)`。
- **数据流**： 数据回调 → `MeasurementEngine` 帧环形缓存（互斥保护，
  深 20000=Windows DataNum）+ 扩展字缓存 → 各视图/保存器按需快照。
- **几何**： 网格/保存器几何来自库 `GETDMINFO`（即 MCBCtrl.ini：
  [MCB] MCB0ADNUM、[ADM] ROW/COLUMN；HMCB4 实配 25 DM）。

## 5. 配置体系（三份 ini 分工）

| 文件 | 读者 | 关键节 |
|---|---|---|
| `app.ini` | Qt 应用 | `[BACKEND] Mode` mock/real |
| `MCB_TEST.ini` | Qt 应用 | `[VZERO]/[VNOISE]/[INL]` 阈值与帧数；`[FILE]` 保存开关（SAVEIMGFILE/IMGFILEPIXEL/GMCORINORI/ALLCOLUMN/SAVE_EXDATA）；`[ADM] ENFNDMODE` |
| `MCBCtrl.ini` | **控制库** | `[CONNECTION] IPADDR/AUTOCONNECT`；`[MCB]/[ADM]/[RANGEn]` 几何与增益；增益校准写回同文件；`[XRAY]/[MODE0]` X-DIF 剂量参数 |

注意：库的 INI 解析器**不支持行内注释**——MCBCtrl.ini 中注释必须独立成行。
库日志写 `<config目录>/HMCBCtrl.log`。

## 6. 功能对照（Windows → Qt）

菜单结构对照 `MCB_TEST.rc` 的 IDR_MAINFRAME（File / MCB Control / Data View /
Measure / Tools / Help），菜单项与工具栏按钮同 QAction 共享。

| Windows 功能 | Qt 状态 | 说明 |
|---|---|---|
| File→Save Test Result | ✅ | csv+txt 双写，Zero/Noise/INL/X-DIF 条件列组，aXptos 系数 |
| File→Save Origin Data | ✅ | `.oridat` 每帧前 128 通道 int32 小端 |
| File→Save Image Data | ✅ | ORI/IMG/CSV 三格式 + `.acce/.Ain0/.godown.csv` 附加 |
| File→Save Position Code Data | ✅ | ptsm 逐帧文本 |
| MCB Control→Start/Stop/Trigger | ✅ | 内/外触发、Reconnect |
| Data View→Data/Wave/One Module | ✅ | 网格分页/波形/单模块 |
| Data View→NDet_Plot | ⛔ 禁用 | Windows 源码整段注释=死功能，保持等价 |
| Measure→Zero / Noise / Noise(DET) | ✅ | fn=0/1/3，均值基线 / FWHM |
| Measure→Diversity in X Ray | ✅ | fn=4 X-DIF，须先 Zero，VzeroX 网格显示 |
| Measure→INL | ✅ | fn=2 多点 DA 扫描+最小二乘（a/b/r/INL‰/DIV%） |
| Measure→Gain automatic calibration | ✅ | fn=6 迭代升档，MCBCtrl.ini+DIASConfig.xml 双写回 |
| Measure→Set DM Range ×2 | ✅ | MCBCtrl.ini [RANGEn] 下发 |
| Measure→SetPOTMT / Set INTT | ✅ | 发 0 |
| Measure→FNDM 参数+中子谱扫描 | ✅ | 参数组包+两阶段谱扫描（背景→谱）+存档 |
| Measure→FNDM DualEnergy/源类型 | ✅ | SETDEMODE/SETNSTYPE |
| Tools→Set New IP | ✅ | 12 字节（IP/mask/gw）0x0601 |
| Tools→Dump EEPROM / ECS Registers | ✅ | hex 落盘 |

## 7. 测试（12 目标，全绿）

```
liblink mcbtypes inifile mockbackend controller measurement
imagedata fndmpara csvwriter xmlwriter datagrid setipdialog
```

覆盖：库链接、状态解码、INI 读写、仿真后端、跨线程信号、测量引擎纯逻辑、
Save Image 三格式字节级断言、FNDM 组包、CSV/txt 格式、XML 写回、
网格几何（offscreen）、Set IP 对话框（offscreen）。运行：`cd build && ctest`。

## 8. 数据文件格式速查

| 格式 | 内容 |
|---|---|
| `result.csv/.txt` | 头两行通道名+量纲；列组随测量结果条件出现 |
| `.oridat` | 每帧 128 通道 int32 小端裸数据 |
| `.ori` | PtT_file_head(128B)+saveinfo(128B 零)+小端 long；每帧列前 16B datainfo；非全列拆 `-N.ori` |
| `.img` | 36B 大端头+大端像素（16bit 右对齐 / 32bit 左对齐）+exdata 4 行+GM 16 行 |
| `.csv`（图像） | `SN,M{j}CH{l}-{k:02d},...,EXCH0..4` |
| `.acce/.Ain0/.godown.csv` | 逐帧 accE/Ain0；godown 变化点 |
| `neutron_spectra_integrated.txt/.csv` | 中子谱（bin×通道×两阶段） |

## 9. 已知差异与限制（有意为之）

- **NDet_Plot 禁用**： Windows 对应处理函数整段被注释，移植保持禁用。
- **Linux 库 FNDM 命令静默**： 库源 SETFNDMPARA/GETFNDMPARA/SETDEMODE/SETNSTYPE
  的 case 被注释（收到不回包）；Qt 侧照 Windows 契约正常组包转发，
  对话框回读因库不回数据而保持界面值。
- **数据网格空行不复刻**： MFC 无网格线空行只是行距效果；Qt 网格线会显成缺数据空格。
- **性能取舍**： 网格列宽一次性自适应+250ms 合并刷新（常驻 ResizeToContents 会饱和 GUI）。

## 10. 开发记录

完整开发过程（需求讨论、Windows 契约调研、设计取舍、调试过程）见
`docs/chat-history-2026-08-13_to_2026-08-19.md`（本项目 7 天开发全程对话导出）。
