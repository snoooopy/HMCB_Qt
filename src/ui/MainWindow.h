#pragma once
#include <QMainWindow>
#include <QList>
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

private:
    void buildActions();   // 共享 QAction（菜单与工具栏挂同一动作，同 MFC 同 ID）
    void buildMenus();
    void buildToolBar();
    void buildStatusBar();
    void wireController();

    // Windows Enable(!m_bStart && m_bDVReady) 的动态门控动作
    void setReadyGated(bool enabled);
    QList<QAction*> m_readyGateActions;
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
    QAction* m_aSendAll  = nullptr;
    QAction* m_aPrevPage = nullptr;
    QAction* m_aNextPage = nullptr;

    // 状态栏 5 格
    QLabel* m_stRun    = nullptr;   // 运行时间
    QLabel* m_stRate   = nullptr;   // 刷新速率
    QLabel* m_stData   = nullptr;   // 累计帧数
    QLabel* m_stDI     = nullptr;   // DI 寄存器
    QLabel* m_stStatus = nullptr;   // 设备状态

    // 统计
    qlonglong m_frameCount = 0;
    int m_runSeconds = 0;
    int m_framesThisSec = 0;
    class QTimer* m_rateTimer = nullptr;
};
