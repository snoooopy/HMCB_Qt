#pragma once
#include <QMainWindow>
#include "core/FndmParaBuilder.h"
#include <QList>
#include <QElapsedTimer>
#include "core/McbTypes.h"

class McbController;
class DataGridWidget;
class StatusSidebar;
class MessageLog;
class GraphView;
class OneModuleView;
class QLabel;
class QAction;

// 主窗口：菜单/工具栏/状态栏 + 中央 splitter（左数据网格，右板状态）+ 底部消息日志。
// 菜单与工具栏严格对照 Windows MCB_TEST.rc 的 IDR_MAINFRAME（文字/顺序/分隔），
// 未实现的 Phase 4 功能置灰占位。持有 McbController 指针（不拥有）。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(McbController* ctrl, QWidget* parent = nullptr);

private slots:
    void onReconnect();
    void onStart();
    void onStop();
    void onExit();
    void onAbout();

    void onStatusMessage(const QString& text, int nMCB);
    void onFrame(int nMCB, const Mcb::McbFrame& frame);
    void onDeviceReady();
    void onRunStateChanged(bool running);

    void onMeasureVzero();
    void onMeasureVnoise();      // function=1
    void onMeasureVnoiseD();     // function=3
    void onMeasureInl();         // function=2
    void onMeasureXdif();        // function=4（须先 Zero）
    void onMeasureGain();        // function=6
    void onSaveResult();
    void onMeasurementReady(int function);
    void onGainCalibReady();
    void onToolsSetNewIp();     // Tools→Set New IP（IDD_DIALOG_SETNEWIP）
    void onDumpEeprom();        // Tools→Dump EEPROM Key Data
    void onDumpEcsRegisters();  // Tools→Dump All ECS Registers
    void onSetPotmt();          // Measure→SetPOTMT（发 0）
    void onSetIntt();           // Measure→Set INTT（发 0）
    void onSetDmRange();        // Measure→Set DM Range（重读 ini 下发）
    void onSetDm2Range();       // Measure→Set DM 2 Types Range
    void onSavePosData();       // File→Save Position Code Data
    void onSaveOriginData();    // File→Save Origin Data
    void onSaveImageData();    // File→Save Image Data（ORI/IMG/CSV）
    void onFndmDeMode();        // Measure→FNDM DualEnergy Mode（toggle）
    void onFndmNsPulsed();      // Measure→Fast Neutron Source Type
    void onFndmNsContinued();
    void onFndmParaSet();       // Measure→Set FNDM Parameter && Scan（对话框）

private:
    void buildActions();   // 共享 QAction（菜单与工具栏挂同一动作，同 MFC 同 ID）
    void buildMenus();
    void buildToolBar();
    void buildStatusBar();
    void wireController();

    QAction* m_aSaveImg = nullptr;   // Save Image Data（菜单+工具栏同动作）
    bool m_bSaveImg = true;          // MCB_TEST.ini [FILE] SAVEIMGFILE（缺省 1）

    // Windows Enable(!m_bStart && m_bDVReady) 的动态门控动作（采集期间禁用）；
    // m_stopGateActions 反相（仅采集运行中启用，对照 Stop: m_bStart&&DVREADY）
    void setReadyGated(bool enabled);
    QList<QAction*> m_readyGateActions;
    QList<QAction*> m_stopGateActions;
    bool m_ready = false;

    McbController* m_ctrl;
    DataGridWidget* m_grid = nullptr;
    StatusSidebar* m_sidebar = nullptr;
    MessageLog*  m_log = nullptr;
    GraphView*   m_graph = nullptr;
    OneModuleView* m_one = nullptr;

    // 共享动作（对照 .rc 命令 ID；Trigger/DataFormat 为互斥组）
    QAction* m_aSaveResult = nullptr;
    QAction* m_aTrigInner  = nullptr;
    QAction* m_aTrigOuter  = nullptr;
    QAction* m_aTrigCascade = nullptr;
    QAction* m_aHex = nullptr;
    QAction* m_aDec = nullptr;
    QAction* m_aMv  = nullptr;
    QAction* m_aGraph = nullptr;
    QAction* m_aOneModule = nullptr;
    QAction* m_aTestMode = nullptr;
    QAction* m_aFndmNsPulsed = nullptr;      // Fast Neutron Source Type（check 互斥）
    QAction* m_aFndmNsContinued = nullptr;
    int m_fndmNs = 0;                        // Windows m_nNs：0-Pulsed 1-Continued
    FndmParaBuilder::LastPara m_fndmLast;    // Windows m_FndLastPara：跨打开记忆
    QAction* m_aSendAll  = nullptr;
    QAction* m_aPrevPage = nullptr;
    QAction* m_aNextPage = nullptr;

    // 状态栏 6 格
    QLabel* m_stRun    = nullptr;   // 运行时间
    QLabel* m_stRate   = nullptr;   // 刷新速率
    QLabel* m_stSpeed  = nullptr;   // 数据速度 MB/s（对照 Windows Pane3 Rate）
    QLabel* m_stData   = nullptr;   // 累计帧数
    QLabel* m_stDI     = nullptr;   // DI 寄存器
    QLabel* m_stStatus = nullptr;   // 设备状态

    // 统计
    qlonglong m_frameCount = 0;
    int m_runSeconds = 0;
    int m_framesThisSec = 0;
    // 帧到达时只缓存值，状态栏/侧栏文本统一由 1Hz rateTimer 刷新
    // （对照 MFC OnTimer 1s 更新状态栏；真机包率下每帧 setText 会占满弱核 GUI）
    qlonglong m_lastSC[8] = {0}, m_lastPC[8] = {0};   // 按板缓存（多盒时 1Hz tick 分板刷新）
    uint32_t m_lastDI[8] = {0};
    class QTimer* m_rateTimer = nullptr;

    // 数据速度（对照 MCB_TESTView.cpp:1846-1882 Pane3 "Rate:%8.3fMb/s avg:%8.3fMb/s"：
    // 每帧位数=Σ板(nDMNum×行×列×adBit)，瞬时=样本差×位数/tick间隔，平均=本轮样本×位数/运行时长；
    // Windows 单位 Mb/s，本工程按需求以 MB/s 显示（÷8））
    qint64 m_speedBitsPerFrame = 0;   // 每帧数据位数（DVREADY 后由几何+AD 位数求得）
    qlonglong m_speedCnt0 = 0;        // 上一 tick 的 scounter（Windows m_uCnt0）
    qlonglong m_speedBaseSC = 0;      // 本轮首帧 scounter（实机每轮从 1 起；0=本轮尚未见帧）
    qint64 m_speedLastMs = 0;         // 上一 tick 的累计毫秒（Windows m_t0→m_t）
    QElapsedTimer m_speedClock;       // 本轮计时（Windows m_tstart）
};
