#pragma once
#include <QMainWindow>
#include "core/McbTypes.h"

class McbController;
class DataGridWidget;
class StatusSidebar;
class MessageLog;
class GraphView;
class OneModuleView;
class QLabel;

// 主窗口：菜单/工具栏/状态栏 + 中央 splitter（左数据网格，右板状态）+ 底部消息日志。
// 持有 McbController 指针（不拥有），把 controller 信号接到各 UI 组件。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(McbController* ctrl, QWidget* parent = nullptr);

private slots:
    void onConnect();
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
    void onToggleGraph(bool on);
    void onToggleOneModule(bool on);
    void onSaveResult();
    void onMeasurementReady(int function);

private:
    void buildMenus();
    void buildToolBar();
    void buildStatusBar();
    void wireController();

    McbController* m_ctrl;
    DataGridWidget* m_grid = nullptr;
    StatusSidebar* m_sidebar = nullptr;
    MessageLog*  m_log = nullptr;
    GraphView*   m_graph = nullptr;
    OneModuleView* m_one = nullptr;

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
