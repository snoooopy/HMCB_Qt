#include "MainWindow.h"
#include "ui/DataGridWidget.h"
#include "ui/StatusSidebar.h"
#include "ui/MessageLog.h"
#include "ui/GraphView.h"
#include "ui/OneModuleView.h"
#include "core/McbController.h"
#include "core/MeasurementEngine.h"
#include "core/CsvResultWriter.h"
#include "core/IniFile.h"
#include "ui/GainCalibResultDialog.h"
#include "ui/SetIpDialog.h"
#include <QMenuBar>
#include <QMenu>
#include <QCoreApplication>
#include <QToolBar>
#include <QAction>
#include <QActionGroup>
#include <QStatusBar>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>
#include <QTimer>
#include <QMessageBox>
#include <QApplication>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

MainWindow::MainWindow(McbController* ctrl, QWidget* parent)
    : QMainWindow(parent), m_ctrl(ctrl) {
    setWindowTitle(QStringLiteral("HM MCB Test Program"));
    resize(1280, 800);

    // ---- 中央区：对照 IDD_MCB_TEST_FORM——左=大数据网格；
    // 右栏自上而下=MCB STATUS 组、SOCKET/PULSE/DIFF 组、MESSAGE 日志 ----
    m_grid = new DataGridWidget;
    m_sidebar = new StatusSidebar;
    m_log = new MessageLog;

    auto* right = new QWidget;
    auto* rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(4);
    rl->addWidget(m_sidebar);
    rl->addWidget(m_log, 1);   // 日志占右栏剩余高度（Windows 比例 ~1/3）

    auto* main = new QSplitter(Qt::Horizontal);
    main->addWidget(m_grid);
    main->addWidget(right);
    main->setStretchFactor(0, 4);
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

    buildActions();
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

void MainWindow::buildActions() {
    // 共享动作：菜单与工具栏挂同一 QAction（对应 MFC 同一命令 ID，勾选态自动同步）
    m_aSaveResult = new QAction(QIcon(QStringLiteral(":/tb/icons/tb_save_result.png")),
                                QStringLiteral("Save Measuring Result(&S)"), this);
    connect(m_aSaveResult, &QAction::triggered, this, &MainWindow::onSaveResult);

    // Trigger 三选一（互斥，默认 Inner=0 内触发）
    auto* trigGroup = new QActionGroup(this);
    // 触发动作直接挂工具栏（.rc: START/TRIGIN/TRIGOUT/STOP），必须带图标，
    // 否则 IconOnly 模式下 Qt 退化为文字按钮（Toolbar.bmp 无 cascade 位图，
    // Cascade 仅在菜单中，保持无图标）
    m_aTrigInner = trigGroup->addAction(QIcon(QStringLiteral(":/tb/icons/tb_trig_inner.png")),
                                        QStringLiteral("Inner"));
    m_aTrigOuter = trigGroup->addAction(QIcon(QStringLiteral(":/tb/icons/tb_trig_outer.png")),
                                        QStringLiteral("Outer"));
    m_aTrigCascade = trigGroup->addAction(QStringLiteral("Cascade"));
    for (auto* a : {m_aTrigInner, m_aTrigOuter, m_aTrigCascade}) {
        a->setCheckable(true);
        addAction(a);
    }
    m_aTrigInner->setChecked(true);
    connect(m_aTrigInner, &QAction::triggered, this, [this] { m_ctrl->setTrigger(0); });
    connect(m_aTrigOuter, &QAction::triggered, this, [this] { m_ctrl->setTrigger(1); });
    connect(m_aTrigCascade, &QAction::triggered, this, [this] { m_ctrl->setTrigger(2); });

    // 数据格式三选一（默认 DEC）
    auto* fmtGroup = new QActionGroup(this);
    m_aHex = fmtGroup->addAction(QIcon(QStringLiteral(":/tb/icons/tb_hex.png")),
                                 QStringLiteral("HEX"));
    m_aDec = fmtGroup->addAction(QIcon(QStringLiteral(":/tb/icons/tb_dec.png")),
                                 QStringLiteral("DEC"));
    m_aMv  = fmtGroup->addAction(QIcon(QStringLiteral(":/tb/icons/tb_mv.png")),
                                 QStringLiteral("mV"));
    for (auto* a : {m_aHex, m_aDec, m_aMv}) {
        a->setCheckable(true);
        addAction(a);
    }
    m_aDec->setChecked(true);
    connect(m_aHex, &QAction::triggered, this, [this] { m_grid->setDisplayFormat(DataGridWidget::DisplayFormat::Hex); });
    connect(m_aDec, &QAction::triggered, this, [this] { m_grid->setDisplayFormat(DataGridWidget::DisplayFormat::Dec); });
    connect(m_aMv,  &QAction::triggered, this, [this] { m_grid->setDisplayFormat(DataGridWidget::DisplayFormat::mV); });

    // 视图开关
    m_aGraph = new QAction(QIcon(QStringLiteral(":/tb/icons/tb_graph.png")),
                           QStringLiteral("Graph"), this);
    m_aGraph->setCheckable(true);
    addAction(m_aGraph);
    connect(m_aGraph, &QAction::toggled, this, [this](bool on) { m_graph->setVisible(on); });

    m_aOneModule = new QAction(QIcon(QStringLiteral(":/tb/icons/tb_one_module.png")),
                               QStringLiteral("One Module"), this);
    m_aOneModule->setCheckable(true);
    addAction(m_aOneModule);
    connect(m_aOneModule, &QAction::toggled, this, [this](bool on) { m_one->setVisible(on); });

    // 模式开关
    m_aTestMode = new QAction(QIcon(QStringLiteral(":/tb/icons/tb_test_mode.png")),
                              QStringLiteral("TEST mode"), this);
    m_aTestMode->setCheckable(true);
    addAction(m_aTestMode);
    connect(m_aTestMode, &QAction::toggled, this, [this](bool on) { m_ctrl->setTestMode(on); });

    m_aSendAll = new QAction(QStringLiteral("Send All Data mode"), this);
    m_aSendAll->setCheckable(true);
    addAction(m_aSendAll);
    connect(m_aSendAll, &QAction::toggled, this, [this](bool on) { m_ctrl->setSendAllData(on); });

    // 翻页
    m_aPrevPage = new QAction(QIcon(QStringLiteral(":/tb/icons/tb_prev_page.png")),
                              QStringLiteral("Previous Page"), this);
    addAction(m_aPrevPage);
    connect(m_aPrevPage, &QAction::triggered, this, [this] { m_grid->prevPage(); });

    m_aNextPage = new QAction(QIcon(QStringLiteral(":/tb/icons/tb_next_page.png")),
                              QStringLiteral("Next Page"), this);
    addAction(m_aNextPage);
    connect(m_aNextPage, &QAction::triggered, this, [this] { m_grid->nextPage(); });
}

void MainWindow::buildMenus() {
    // ===== 严格对照 MCB_TEST.rc IDR_MAINFRAME（文字/顺序/分隔线）=====
    // File(&F)
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("File(&F)"));
    fileMenu->addAction(m_aSaveResult);
    fileMenu->addAction(QStringLiteral("Save Origin Data(&D)"))->setEnabled(false);      // Phase 4
    fileMenu->addAction(QStringLiteral("Save Image Data(&I)"))->setEnabled(false);       // Phase 4
    fileMenu->addAction(QStringLiteral("Save Position Code Data(&C)"))->setEnabled(false); // Phase 4
    fileMenu->addSeparator();
    auto* aExit = fileMenu->addAction(QStringLiteral("Exit(&X)"));
    connect(aExit, &QAction::triggered, this, &MainWindow::onExit);

    // MCB Control(&M)
    auto* ctrlMenu = menuBar()->addMenu(QStringLiteral("MCB Control(&M)"));
    auto* aStart = ctrlMenu->addAction(QStringLiteral("Start(&S)"));
    auto* aStop  = ctrlMenu->addAction(QStringLiteral("Stop(&P)"));
    connect(aStart, &QAction::triggered, this, &MainWindow::onStart);
    connect(aStop,  &QAction::triggered, this, &MainWindow::onStop);
    ctrlMenu->addMenu(QStringLiteral("Trigger"))->addActions(
        {m_aTrigInner, m_aTrigOuter, m_aTrigCascade});
    auto* aReconn = ctrlMenu->addAction(QStringLiteral("Reconnect(&C)"));
    connect(aReconn, &QAction::triggered, this, &MainWindow::onReconnect);

    // Data View(&D)
    auto* viewMenu = menuBar()->addMenu(QStringLiteral("Data View(&D)"));
    viewMenu->addAction(m_aPrevPage);
    viewMenu->addAction(m_aNextPage);
    viewMenu->addSeparator();
    viewMenu->addMenu(QStringLiteral("Data Format"))->addActions({m_aHex, m_aDec, m_aMv});
    viewMenu->addAction(m_aGraph);
    viewMenu->addAction(QStringLiteral("NDet_Plot"))->setEnabled(false);   // Phase 4
    viewMenu->addAction(m_aOneModule);

    // Measure(&M)
    auto* measMenu = menuBar()->addMenu(QStringLiteral("Measure(&M)"));
    auto* aVz  = measMenu->addAction(QStringLiteral("Zero"));
    auto* aVn  = measMenu->addAction(QStringLiteral("Noise"));
    auto* aVnd = measMenu->addAction(QStringLiteral("Noise(DET)"));
    auto* aInl = measMenu->addAction(QStringLiteral("INL"));
    connect(aVz,  &QAction::triggered, this, &MainWindow::onMeasureVzero);
    connect(aVn,  &QAction::triggered, this, &MainWindow::onMeasureVnoise);
    connect(aVnd, &QAction::triggered, this, &MainWindow::onMeasureVnoiseD);
    connect(aInl, &QAction::triggered, this, &MainWindow::onMeasureInl);
    measMenu->addAction(QStringLiteral("Diversity in X Ray"))->setEnabled(false);  // Phase 4
    auto* aGain = measMenu->addAction(QStringLiteral("Gain automatic calibration"));
    connect(aGain, &QAction::triggered, this, &MainWindow::onMeasureGain);
    measMenu->addSeparator();
    measMenu->addAction(m_aTestMode);
    measMenu->addAction(QStringLiteral("Set DM Range"))->setEnabled(false);         // Phase 4
    measMenu->addAction(QStringLiteral("Set DM 2 Types Range"))->setEnabled(false); // Phase 4
    measMenu->addAction(m_aSendAll);
    measMenu->addAction(QStringLiteral("SetPOTMT"))->setEnabled(false);             // Phase 4
    measMenu->addAction(QStringLiteral("Set INTT"))->setEnabled(false);             // Phase 4
    measMenu->addSeparator();
    // .rc 中 "&&" 转义为显示 "&"
    measMenu->addAction(QStringLiteral("Set FNDM Parameteret &&  Scan Neutron Spectrum"))
        ->setEnabled(false);                                                        // Phase 4
    measMenu->addAction(QStringLiteral("FNDM DualEnergy Mode"))->setEnabled(false); // Phase 4
    auto* fnsMenu = measMenu->addMenu(QStringLiteral("Fast Neutron Source Type"));
    fnsMenu->addAction(QStringLiteral("Pulsed"))->setEnabled(false);                // Phase 4
    fnsMenu->addAction(QStringLiteral("Continued"))->setEnabled(false);             // Phase 4

    // Tools（Set New IP 已实现；其余 Phase 4）
    auto* toolsMenu = menuBar()->addMenu(QStringLiteral("Tools"));
    auto* aSetIp = toolsMenu->addAction(QStringLiteral("Set New IP"));
    connect(aSetIp, &QAction::triggered, this, &MainWindow::onToolsSetNewIp);
    toolsMenu->addAction(QStringLiteral("Dump EEPROM Key Data"))->setEnabled(false);
    toolsMenu->addAction(QStringLiteral("Dump All ECS Registers"))->setEnabled(false);

    // Help(&H)
    auto* helpMenu = menuBar()->addMenu(QStringLiteral("Help(&H)"));
    auto* aAbout = helpMenu->addAction(QStringLiteral("About HMCB_TEST(&A)..."));
    connect(aAbout, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::buildToolBar() {
    // ===== 严格对照 MCB_TEST.rc IDR_MAINFRAME TOOLBAR（按钮/顺序/分隔线）=====
    auto* tb = addToolBar(QStringLiteral("main"));
    tb->setMovable(false);
    tb->setIconSize(QSize(19, 17));       // MFC 工具栏原始尺寸 19x17
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    auto IC = [](const char* n) { return QIcon(QStringLiteral(":/tb/icons/tb_") + n + ".png"); };

    tb->addAction(m_aSaveResult);
    tb->addSeparator();
    auto* aSaveImg = tb->addAction(IC("save_image"), QStringLiteral("Save Image Data"));
    aSaveImg->setEnabled(false);          // Phase 4
    auto* aSaveOrigin = tb->addAction(IC("save_origin"), QStringLiteral("Save Origin Data"));
    aSaveOrigin->setEnabled(false);       // Phase 4
    tb->addSeparator();
    auto* aStart = tb->addAction(IC("start"), QStringLiteral("Start"));
    connect(aStart, &QAction::triggered, this, &MainWindow::onStart);
    tb->addAction(m_aTrigInner);
    tb->addAction(m_aTrigOuter);
    auto* aStop = tb->addAction(IC("stop"), QStringLiteral("Stop"));
    connect(aStop, &QAction::triggered, this, &MainWindow::onStop);
    tb->addSeparator();
    auto* aVz  = tb->addAction(IC("zero"), QStringLiteral("Zero"));
    connect(aVz, &QAction::triggered, this, &MainWindow::onMeasureVzero);
    auto* aVn  = tb->addAction(IC("noise"), QStringLiteral("Noise"));
    connect(aVn, &QAction::triggered, this, &MainWindow::onMeasureVnoise);
    auto* aVnd = tb->addAction(IC("noise_det"), QStringLiteral("Noise(DET)"));
    connect(aVnd, &QAction::triggered, this, &MainWindow::onMeasureVnoiseD);
    auto* aInl = tb->addAction(IC("inl"), QStringLiteral("INL"));
    connect(aInl, &QAction::triggered, this, &MainWindow::onMeasureInl);
    tb->addAction(IC("xdif"), QStringLiteral("Diversity in X Ray"))->setEnabled(false); // Phase 4
    auto* aGain = tb->addAction(IC("gain_calib"), QStringLiteral("Gain automatic calibration"));
    connect(aGain, &QAction::triggered, this, &MainWindow::onMeasureGain);
    tb->addAction(m_aTestMode);
    tb->addAction(IC("fndm"), QStringLiteral("Set FNDM Parameteret &&  Scan Neutron Spectrum"))
        ->setEnabled(false);                                                              // Phase 4
    tb->addAction(IC("ndet"), QStringLiteral("NDet_Plot"))->setEnabled(false);            // Phase 4
    tb->addSeparator();
    tb->addAction(m_aOneModule);
    tb->addSeparator();
    tb->addAction(m_aPrevPage);
    tb->addAction(m_aNextPage);
    tb->addSeparator();
    tb->addAction(m_aHex);
    tb->addAction(m_aDec);
    tb->addAction(m_aMv);
    tb->addAction(m_aGraph);
    tb->addSeparator();
    auto* aExit = tb->addAction(IC("exit"), QStringLiteral("Exit"));
    connect(aExit, &QAction::triggered, this, &MainWindow::onExit);
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
    connect(m_ctrl, &McbController::gainCalibReady, this, &MainWindow::onGainCalibReady);
}

// ---- 槽 ----
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
    // 对照 IDD_ABOUTBOX（HMCB_TEST / Copyright (C) 2023~2033）
    QMessageBox::about(this, QStringLiteral("About HMCB_TEST"),
        QStringLiteral("HMCB_TEST (Qt port)\nCopyright (C) 2023~2033\n基于 libmcbctrl | Phase 3"));
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
        // 按实际几何建表（对照 Windows DVREADY: ResetDataList+InitialDataList，
        // 几何来自 GETDMINFO=MCBCtrl.ini [MCB]MCB0ADNUM / [ADM]ROW/COLUMN）+ 停靠窗几何
        DMINFO d = m_ctrl->getDmInfo(0);
        const int rows = d.nRowNum * d.nColNum;
        if (rows > 0 && d.nDMNum > 0)
            m_grid->setGeometry(rows, d.nRowNum, d.nDMNum);
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
void MainWindow::onMeasureInl() {
    if (m_ctrl) { m_ctrl->measureInl(); m_log->appendMessage(QStringLiteral("开始 INL 测量…"), 0); }
}
void MainWindow::onMeasureGain() {
    if (m_ctrl) { m_ctrl->measureGain(); m_log->appendMessage(QStringLiteral("开始增益自动校准…"), 0); }
}
void MainWindow::onGainCalibReady() {
    m_log->appendMessage(QStringLiteral("增益自动校准完成"), 0);
    if (!m_ctrl) return;
    QList<QVector<uint8_t>> gains;
    QVector<int> counts;
    const DMINFO d = m_ctrl->getDmInfo(0);
    counts << qMax(0, int(d.nDMNum));
    gains << m_ctrl->dmGainValues(0);
    GainCalibResultDialog dlg(counts, gains, this);
    dlg.exec();
}

void MainWindow::onMeasurementReady(int function) {
    m_log->appendMessage(QStringLiteral("测量完成 (function=%1)").arg(function), 0);
    if (m_ctrl && m_ctrl->engine())
        m_grid->showMeasurement(m_ctrl->engine(), 0, function);
}

void MainWindow::onToolsSetNewIp() {
    // 对照 Windows MCB_TESTView：初值 IP 读 MCBCtrl.ini [CONNECTION] IPADDR
    IniFile ini;
    ini.setFileName(QCoreApplication::applicationDirPath()
                    + QStringLiteral("/config/MCBCtrl.ini"));
    QString cur = QStringLiteral("192.111.111.211");
    if (ini.readAll())
        cur = ini.getString(QStringLiteral("CONNECTION"), QStringLiteral("IPADDR"), cur);

    SetIpDialog dlg(this);
    dlg.setAddresses(cur, QStringLiteral("255.255.255.0"), QStringLiteral("192.111.111.1"));
    if (dlg.exec() != QDialog::Accepted) return;
    m_ctrl->setNewIp(dlg.ipAddress(), dlg.subnetMask(), dlg.gateway());
}

void MainWindow::onSaveResult() {
    if (!m_ctrl || !m_ctrl->engine()) return;
    auto& eng = *m_ctrl->engine();   // 非 const：保存后要 clearResults()
    if (!eng.bVzero(0) && !eng.bVnoise(0) && !eng.bInl(0)) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("尚无测量结果，请先执行 VZero/VNoise/INL。"));
        return;
    }
    const QString base = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存测量结果"),
        QStringLiteral("result00.csv"), QStringLiteral("CSV(*.csv)"));
    if (base.isEmpty()) return;
    const QString csvPath = base.toLower().endsWith(QStringLiteral(".csv"))
                          ? base : base + QStringLiteral(".csv");
    const QString txtPath = csvPath.left(csvPath.size() - 4) + QStringLiteral(".txt");

    CsvResultWriter::Options opt;
    const auto& cfg = eng.config();
    opt.vnFormat = cfg.vnFormat; opt.vzFormat = cfg.vzFormat;
    opt.adBit = cfg.adBit; opt.data2mV = cfg.data2mV;
    opt.hasVz = eng.bVzero(0); opt.hasVn = eng.bVnoise(0); opt.hasInl = eng.bInl(0);
    opt.inl = eng.inl(0); opt.diff = eng.inlDiff(0);
    const int n = qMax(qMax(eng.vz(0).size(), eng.vn(0).size()), eng.inl(0).size());

    bool ok = false;
    {
        QFile f(csvPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            CsvResultWriter::writeCsv(out, n, eng.vz(0), eng.vn(0), opt);
            ok = true;
        }
    }
    if (ok) {
        QFile f(txtPath);   // 忠实复刻：总是 csv+txt 双写（REPORTFORMAT 失效）
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            CsvResultWriter::writeTxt(out, n, eng.vz(0), eng.vn(0), opt);
        }
    }
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法写文件：") + csvPath);
        return;
    }
    eng.clearResults();                       // 忠实复刻：保存后清空（再存只剩 Channel）
    m_grid->setDisplayMode(DataGridWidget::DisplayMode::Raw);
    m_log->appendMessage(QStringLiteral("已保存：%1 + %2（结果已清空）").arg(csvPath, txtPath), 0);
}
