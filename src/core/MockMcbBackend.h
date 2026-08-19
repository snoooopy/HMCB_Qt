#pragma once
#include "IMcbBackend.h"
#include <QTimer>
#include <atomic>
#include <thread>
#include <cstdint>

// 仿真后端：initialize() 用 QTimer 依序推送状态码直至 DVREADY；
// start() 起数据线程按 ~10Hz 产生合成 McbFrame（正弦+噪声）。
// 回调在工作线程触发（与真实库一致），由 McbController 跨线程转发。
class MockMcbBackend : public IMcbBackend {
public:
    MockMcbBackend();
    ~MockMcbBackend() override;

    void setStatusCallback(StatusCb cb) override { m_status = cb; }
    void setDataCallback(DataCb cb) override     { m_data = cb; }
    void setConfigFile(const QString&) override {}
    void initialize() override;
    void close() override;
    void start(uint32_t sampleCount) override;
    void stop() override;

    void setTrigger(int) override {}
    void setRangeMode(int r) override { m_range = r; }
    void setTiming(uint32_t, uint32_t, uint32_t) override {}
    void setSendAllData(bool) override {}
    void setTestMode(bool) override {}

    void setDa(uint32_t da32) override { m_da = da32; m_rangeSet = false; }   // DA 模型接管
    void setDmRange(const QVector<uint8_t>& range65) override {
        if (range65.size() < 65) return;
        m_dmGainRange = range65.mid(1, 64);
        m_rangeSet = true;                                                   // 增益模型接管
    }

    void setDm2TypeRange(const QVector<uint8_t>& range65) override {
        if (range65.size() < 1 + 2 * m_dmNum) return;
        m_dm2TypeRange = range65.mid(1, 2 * m_dmNum);
    }
    QVector<uint8_t> dm2TypeRange() const { return m_dm2TypeRange; }   // 测试断言用

    void setNewIp(const quint8 addr[12]) override {
        m_lastNewIp.clear();
        for (int i = 0; i < 12; ++i) m_lastNewIp.append(addr[i]);
    }
    QVector<uint8_t> lastNewIp() const { return m_lastNewIp; }   // 测试断言用

    void dumpEepromKeyData() override      { m_dumpEeprom = true; }
    void dumpAllEcsRegisters() override    { m_dumpEcs = true; }
    bool dumpEepromRequested() const { return m_dumpEeprom; }   // 测试断言用
    bool dumpEcsRequested() const   { return m_dumpEcs; }

    void setPotmt(uint32_t v) override { m_potmt = v; }
    void setIntt(uint32_t v) override { m_intt = v; }
    uint32_t lastPotmt() const { return m_potmt; }
    uint32_t lastIntt() const { return m_intt; }

    uint32_t getMcbNum() override { return 1; }
    bool getDeviceReady() override { return m_ready; }
    bool getRunStatus() override { return m_running; }
    DMINFO getDmInfo(int nMCB) override;

private:
    StatusCb m_status;
    DataCb   m_data;
    QTimer   m_initTimer;
    int      m_initStep = 0;
    std::atomic_bool m_running{false};
    std::atomic_bool m_ready{false};
    std::atomic_uint m_pcounter{0};
    std::thread m_dataThread;
    uint32_t m_sampleLimit = 0;          // 0=连续；>0=有限采集帧数
    std::atomic_uint m_produced{0};      // 本轮已产出帧数

    int m_range = 7;
    int m_dmNum = 25, m_rowNum = 32, m_colNum = 4;  // 默认几何，可由配置覆盖

    uint32_t m_da = 0;                    // 最近 AOUTDASET 32bit 码
    QVector<uint8_t> m_dmGainRange;       // 每 DM 档位（0..7）
    QVector<uint8_t> m_dm2TypeRange;      // 双类型档（2×nDMNum）
    QVector<double> m_dmAmp;              // 每 DM 基础幅度（20bit 域）
    bool m_rangeSet = false;              // 最后设置的控制生效：setDa 清 / setDmRange 置
    QVector<uint8_t> m_lastNewIp;         // 最近一次 setNewIp 的 12 字节
    bool m_dumpEeprom = false, m_dumpEcs = false;
    uint32_t m_potmt = 0, m_intt = 0;

    void emitStatus(WPARAM cat, int nMCB, uint32_t code) {
        if (m_status) m_status(cat, nMCB, code);
    }
    void dataLoop();
};
