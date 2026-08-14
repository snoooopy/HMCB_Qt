#include "MockMcbBackend.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <algorithm>

MockMcbBackend::MockMcbBackend() {
    m_initTimer.setSingleShot(false);
    QObject::connect(&m_initTimer, &QTimer::timeout, [this]() {
        static const struct { WPARAM cat; uint32_t code; } seq[] = {
            {MCB_DVSTS, 0x0101},  // CONNECTOK
            {MCB_DVSTS, 0x0200},  // MBFPGACFG_OK
            {MCB_DVSTS, 0x0201},  // ADFPGACFG_OK
            {MCB_DVSTS, 0x0204}   // REGSET_OK
        };
        if (m_initStep < 4) {
            emitStatus(seq[m_initStep].cat, 0, seq[m_initStep].code);
        } else {
            emitStatus(MCB_DVREADY, 0, 0);
            m_ready = true;
            m_initTimer.stop();
        }
        ++m_initStep;
    });
}

MockMcbBackend::~MockMcbBackend() { close(); }

void MockMcbBackend::initialize() {
    m_initStep = 0;
    m_ready = false;
    m_initTimer.start(150);   // 每 150ms 推进一步状态
}

void MockMcbBackend::close() {
    m_initTimer.stop();
    stop();
    m_ready = false;
}

DMINFO MockMcbBackend::getDmInfo(int nMCB) {
    DMINFO d{};
    d.nMCB = nMCB;
    d.nDMNum = m_dmNum;
    d.nRowNum = m_rowNum;
    d.nColNum = m_colNum;
    d.nAllChnNum = m_dmNum * m_rowNum * m_colNum;
    return d;
}

void MockMcbBackend::start(uint32_t) {
    if (m_running) return;
    if (m_dataThread.joinable()) m_dataThread.join();
    m_running = true;
    m_dataThread = std::thread([this] { dataLoop(); });
    emitStatus(MCB_DVSTS, 0, 0x0300);  // STARTOK
}

void MockMcbBackend::stop() {
    m_running = false;
    if (m_dataThread.joinable()) m_dataThread.join();
    emitStatus(MCB_STOPOK, 0, 0x0300);
}

void MockMcbBackend::dataLoop() {
    const int n = m_dmNum * m_rowNum * m_colNum;
    while (m_running) {
        Mcb::McbFrame f;
        f.nMCB = 0;
        f.pcounter = ++m_pcounter;
        f.scounter = f.pcounter;
        f.channels.resize(n);
        const double phase = m_pcounter.load() * 0.1;
        for (int i = 0; i < n; i++) {
            double v = 2048.0
                     + 1500.0 * std::sin(i * 0.05 + phase)
                     + (std::rand() % 200 - 100);
            f.channels[i] = static_cast<uint32_t>(std::clamp(v, 0.0, 4095.0));
        }
        if (m_data) m_data(f);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
