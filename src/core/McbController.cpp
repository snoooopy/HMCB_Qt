#include "McbController.h"
#include "MockMcbBackend.h"
#include "RealMcbBackend.h"
#include "MeasurementEngine.h"
#include "IniFile.h"
#include "XmlGainWriter.h"
#include <QDir>
#include <QTimer>
#include <QCoreApplication>
#include <cmath>

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
        // STOPOK 延迟到本对象线程处理：回调来自数据线程，直接重启采集会在
        // 数据线程内 join 自身（std::system_error: Resource deadlock avoided）。
        if (category == MCB_STOPOK)
            QTimer::singleShot(0, this, [this, natural = (code != 0)] {
                handleStopok(natural);
            });
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
        cfg.inlSampleNum = ini.getInt("INL", "SAMPLENUM", 512);
        cfg.inlDaMax     = ini.getInt("INL", "DAMAX", 16100);
        cfg.inlPoint     = ini.getInt("INL", "MEASUREPOINT", 12);
        cfg.inlMax       = ini.getDouble("INL", "MAXINL", 33554432.0) / 1000.0;
        cfg.diffMax      = ini.getDouble("INL", "MAXDIF", 33554432.0) / 100.0;
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

void McbController::measureInl() {
    if (!m_be || !m_engine) return;
    const auto& cfg = m_engine->config();
    m_measFn = 2;
    m_engine->reset(0);
    m_engine->beginInl(cfg.inlPoint);
    m_inlTestPoint = 0;
    DMINFO d = m_be->getDmInfo(0);
    m_chPerDm = d.nDMNum > 0 ? d.nAllChnNum / d.nDMNum : 128;
    m_be->setDa(m_engine->inlDaCode(0));      // 第 1 点：摆幅 0
    start(static_cast<uint32_t>(cfg.inlSampleNum > 0 ? cfg.inlSampleNum : 512));
    emit statusMessage(QStringLiteral("[MCB0] INL 测量开始（%1 点）").arg(cfg.inlPoint), 0);
}

void McbController::handleStopok(bool natural) {
    if (m_measFn == 2) { handleInlStop(natural); return; }
    if (m_measFn == 6) { handleGainStop(natural); return; }
    if (m_measFn >= 0 && m_engine) {          // fn 0/1/3：两种停都计算（Phase 2 行为）
        if (m_measFn == 0) m_engine->calcZero(0);
        else               m_engine->calcNoise(0);   // Noise / NoiseD 同算 FWHM
        const int fn = m_measFn;
        m_measFn = -1;
        emit measurementReady(fn);
    }
    if (m_running) { m_running = false; emit runStateChanged(false); }  // 有限自停也通知 UI
}

void McbController::handleInlStop(bool natural) {
    if (natural) {
        m_engine->addInlPoint(0, m_inlTestPoint);
        ++m_inlTestPoint;
        if (m_inlTestPoint < m_engine->config().inlPoint) {
            m_engine->reset(0);               // 清环，保证均值只含本点帧
            m_be->setDa(m_engine->inlDaCode(m_inlTestPoint));
            start(static_cast<uint32_t>(m_engine->config().inlSampleNum));
            return;
        }
    }
    finalizeInl();   // 测完 / 命令停：用已采点收尾（<5 点丢弃）
}

void McbController::finalizeInl() {
    const bool ok = m_engine && m_engine->calcInl(0, m_chPerDm);
    const int fn = m_measFn;
    m_measFn = -1;
    if (m_running) { m_running = false; emit runStateChanged(false); }
    if (ok) emit measurementReady(fn);
    else emit statusMessage(QStringLiteral("[MCB0] INL 点数不足，已丢弃"), 0);
}

// ---- 增益自动校准（对照 OnGainAutoCalib / VGainValueAsVzeroAnalysisCal:2699）----

void McbController::measureGain() {
    if (!m_be || !m_engine) return;
    m_measFn = 6;
    m_gainStep = 0;
    m_dmGainBoard[0] = QVector<uint8_t>(65, 0);   // 全 DM 0 档起步（Windows 硬编码）
    m_engine->reset(0);
    applyDmRange(0);
    emit statusMessage(QStringLiteral("[MCB0] 增益自动校准开始"), 0);
    scheduleGainAcquire();
}

void McbController::applyDmRange(int nMCB) {
    Q_UNUSED(nMCB);
    if (m_be) m_be->setDmRange(m_dmGainBoard[0]);
}

