#include "MainWindow.h"
#include "ui/DataGridWidget.h"
#include "ui/StatusSidebar.h"
#include "ui/MessageLog.h"
#include "ui/GraphView.h"
#include "ui/OneModuleView.h"
#include "core/McbController.h"
#include "core/MeasurementEngine.h"
#include "core/CsvResultWriter.h"
#include "core/PosDataWriter.h"
#include "core/OriginDataWriter.h"
#include "core/ImageDataWriter.h"
#include "core/IniFile.h"
#include "ui/GainCalibResultDialog.h"
#include "ui/SetIpDialog.h"
#include "ui/FndmParaDialog.h"
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
#include <QSettings>

MainWindow::MainWindow(McbController* ctrl, QWidget* parent)
    : QMainWindow(parent), m_ctrl(ctrl) {
    setWindowTitle(QStringLiteral("HM MCB Test Program"));
    resize(1280, 800);
    {
        // 对照 MCB_TESTView.cpp:329：m_bSaveImg = [FILE] SAVEIMGFILE（缺省 1）
        IniFile ini;
        ini.setFileName(QCoreApplication::applicationDirPath()
                        + QStringLiteral("/config/MCB_TEST.ini"));
        m_bSaveImg = !ini.readAll() || ini.getInt(QStringLiteral("FILE"),
                                                  QStringLiteral("SAVEIMGFILE"), 1) == 1;
    }

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

    // 公司 logo：对照 IDD_MCB_TEST_FORM 右下角 IDB_BITMAP_LOGO
    // （CONTROL 130 SS_BITMAP 518,278 143×16 = res/LOGO.bmp 286×36 半尺寸）
    auto* logo = new QLabel(right);
    logo->setPixmap(QPixmap(QStringLiteral(":/tb/icons/logo_nuctech.png")));
    logo->setScaledContents(true);
    logo->setFixedSize(143, 16);
    auto* logoRow = new QHBoxLayout;
    logoRow->setContentsMargins(0, 0, 8, 0);
    logoRow->addStretch(1);
    logoRow->addWidget(logo);
    rl->addLayout(logoRow);

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
        // 帧计数/包计数/DI 由帧回调缓存到此统一刷新（对照 MFC OnTimer 1s 状态栏）
        m_stData->setText(QStringLiteral("%1 帧").arg(m_frameCount));
        m_stDI->setText(QStringLiteral("DI: 0x%1").arg(m_lastDI, 0, 16));
        m_sidebar->setCounts(0, m_lastSC, m_lastPC);
    });

    if (m_ctrl) wireController();
    // 初始未就绪：停止态/运行态两组按钮全部禁用（对照 Windows 启动即灰的
    // OnUpdate* 系，如截图 Stop 灰按钮；DVREADY 后 setReadyGated(true) 放开）
    setReadyGated(false);
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
    auto* aSaveOrigin = fileMenu->addAction(QStringLiteral("Save Origin Data(&D)"));
    connect(aSaveOrigin, &QAction::triggered, this, &MainWindow::onSaveOriginData);
    m_aSaveImg = fileMenu->addAction(QStringLiteral("Save Image Data(&I)"));
    connect(m_aSaveImg, &QAction::triggered, this, &MainWindow::onSaveImageData);
    auto* aSavePos = fileMenu->addAction(QStringLiteral("Save Position Code Data(&C)"));
    connect(aSavePos, &QAction::triggered, this, &MainWindow::onSavePosData);
    fileMenu->addSeparator();
    auto* aExit = fileMenu->addAction(QStringLiteral("Exit(&X)"));
    connect(aExit, &QAction::triggered, this, &MainWindow::onExit);
    m_readyGateActions += { aExit };   // Windows: !m_bStart||m_bStop（采集中禁退）

    // MCB Control(&M)
    auto* ctrlMenu = menuBar()->addMenu(QStringLiteral("MCB Control(&M)"));
    auto* aStart = ctrlMenu->addAction(QStringLiteral("Start(&S)"));
    auto* aStop  = ctrlMenu->addAction(QStringLiteral("Stop(&P)"));
    connect(aStart, &QAction::triggered, this, &MainWindow::onStart);
    connect(aStop,  &QAction::triggered, this, &MainWindow::onStop);
    m_readyGateActions += { aStart };                 // Windows: !start&&ready
    m_stopGateActions   += { aStop };                 // Windows: m_bStart&&DVREADY&&!m_bStop
    ctrlMenu->addMenu(QStringLiteral("Trigger"))->addActions(
        {m_aTrigInner, m_aTrigOuter, m_aTrigCascade});
    auto* aReconn = ctrlMenu->addAction(QStringLiteral("Reconnect(&C)"));
    connect(aReconn, &QAction::triggered, this, &MainWindow::onReconnect);
    m_readyGateActions += { aReconn };                // Windows: Enable(!m_bStart)

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
    auto* aXdif = measMenu->addAction(QStringLiteral("Diversity in X Ray"));
    connect(aXdif, &QAction::triggered, this, &MainWindow::onMeasureXdif);
    auto* aGain = measMenu->addAction(QStringLiteral("Gain automatic calibration"));
    connect(aGain, &QAction::triggered, this, &MainWindow::onMeasureGain);
    // 测量类命令：停止态+就绪才可用（对照各 OnUpdateMeasureXxx !m_bStart&&m_bDVReady）
    m_readyGateActions += { aVz, aVn, aVnd, aInl, aXdif, aGain };
    measMenu->addSeparator();
    measMenu->addAction(m_aTestMode);
    auto* aSetDmRange = measMenu->addAction(QStringLiteral("Set DM Range"));
    connect(aSetDmRange, &QAction::triggered, this, &MainWindow::onSetDmRange);
    auto* aSetDm2 = measMenu->addAction(QStringLiteral("Set DM 2 Types Range"));
    connect(aSetDm2, &QAction::triggered, this, &MainWindow::onSetDm2Range);
    m_readyGateActions += { aSetDmRange, aSetDm2 };   // Windows: !start&&ready&&!FND
    measMenu->addAction(m_aSendAll);
    auto* aPotmt = measMenu->addAction(QStringLiteral("SetPOTMT"));
    connect(aPotmt, &QAction::triggered, this, &MainWindow::onSetPotmt);
    auto* aIntt = measMenu->addAction(QStringLiteral("Set INTT"));
    connect(aIntt, &QAction::triggered, this, &MainWindow::onSetIntt);
    m_readyGateActions += { aPotmt, aIntt };   // Windows: Enable(!m_bStart && m_bDVReady)
    measMenu->addSeparator();
    // .rc 中 "&&" 转义为显示 "&"
    // FNDM 三项共用门控 !start && ready && ENFNDMODE（Windows m_bFND）
    const bool fnd = m_ctrl && m_ctrl->fndmEnabled();
    auto* aFndmSet = measMenu->addAction(QStringLiteral("Set FNDM Parameteret &&  Scan Neutron Spectrum"));
    aFndmSet->setEnabled(fnd);
    connect(aFndmSet, &QAction::triggered, this, &MainWindow::onFndmParaSet);
    if (fnd) m_readyGateActions += { aFndmSet };
    auto* aFndmDe = measMenu->addAction(QStringLiteral("FNDM DualEnergy Mode"));
    aFndmDe->setCheckable(true);
    aFndmDe->setEnabled(fnd);
    connect(aFndmDe, &QAction::triggered, this, &MainWindow::onFndmDeMode);
    auto* fnsMenu = measMenu->addMenu(QStringLiteral("Fast Neutron Source Type"));
    fnsMenu->setEnabled(fnd);
    m_aFndmNsPulsed = fnsMenu->addAction(QStringLiteral("Pulsed"));
    m_aFndmNsPulsed->setCheckable(true);
    m_aFndmNsPulsed->setChecked(true);                 // m_nNs 初值 0（Pulsed）
    connect(m_aFndmNsPulsed, &QAction::triggered, this, &MainWindow::onFndmNsPulsed);
    m_aFndmNsContinued = fnsMenu->addAction(QStringLiteral("Continued"));
    m_aFndmNsContinued->setCheckable(true);
    connect(m_aFndmNsContinued, &QAction::triggered, this, &MainWindow::onFndmNsContinued);
    if (fnd) m_readyGateActions += { aFndmDe };        // 采集期间关（子菜单随 fnsMenu 整体）
    // 共享动作（触发/模式/发数）：同 !m_bStart&&m_bDVReady 门控
    m_readyGateActions += { m_aTrigInner, m_aTrigOuter, m_aTrigCascade,
                            m_aTestMode, m_aSendAll };

    // Tools（Set New IP 已实现；其余 Phase 4）
    auto* toolsMenu = menuBar()->addMenu(QStringLiteral("Tools"));
    auto* aSetIp = toolsMenu->addAction(QStringLiteral("Set New IP"));
    connect(aSetIp, &QAction::triggered, this, &MainWindow::onToolsSetNewIp);
    auto* aDumpEeprom = toolsMenu->addAction(QStringLiteral("Dump EEPROM Key Data"));
    connect(aDumpEeprom, &QAction::triggered, this, &MainWindow::onDumpEeprom);
    auto* aDumpEcs = toolsMenu->addAction(QStringLiteral("Dump All ECS Registers"));
    connect(aDumpEcs, &QAction::triggered, this, &MainWindow::onDumpEcsRegisters);
    m_readyGateActions += { aDumpEeprom, aDumpEcs };   // 停止态+就绪才可用（同 Windows）

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
    tb->addAction(m_aSaveImg);
    auto* aSaveOriginTb = tb->addAction(IC("save_origin"), QStringLiteral("Save Origin Data"));
    connect(aSaveOriginTb, &QAction::triggered, this, &MainWindow::onSaveOriginData);
    tb->addSeparator();
    auto* aStart = tb->addAction(IC("start"), QStringLiteral("Start"));
    connect(aStart, &QAction::triggered, this, &MainWindow::onStart);
    m_readyGateActions += { aStart };
    tb->addAction(m_aTrigInner);
    tb->addAction(m_aTrigOuter);
    auto* aStop = tb->addAction(IC("stop"), QStringLiteral("Stop"));
    connect(aStop, &QAction::triggered, this, &MainWindow::onStop);
    m_stopGateActions += { aStop };
    tb->addSeparator();
    auto* aVz  = tb->addAction(IC("zero"), QStringLiteral("Zero"));
    connect(aVz, &QAction::triggered, this, &MainWindow::onMeasureVzero);
    auto* aVn  = tb->addAction(IC("noise"), QStringLiteral("Noise"));
    connect(aVn, &QAction::triggered, this, &MainWindow::onMeasureVnoise);
    auto* aVnd = tb->addAction(IC("noise_det"), QStringLiteral("Noise(DET)"));
    connect(aVnd, &QAction::triggered, this, &MainWindow::onMeasureVnoiseD);
    auto* aInl = tb->addAction(IC("inl"), QStringLiteral("INL"));
    connect(aInl, &QAction::triggered, this, &MainWindow::onMeasureInl);
    auto* aXdifTb = tb->addAction(IC("xdif"), QStringLiteral("Diversity in X Ray"));
    connect(aXdifTb, &QAction::triggered, this, &MainWindow::onMeasureXdif);
    auto* aGain = tb->addAction(IC("gain_calib"), QStringLiteral("Gain automatic calibration"));
    connect(aGain, &QAction::triggered, this, &MainWindow::onMeasureGain);
    m_readyGateActions += { aVz, aVn, aVnd, aInl, aXdifTb, aGain };
    tb->addAction(m_aTestMode);
    auto* aFndmSetTb = tb->addAction(IC("fndm"),
        QStringLiteral("Set FNDM Parameteret &&  Scan Neutron Spectrum"));
    connect(aFndmSetTb, &QAction::triggered, this, &MainWindow::onFndmParaSet);
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
    m_readyGateActions += { aExit };
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
    if (m_aSaveImg && !m_aSaveImg->isEnabled() && m_bSaveImg)
        m_aSaveImg->setEnabled(true);     // 对照 OnUpdateSavedata：有帧数据才可用
    ++m_framesThisSec;
    // 只缓存，不做任何 setText（每帧 setText 在真机包率下饱和 GUI 线程；
    // 文本统一由 1Hz rateTimer 刷新，网格由 DataGridWidget 250ms 合并）
    m_lastSC = frame.scounter;
    m_lastPC = frame.pcounter;
    m_lastDI = frame.regError_DI;
    m_grid->updateFrame(nMCB, frame);
}

