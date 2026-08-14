#include "MainWindow.h"
#include "ui/DataGridWidget.h"
#include "ui/StatusSidebar.h"
#include "ui/MessageLog.h"
#include "ui/GraphView.h"
#include "ui/OneModuleView.h"
#include "core/McbController.h"
#include "core/MeasurementEngine.h"
#include "core/CsvResultWriter.h"
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
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

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

    // Phase 2 停靠窗：波形 / 单模块（默认隐藏，菜单切换）
    m_graph = new GraphView(this);
    m_one   = new OneModuleView(this);
    addDockWidget(Qt::RightDockWidgetArea, m_graph);
    addDockWidget(Qt::RightDockWidgetArea, m_one);
    m_graph->hide(); m_one->hide();
    if (m_ctrl && m_ctrl->engine()) {
        m_graph->setEngine(m_ctrl->engine());
        m_one->setEngine(m_ctrl->engine());
    }

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
    auto* aSave = fileMenu->addAction(QStringLiteral("保存测量结果(&S)"));
    connect(aSave, &QAction::triggered, this, &MainWindow::onSaveResult);
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

    // 数据视图（Phase 2 启用：波形 / 单模块）
    auto* viewMenu = menuBar()->addMenu(QStringLiteral("数据视图(&V)"));
    auto* aGraph = viewMenu->addAction(QStringLiteral("波形(&G)"));
    auto* aOne   = viewMenu->addAction(QStringLiteral("单模块(&M)"));
    aGraph->setCheckable(true); aOne->setCheckable(true);
    connect(aGraph, &QAction::toggled, this, &MainWindow::onToggleGraph);
    connect(aOne,   &QAction::toggled, this, &MainWindow::onToggleOneModule);

    // 测量（Phase 2 启用：VZero / VNoise / VNoise1(NoiseD)）
    auto* measMenu = menuBar()->addMenu(QStringLiteral("测量(&E)"));
    auto* aVz  = measMenu->addAction(QStringLiteral("VZero"));
    auto* aVn  = measMenu->addAction(QStringLiteral("VNoise"));
    auto* aVnd = measMenu->addAction(QStringLiteral("VNoise1(NoiseD)"));
    connect(aVz,  &QAction::triggered, this, &MainWindow::onMeasureVzero);
    connect(aVn,  &QAction::triggered, this, &MainWindow::onMeasureVnoise);
    connect(aVnd, &QAction::triggered, this, &MainWindow::onMeasureVnoiseD);

    // 工具 —— 后续阶段启用
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
    connect(m_ctrl, &McbController::measurementReady, this, &MainWindow::onMeasurementReady);
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
        QStringLiteral("HMCB 探测器测试软件 Qt 版\n基于 libmcbctrl\nPhase 2：可视化与基础测量"));
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
        // 按实际几何调整网格行数 + 停靠窗几何
        DMINFO d = m_ctrl->getDmInfo(0);
        const int rows = d.nRowNum * d.nColNum;
        if (rows > 0) m_grid->setRows(rows);
        m_graph->setBoard(0);
        m_one->setBoard(0);
        m_one->setModuleGeometry(d.nRowNum, d.nColNum, d.nDMNum);
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

// ---- Phase 2：测量 / 视图 / 保存 ----
void MainWindow::onMeasureVzero() {
    if (m_ctrl) { m_ctrl->measure(0, 0); m_log->appendMessage(QStringLiteral("开始 Zero 测量…"), 0); }
}
void MainWindow::onMeasureVnoise() {
    if (m_ctrl) { m_ctrl->measure(1, 0); m_log->appendMessage(QStringLiteral("开始 Noise 测量…"), 0); }
}
void MainWindow::onMeasureVnoiseD() {
    if (m_ctrl) { m_ctrl->measure(3, 0); m_log->appendMessage(QStringLiteral("开始 NoiseD 测量…"), 0); }
}

void MainWindow::onToggleGraph(bool on)     { m_graph->setVisible(on); }
void MainWindow::onToggleOneModule(bool on) { m_one->setVisible(on); }

void MainWindow::onMeasurementReady(int function) {
    m_log->appendMessage(QStringLiteral("测量完成 (function=%1)").arg(function), 0);
    if (m_ctrl && m_ctrl->engine())
        m_grid->showMeasurement(m_ctrl->engine(), 0, function);
}

void MainWindow::onSaveResult() {
    if (!m_ctrl || !m_ctrl->engine()) return;
    const auto& eng = *m_ctrl->engine();
    if (eng.vz(0).isEmpty() && eng.vn(0).isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("尚无测量结果，请先执行 VZero/VNoise。"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存测量结果"),
        QStringLiteral("result00.csv"),
        QStringLiteral("CSV(*.csv);;TXT(*.txt)"));
    if (path.isEmpty()) return;
    const bool isCsv = path.toLower().endsWith(QStringLiteral(".csv"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法写文件：") + path);
        return;
    }
    QTextStream out(&f);
    CsvResultWriter::Options opt;
    const auto& cfg = eng.config();
    opt.vnFormat = cfg.vnFormat; opt.vzFormat = cfg.vzFormat;
    opt.adBit = cfg.adBit; opt.data2mV = cfg.data2mV; opt.csv = isCsv;
    const int n = qMax(eng.vz(0).size(), eng.vn(0).size());
    CsvResultWriter::write(out, n, eng.vz(0), eng.vn(0), opt);
    m_log->appendMessage(QStringLiteral("已保存：%1").arg(path), 0);
}
