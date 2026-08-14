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

    int m_range = 7;
    int m_dmNum = 25, m_rowNum = 32, m_colNum = 4;  // 默认几何，可由配置覆盖

    void emitStatus(WPARAM cat, int nMCB, uint32_t code) {
        if (m_status) m_status(cat, nMCB, code);
    }
    void dataLoop();
};
