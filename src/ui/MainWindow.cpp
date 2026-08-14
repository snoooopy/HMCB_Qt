#include "MainWindow.h"
#include "ui/DataGridWidget.h"
#include "ui/StatusSidebar.h"
#include "ui/MessageLog.h"
#include "core/McbController.h"
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QAction>
#include <QStatusBar>
#include <QLabel>
#include <QSplitter>
#include <QTimer>
#include <QMessageBox>
#include <QApplication>

MainWindow::MainWindow(McbController* ctrl, QWidget* parent)
    : QMainWindow(parent), m_ctrl(ctrl) {
    setWindowTitle(QStringLiteral("HMCB 探测器测试软件 (Qt)"));
    resize(1280, 800);

    // ---- 中央区：上=[网格 | 侧栏]，下=消息日志 ----
    auto* top = new QSplitter(Qt::Horizontal);
    m_grid = new DataGridWidget;
    m_sidebar = new StatusSidebar;
    top->addWidget(m_grid);
    top->addWidget(m_sidebar);
    top->setStretchFactor(0, 4);
    top->setStretchFactor(1, 1);

    m_log = new MessageLog;

    auto* main = new QSplitter(Qt::Vertical);
    main->addWidget(top);
    main->addWidget(m_log);
    main->setStretchFactor(0, 3);
    main->setStretchFactor(1, 1);
    setCentralWidget(main);

    buildMenus();
    buildToolBar();
    buildStatusBar();

    m_rateTimer = new QTimer(this);
    m_rateTimer->setInterval(1000);
    connect(m_rateTimer, &QTimer::timeout, this, [this]() {
        ++m_runSeconds;
        m_stRun->setText(QStringLiteral("RUN %1s").arg(m_runSeconds));
        m_stRate->setText(QStringLiteral("%1 fps").arg(m_framesThisSec));
        m_framesThisSec = 0;
    });

    if (m_ctrl) wireController();
}

void MainWindow::buildMenus() {
    // 文件
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    auto* aExit = fileMenu->addAction(QStringLiteral("退出(&X)"));
    connect(aExit, &QAction::triggered, this, &MainWindow::onExit);

    // MCB 控制
    auto* ctrlMenu = menuBar()->addMenu(QStringLiteral("MCB控制(&M)"));
    auto* aConn = ctrlMenu->addAction(QStringLiteral("连接/初始化(&C)"));
    auto* aReconn = ctrlMenu->addAction(QStringLiteral("重连(&R)"));
    ctrlMenu->addSeparator();
    auto* aStart = ctrlMenu->addAction(QStringLiteral("开始采集(&S)"));
    auto* aStop  = ctrlMenu->addAction(QStringLiteral("停止采集(&T)"));
    connect(aConn,   &QAction::triggered, this, &MainWindow::onConnect);
    connect(aReconn, &QAction::triggered, this, &MainWindow::onReconnect);
    connect(aStart,  &QAction::triggered, this, &MainWindow::onStart);
    connect(aStop,   &QAction::triggered, this, &MainWindow::onStop);

    // 数据视图 / 测量 / 工具 —— Phase 1 仅占位（后续阶段启用）
    menuBar()->addMenu(QStringLiteral("数据视图(&V)"))->setEnabled(false);
    menuBar()->addMenu(QStringLiteral("测量(&E)"))->setEnabled(false);
    menuBar()->addMenu(QStringLiteral("工具(&T)"))->setEnabled(false);

    // 帮助
    auto* helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    auto* aAbout = helpMenu->addAction(QStringLiteral("关于(&A)"));
    connect(aAbout, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::buildToolBar() {
    auto* tb = addToolBar(QStringLiteral("main"));
    tb->setMovable(false);
    auto* aConn  = tb->addAction(QStringLiteral("连接"));
    auto* aStart = tb->addAction(QStringLiteral("开始"));
    auto* aStop  = tb->addAction(QStringLiteral("停止"));
    auto* aExit  = tb->addAction(QStringLiteral("退出"));
    connect(aConn,  &QAction::triggered, this, &MainWindow::onConnect);
    connect(aStart, &QAction::triggered, this, &MainWindow::onStart);
    connect(aStop,  &QAction::triggered, this, &MainWindow::onStop);
    connect(aExit,  &QAction::triggered, this, &MainWindow::onExit);
}

void MainWindow::buildStatusBar() {
    m_stRun    = new QLabel(QStringLiteral("IDLE"));
    m_stRate   = new QLabel(QStringLiteral("0 fps"));
    m_stData   = new QLabel(QStringLiteral("0 帧"));
    m_stDI     = new QLabel(QStringLiteral("DI: -"));
    m_stStatus = new QLabel(QStringLiteral("未就绪"));
    for (auto* l : {m_stRun, m_stRate, m_stData, m_stDI, m_stStatus}) {
        l->setMinimumWidth(90);
        statusBar()->addWidget(l);
    }
}

void MainWindow::wireController() {
    connect(m_ctrl, &McbController::statusMessage,  this, &MainWindow::onStatusMessage);
    connect(m_ctrl, &McbController::frameReady,     this, &MainWindow::onFrame);
    connect(m_ctrl, &McbController::deviceReady,    this, &MainWindow::onDeviceReady);
    connect(m_ctrl, &McbController::runStateChanged, this, &MainWindow::onRunStateChanged);
}

// ---- 槽 ----
void MainWindow::onConnect()   { if (m_ctrl) m_ctrl->initializeBackend(); }
void MainWindow::onReconnect() {
    if (!m_ctrl) return;
    m_ctrl->stop();
    m_ctrl->shutdown();
    m_ctrl->startUp();
    m_ctrl->initializeBackend();
    m_sidebar->clearAll();
    m_stStatus->setText(QStringLiteral("重连中…"));
}

void MainWindow::onStart()     { if (m_ctrl) m_ctrl->start(0); }
void MainWindow::onStop()      { if (m_ctrl) m_ctrl->stop(); }
void MainWindow::onExit() {
    if (m_ctrl) { m_ctrl->stop(); m_ctrl->shutdown(); }
    close();
}
void MainWindow::onAbout() {
    QMessageBox::about(this, QStringLiteral("关于"),
        QStringLiteral("HMCB 探测器测试软件 Qt 版\n基于 libmcbctrl\nPhase 1"));
}

void MainWindow::onStatusMessage(const QString& text, int nMCB) {
    m_log->appendMessage(text, nMCB);
    m_stStatus->setText(text);
}

void MainWindow::onFrame(int nMCB, const Mcb::McbFrame& frame) {
    ++m_frameCount;
    ++m_framesThisSec;
    m_grid->updateFrame(nMCB, frame);
    m_sidebar->setCounts(0, frame.scounter, frame.pcounter);
    m_stData->setText(QStringLiteral("%1 帧").arg(m_frameCount));
    m_stDI->setText(QStringLiteral("DI: 0x%1").arg(frame.regError_DI, 0, 16));
}

void MainWindow::onDeviceReady() {
    m_sidebar->setBoardReady(0, true);
    m_stStatus->setText(QStringLiteral("设备就绪"));
    if (m_ctrl) {
        // 按实际几何调整网格行数
        DMINFO d = m_ctrl->getDmInfo(0);
        const int rows = d.nRowNum * d.nColNum;
        if (rows > 0) m_grid->setRows(rows);
    }
}

void MainWindow::onRunStateChanged(bool running) {
    if (running) {
        m_runSeconds = 0;
        m_framesThisSec = 0;
        m_rateTimer->start();
    } else {
        m_rateTimer->stop();
        m_stRun->setText(QStringLiteral("STOPPED"));
    }
}
