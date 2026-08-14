#include "McbController.h"
#include "MockMcbBackend.h"
#include "RealMcbBackend.h"
#include "MeasurementEngine.h"
#include "IniFile.h"
#include <QDir>
#include <QCoreApplication>

McbController::McbController(QObject* parent) : QObject(parent) {
    m_engine = std::make_unique<MeasurementEngine>();
}

void McbController::setBackendMode(const QString& mode) {
    m_mode = mode.toLower().trimmed();
}

void McbController::startUp() {
    if (m_be) return;   // 已启动
    if (m_mode == QStringLiteral("real"))
        m_be = std::make_unique<RealMcbBackend>();
    else
        m_be = std::make_unique<MockMcbBackend>();

    // 状态回调（可能来自库/仿真工作线程）：直接 emit，带 context 的连接会 Queued。
    m_be->setStatusCallback([this](WPARAM category, int nMCB, uint32_t code) {
        emit statusMessage(
            QStringLiteral("[%1] %2")
                .arg(Mcb::statusCategoryStr(category), Mcb::statusCodeStr(code)),
            nMCB);
        if (category == MCB_DVREADY) emit deviceReady();
        if (category == MCB_STOPOK) handleStopok();
    });

    // 数据回调（数据线程）：累积到测量引擎，再 emit frameReady（跨线程需 McbFrame metatype）。
    m_be->setDataCallback([this](const Mcb::McbFrame& f) {
        if (m_engine) m_engine->appendFrame(f.nMCB, f.channels);
        emit frameReady(f.nMCB, f);
    });

    // 读 config/MCB_TEST.ini 注入测量引擎
    IniFile ini;
    ini.setFileName(QCoreApplication::applicationDirPath() + QDir::separator()
                    + QStringLiteral("config/MCB_TEST.ini"));
    if (ini.readAll()) {
        MeasurementEngine::Config cfg;
        cfg.vzSampleNum = ini.getInt("VZERO", "SAMPLENUM", 512);
        cfg.vzMax       = ini.getDouble("VZERO", "MAXVZ", 0);
        cfg.vzMin       = ini.getDouble("VZERO", "MINVZ", 0);
        cfg.vzFormat    = ini.getInt("VZERO", "FORMAT", 0);
        cfg.vnSampleNum = ini.getInt("VNOISE", "SAMPLENUM", 2048);
        cfg.vnMax       = ini.getDouble("VNOISE", "MAXVN", 0);
        cfg.vnMin       = ini.getDouble("VNOISE", "MINVN", 0);
        cfg.vndMax      = ini.getDouble("VNOISE", "MAXVND", 0);
        cfg.vndMin      = ini.getDouble("VNOISE", "MINVND", 0);
        cfg.vnFormat    = ini.getInt("VNOISE", "FORMAT", 0);
        m_engine->applyConfig(cfg);
    }
}

void McbController::shutdown() {
    if (!m_be) return;
    m_be->close();
    m_be.reset();
}

void McbController::initializeBackend() { if (m_be) m_be->initialize(); }

void McbController::start(uint32_t sampleCount) {
    if (!m_be) return;
    m_running = true;
    m_be->start(sampleCount);
    emit runStateChanged(true);
}

void McbController::stop() {
    if (!m_be) return;
    m_be->stop();
    if (m_running) { m_running = false; emit runStateChanged(false); }
}

void McbController::measure(int function, uint32_t sampleCount) {
    if (!m_be || !m_engine) return;
    m_measFn = function;
    const int N = (function == 0)
        ? m_engine->config().vzSampleNum
        : m_engine->config().vnSampleNum;
    start(static_cast<uint32_t>(N > 0 ? N : (int)sampleCount));
}

void McbController::handleStopok() {
    if (m_measFn >= 0 && m_engine) {
        if (m_measFn == 0) m_engine->calcZero(0);
        else               m_engine->calcNoise(0);   // Noise / NoiseD 同算 FWHM
        const int fn = m_measFn;
        m_measFn = -1;
        emit measurementReady(fn);
    }
    if (m_running) { m_running = false; emit runStateChanged(false); }  // 有限自停也通知 UI
}

void McbController::setTrigger(int mode)        { if (m_be) m_be->setTrigger(mode); }
void McbController::setRangeMode(int range)     { if (m_be) m_be->setRangeMode(range); }
void McbController::setTiming(uint32_t potmt, uint32_t potmw, uint32_t intt) {
    if (m_be) m_be->setTiming(potmt, potmw, intt);
}

DMINFO    McbController::getDmInfo(int nMCB) { return m_be ? m_be->getDmInfo(nMCB) : DMINFO{}; }
uint32_t  McbController::getMcbNum()         { return m_be ? m_be->getMcbNum() : 0; }
bool      McbController::getRunStatus()      { return m_be ? m_be->getRunStatus() : false; }
