#include "McbController.h"
#include "MockMcbBackend.h"
#include "RealMcbBackend.h"

McbController::McbController(QObject* parent) : QObject(parent) {}

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
    });

    // 数据回调（数据线程）：同样直接 emit frameReady（跨线程需 McbFrame metatype）。
    m_be->setDataCallback([this](const Mcb::McbFrame& f) {
        emit frameReady(f.nMCB, f);
    });
}

void McbController::shutdown() {
    if (!m_be) return;
    m_be->close();
    m_be.reset();
}

void McbController::initializeBackend() { if (m_be) m_be->initialize(); }

void McbController::start(uint32_t sampleCount) {
    if (!m_be) return;
    m_be->start(sampleCount);
    emit runStateChanged(true);
}

void McbController::stop() {
    if (!m_be) return;
    m_be->stop();
    emit runStateChanged(false);
}

void McbController::setTrigger(int mode)        { if (m_be) m_be->setTrigger(mode); }
void McbController::setRangeMode(int range)     { if (m_be) m_be->setRangeMode(range); }
void McbController::setTiming(uint32_t potmt, uint32_t potmw, uint32_t intt) {
    if (m_be) m_be->setTiming(potmt, potmw, intt);
}

DMINFO    McbController::getDmInfo(int nMCB) { return m_be ? m_be->getDmInfo(nMCB) : DMINFO{}; }
uint32_t  McbController::getMcbNum()         { return m_be ? m_be->getMcbNum() : 0; }
bool      McbController::getRunStatus()      { return m_be ? m_be->getRunStatus() : false; }
