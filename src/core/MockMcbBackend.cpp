#include "MockMcbBackend.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <algorithm>

MockMcbBackend::MockMcbBackend() {
    m_dmAmp.resize(64);                   // (0.95 + (dm%5)*0.45)·2^20 → 0 档普遍超 0.9 阈值，
    for (int dm = 0; dm < 64; ++dm)       // 升 1~2 档收敛，可演示多步增益校准
        m_dmAmp[dm] = (0.95 + (dm % 5) * 0.45) * 1048576.0;

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

void MockMcbBackend::start(uint32_t sampleCount) {
    if (m_running) return;
    if (m_dataThread.joinable()) m_dataThread.join();
    m_running = true;
    m_sampleLimit = sampleCount;
    m_produced = 0;
    m_dataThread = std::thread([this] { dataLoop(); });
    emitStatus(MCB_DVSTS, 0, 0x0300);  // STARTOK
}

void MockMcbBackend::stop() {
    m_running = false;
    if (m_dataThread.joinable()) m_dataThread.join();
    emitStatus(MCB_STOPOK, 0, 0);   // 命令停：code=0（Windows lParam 低 16 位语义）
}

void MockMcbBackend::dataLoop() {
    const int n = m_dmNum * m_rowNum * m_colNum;
    const int chPerDm = m_rowNum * m_colNum;
    while (m_running) {
        if (m_sampleLimit > 0 && m_produced >= m_sampleLimit) break;   // 采满自停
        Mcb::McbFrame f;
        f.nMCB = 0;
        f.pcounter = ++m_pcounter;
        f.scounter = f.pcounter;
        f.channels.resize(n);
        const double phase = m_pcounter.load() * 0.1;
        // DA 模型（INL 仿真）：摆幅→x(mV)，y=40+0.105x+40(x/5000)³，通道斜率 ±1.8% 固定偏差
        const double swing16 = double(int32_t(m_da >> 16) - int32_t(m_da & 0xFFFF));
        const double xMv = swing16 * 2500.0 / 65536.0;
        for (int i = 0; i < n; i++) {
            double v;
            if (m_rangeSet) {
                // 增益模型：每 DM 幅度 A>>档位，饱和 2^20（演示多步收敛）
                const int dm = i / chPerDm;
                const double amp = m_dmAmp.value(dm, 1.0e6);
                v = std::min(amp / double(1u << m_dmGainRange.value(dm, 0)), 1048575.0);
            } else {
                const double jitter = 1.0 + ((i % 7) - 3) * 0.006;
                const double yMv = (40.0 + 0.105 * xMv + 40.0 * std::pow(xMv / 5000.0, 3)) * jitter;
                v = yMv * 3355.4432;          // INL 除回 3355.4432 还原设计 mV
            }
            v += 300.0 * std::sin(i * 0.05 + phase) + (std::rand() % 400 - 200);
            f.channels[i] = static_cast<uint32_t>(std::clamp(v, 0.0, 1048575.0));
        }
        if (m_data) m_data(f);
        ++m_produced;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    m_running = false;
    if (m_sampleLimit > 0) emitStatus(MCB_STOPOK, 0, 1);   // 自然停：code=1
}