void McbController::scheduleGainAcquire() {
    const int delay = m_engine->config().gainDelayMs > 0
                    ? m_engine->config().gainDelayMs : 2000;   // Windows Sleep(2000)
    QTimer::singleShot(delay, this, [this]() {
        if (m_measFn == 6 && m_be)
            start(static_cast<uint32_t>(m_engine->config().gainFrames));
    });
}

void McbController::handleGainStop(bool natural) {
    if (!natural) {   // 用户中止：不写回
        m_measFn = -1;
        if (m_running) { m_running = false; emit runStateChanged(false); }
        emit statusMessage(QStringLiteral("[MCB0] 增益校准被中止，未写回"), 0);
        return;
    }
    const auto& cfg = m_engine->config();
    m_engine->calcZero(0);                       // 复用均值（对照 VGainValueAsVzeroAnalysisCal）
    const double th = cfg.gainThreshold * std::pow(2.0, cfg.adBit);
    const auto& z = m_engine->vz(0);
    DMINFO d = m_be->getDmInfo(0);
    const int chPerDm = d.nDMNum > 0 ? d.nAllChnNum / d.nDMNum : 128;
    bool raised = false;
    for (int ch = 0; ch < z.size(); ++ch) {
        if (z[ch] > th) {                        // 超阈通道 → 所属 DM 档+1（只升不降，饱和 7）
            const int dm = ch / chPerDm;
            if (dm < 64 && m_dmGainBoard[0][1 + dm] < 7) {
                ++m_dmGainBoard[0][1 + dm];
                raised = true;
            }
        }
    }
    if (raised && m_gainStep + 1 < cfg.gainMaxSteps) {
        ++m_gainStep;
        applyDmRange(0);
        emit statusMessage(QStringLiteral("[MCB0] 增益升档（第 %1 轮）").arg(m_gainStep), 0);
        scheduleGainAcquire();
        return;
    }
    finalizeGain();
}

void McbController::finalizeGain() {
    // ini + XML 双写（对照 WriteGainValueToConfigureFile:2829-2897）
    DMINFO d = m_be ? m_be->getDmInfo(0) : DMINFO{};
    const int nDm = d.nDMNum > 0 ? d.nDMNum
                  : (m_dmGainBoard[0].isEmpty() ? 0 : m_dmGainBoard[0].size() - 1);
    IniFile ini;
    ini.setFileName(m_gainIniPath.isEmpty()
        ? QCoreApplication::applicationDirPath() + QStringLiteral("/config/MCBCtrl.ini")
        : m_gainIniPath);
    ini.readAll();                                // 文件/节不存在则创建
    QVector<uint8_t> gains(nDm);
    for (int j = 0; j < nDm; ++j) {
        ini.setInt(QStringLiteral("RANGE0"), QStringLiteral("MCB0DM%1").arg(j),
                   m_dmGainBoard[0][1 + j]);
        gains[j] = m_dmGainBoard[0][1 + j];
    }
    ini.writeAll();
    const QString xmlPath = m_gainXmlPath.isEmpty()
        ? QCoreApplication::applicationDirPath() + QStringLiteral("/DIASConfig.xml")
        : m_gainXmlPath;
    if (!XmlGainWriter::write(xmlPath, 0, gains))
        emit statusMessage(QStringLiteral("[MCB0] DIASConfig.xml 写入失败"), 0);
    m_measFn = -1;
    if (m_running) { m_running = false; emit runStateChanged(false); }
    emit gainCalibReady();
}

void McbController::setGainWritePaths(const QString& iniPath, const QString& xmlPath) {
    m_gainIniPath = iniPath;
    m_gainXmlPath = xmlPath;
}

QVector<uint8_t> McbController::dmGainValues(int nMCB) const {
    const auto& b = m_dmGainBoard[nMCB & 7];
    return b.isEmpty() ? QVector<uint8_t>() : b.mid(1);
}

void McbController::setTrigger(int mode)        { if (m_be) m_be->setTrigger(mode); }
void McbController::setRangeMode(int range)     { if (m_be) m_be->setRangeMode(range); }
void McbController::setTiming(uint32_t potmt, uint32_t potmw, uint32_t intt) {
    if (m_be) m_be->setTiming(potmt, potmw, intt);
}

DMINFO    McbController::getDmInfo(int nMCB) { return m_be ? m_be->getDmInfo(nMCB) : DMINFO{}; }
uint32_t  McbController::getMcbNum()         { return m_be ? m_be->getMcbNum() : 0; }
bool      McbController::getRunStatus()      { return m_be ? m_be->getRunStatus() : false; }