void MainWindow::onDeviceReady() {
    m_sidebar->setBoardReady(0, true);
    m_stStatus->setText(QStringLiteral("设备就绪"));
    m_ready = true;
    setReadyGated(true);        // 对照 Windows DVREADY 后放开停止态命令
    if (m_ctrl) {
        // 按实际几何建表（对照 Windows DVREADY: ResetDataList+InitialDataList，
        // 几何来自 GETDMINFO=MCBCtrl.ini [MCB]MCB0ADNUM / [ADM]ROW/COLUMN）+ 停靠窗几何
        DMINFO d = m_ctrl->getDmInfo(0);
        const int rows = d.nRowNum * d.nColNum;
        if (rows > 0 && d.nDMNum > 0)
            m_grid->setGeometry(rows, d.nRowNum, d.nDMNum);
        // 实际 AD 位数（GETDMADBIT）：mV 换算/ExDATA 标度依据（实机 16bit）
        m_grid->setAdBit(m_ctrl->adBit());
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
    setReadyGated(!running);   // 采集期间关闭停止态命令（对照 Windows !m_bStart）
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
void MainWindow::onMeasureXdif() {
    // 日志由 Controller statusMessage 发（未做 Zero 时同样有提示）
    if (m_ctrl) m_ctrl->measureXdif();
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

void MainWindow::onDumpEeprom()       { m_ctrl->dumpEepromKeyData(); }
void MainWindow::onDumpEcsRegisters() { m_ctrl->dumpAllEcsRegisters(); }
void MainWindow::onSetPotmt()         { m_ctrl->setPotmtZero(); }
void MainWindow::onSetIntt()          { m_ctrl->setInttZero(); }
void MainWindow::onSetDmRange()       { m_ctrl->setDmRangeFromIni(); }
void MainWindow::onSetDm2Range()      { m_ctrl->setDm2TypeRangeFromIni(); }

void MainWindow::onSavePosData() {
    // 对照 OnSavePosdata：txt 每帧一行 Packet/AngleCode/ConveyorCode
    if (!m_ctrl || !m_ctrl->engine()) return;
    const auto frames = m_ctrl->engine()->posSnapshot(0);
    if (frames.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("尚无 POS 数据，请先采集。"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Position Code Data"), QString(),
        QStringLiteral("Text file(*.txt)"));
    if (path.isEmpty()) return;
    if (PosDataWriter::write(path, frames))
        m_log->appendMessage(QStringLiteral("已保存：%1（%2 帧）").arg(path).arg(frames.size()), 0);
    else
        QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Open File Error!"));
}

// ---- Phase 4-6：FNDM 双能模式 / 快中子源类型 ----
void MainWindow::onFndmParaSet() {
    // 对照 OnFndmSet：打开前 GETFNDMPARA 回填 lastPara，再弹模态框
    if (!m_ctrl) return;
    const auto cur = m_ctrl->getFndmPara();
    if (!cur.isEmpty())
        FndmParaBuilder::extractCurrent(cur, m_fndmLast.module, m_fndmLast.row,
                                        m_fndmLast.column, m_fndmLast.nValBias,
                                        m_fndmLast.nValThrL, m_fndmLast.nValThrU);
    FndmParaDialog dlg(m_ctrl, &m_fndmLast, this);
    dlg.exec();
}
void MainWindow::onFndmDeMode() {
    // 对照 OnFndmDe：toggle 后发 SETDEMODE，菜单 check 态跟随
    if (!m_ctrl) return;
    QAction* a = qobject_cast<QAction*>(sender());
    const bool on = a ? a->isChecked() : false;
    m_ctrl->setFndmDeMode(on);
}
void MainWindow::onFndmNsPulsed() {
    // 对照 OnFastneutronsourcetypePulse：仅 m_nNs!=0 时切换并发送
    if (!m_ctrl || m_fndmNs == 0) { if (m_aFndmNsPulsed) m_aFndmNsPulsed->setChecked(true); return; }
    m_fndmNs = 0;
    m_aFndmNsPulsed->setChecked(true);
    m_aFndmNsContinued->setChecked(false);
    m_ctrl->setFndmNsType(0);
}
void MainWindow::onFndmNsContinued() {
    if (!m_ctrl || m_fndmNs == 1) { if (m_aFndmNsContinued) m_aFndmNsContinued->setChecked(false); return; }
    m_fndmNs = 1;
    m_aFndmNsContinued->setChecked(true);
    m_aFndmNsPulsed->setChecked(false);
    m_ctrl->setFndmNsType(1);
}

void MainWindow::onSaveOriginData() {
    // 对照 OnSavedatao：.oridat 二进制，每帧前 128 通道 int32 逐点
    if (!m_ctrl || !m_ctrl->engine()) return;
    const auto frames = m_ctrl->engine()->ringSnapshot(0);
    if (frames.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("尚无采集数据，请先采集。"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Origin Data"), QString(),
        QStringLiteral("数据文件(*.oridat)"));
    if (path.isEmpty()) return;
    if (OriginDataWriter::write(path, frames))
        m_log->appendMessage(QStringLiteral("已保存：%1（%2 帧）").arg(path).arg(frames.size()), 0);
    else
        QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Open File Error!"));
}

void MainWindow::onSaveImageData() {
    // 对照 OnSavedata（MCB_TESTView.cpp:4134-4595）：ORI/IMG/CSV 三格式，
    // filter 索引经 QSettings 记忆（Windows 注册表 HMCB_TEST\SaveIMGFileType\Index 缺省 2）
    if (!m_ctrl || !m_ctrl->engine()) return;
    const auto frames = m_ctrl->engine()->ringSnapshot(0);
    if (frames.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("尚无采集数据，请先采集。"));
        return;
    }
    ImageDataWriter::Options opt;
    {
        // 对照 331-334 行：[FILE] 四开关 + 431 行 [ADM] ENFNDMODE（GM 行移位例外）
        IniFile ini;
        ini.setFileName(QCoreApplication::applicationDirPath()
                        + QStringLiteral("/config/MCB_TEST.ini"));
        const bool has = ini.readAll();
        opt.pixel16    = !has || ini.getInt(QStringLiteral("FILE"), QStringLiteral("IMGFILEPIXEL"), 16) != 32;
        opt.oriGM      = has && ini.getInt(QStringLiteral("FILE"), QStringLiteral("GMCORINORI"), 0) == 1;
        opt.allColumn  = !has || ini.getInt(QStringLiteral("FILE"), QStringLiteral("ALLCOLUMN"), 1) == 1;
        opt.saveExdata = !has || ini.getInt(QStringLiteral("FILE"), QStringLiteral("SAVE_EXDATA"), 1) == 1;
        opt.fnd        = has && ini.getInt(QStringLiteral("ADM"), QStringLiteral("ENFNDMODE"), 0) == 1;
    }
    opt.adBit = m_ctrl->engine()->config().adBit;
    const DMINFO d = m_ctrl->getDmInfo(0);
    opt.nRowNum = int(d.nRowNum);
    opt.nColNum = int(d.nColNum);
    opt.nAdm = (opt.nRowNum > 0 && opt.nColNum > 0)
             ? qMax(1, int(d.nAllChnNum) / (opt.nRowNum * opt.nColNum)) : 1;

    QSettings settings(QStringLiteral("HMCB_TEST"), QStringLiteral("SaveIMGFileType"));
    const int lastIdx = qBound(1, settings.value(QStringLiteral("Index"), 2).toInt(), 3);
    QFileDialog dlg(this, QStringLiteral("Save Image Data"));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setNameFilters({QStringLiteral("ORI format file(*.ori)"),
                        QStringLiteral("image format file(*.img)"),
                        QStringLiteral("CSV(*.csv)")});
    dlg.selectNameFilter(dlg.nameFilters().value(lastIdx - 1));
    if (dlg.exec() != QDialog::Accepted || dlg.selectedFiles().isEmpty()) return;
    const QString path = dlg.selectedFiles().constFirst();
    const int idx = dlg.nameFilters().indexOf(dlg.selectedNameFilter()) + 1;
    const auto fmt = idx == 1 ? ImageDataWriter::Format::Ori
                  : idx == 3 ? ImageDataWriter::Format::Csv
                             : ImageDataWriter::Format::Img;
    settings.setValue(QStringLiteral("Index"), idx);

    QString err;
    if (!ImageDataWriter::save(path, fmt, frames, m_ctrl->engine()->exSnapshot(0), opt, &err)) {
        QMessageBox::warning(this, QStringLiteral("Error"), err);
        return;
    }
    m_log->appendMessage(QStringLiteral("已保存：%1（%2 帧）").arg(path).arg(frames.size()), 0);
}

// Windows Enable(!m_bStart && m_bDVReady)：就绪后开，采集期间关；
// Stop 组反相（m_bStart&&DVREADY&&!m_bStop），未就绪时两组皆禁
void MainWindow::setReadyGated(bool enabled) {
    for (QAction* a : m_readyGateActions)
        a->setEnabled(enabled && m_ready);
    for (QAction* a : m_stopGateActions)
        a->setEnabled(!enabled && m_ready);
}

void MainWindow::onSaveResult() {
    if (!m_ctrl || !m_ctrl->engine()) return;
    auto& eng = *m_ctrl->engine();   // 非 const：保存后要 clearResults()
    if (!eng.bVzero(0) && !eng.bVnoise(0) && !eng.bInl(0) && !eng.bXdif(0)) {
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
    opt.hasXdif = eng.bXdif(0);
    opt.vzX = eng.vzx(0); opt.xDif = eng.xdif(0);
    // aXptos 对照 MCB_TESTView:741-743：drange[rangeMode]/((1<<adBit)*0.996)/ddose，
    // ddose=xdoserate/3600*intt/1e6。三参数读 MCBCtrl.ini（缺省同 Windows）
    {
        IniFile ini;
        ini.setFileName(QCoreApplication::applicationDirPath()
                        + QStringLiteral("/config/MCBCtrl.ini"));
        const bool has = ini.readAll();
        const double xdoserate = has ? ini.getString(
            QStringLiteral("XRAY"), QStringLiteral("XDOSERATE"),
            QStringLiteral("1.877e5")).toDouble() : 1.877e5;
        const int intt = qMax(1, has ? ini.getInt(
            QStringLiteral("MODE0"), QStringLiteral("INTT"), 400) : 400);
        const int rangeMode = qBound(0, has ? ini.getInt(
            QStringLiteral("RANGE0"), QStringLiteral("MCB0DM0"), 2) : 2, 7);
        static const double drange[8] = {12.5, 50, 100, 150, 200, 250, 300, 350};
        const double ddose = qMax(1.0, xdoserate) / 3600.0 * double(intt) / 1e6;
        opt.aXptos = drange[rangeMode] / (std::pow(2.0, cfg.adBit) * 0.996) / ddose;
    }
    const int n = qMax(qMax(qMax(eng.vz(0).size(), eng.vn(0).size()), eng.inl(0).size()),
                       eng.vzx(0).size());

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
