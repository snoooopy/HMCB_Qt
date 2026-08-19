#pragma once
#include "IMcbBackend.h"

// 真实硬件后端：直接调用 libmcbctrl 的 SetMCBCommand，并注册静态 trampoline 作为 PFCALLBACK。
// trampoline 在库的工作线程触发，解析 MCB_MSG_STATUS / MCB_MSG_DATA 后回调上层。
// 注意：getter 在库未 initialize 前调用可能不安全，仅在 deviceReady 后使用。
class RealMcbBackend : public IMcbBackend {
public:
    void setStatusCallback(StatusCb cb) override { s_status = cb; }
    void setDataCallback(DataCb cb) override     { s_data = cb; }

    void setConfigFile(const QString& path) override;
    void initialize() override;
    void close() override;
    void start(uint32_t sampleCount) override;
    void stop() override;

    void setTrigger(int mode) override;
    void setRangeMode(int range) override;
    void setTiming(uint32_t potmt, uint32_t potmw, uint32_t intt) override;
    void setSendAllData(bool on) override;
    void setTestMode(bool on) override;

    void setDa(uint32_t da32) override;
    void setDmRange(const QVector<uint8_t>& range65) override;
    void setNewIp(const quint8 addr[12]) override;

    uint32_t getMcbNum() override;
    bool getDeviceReady() override;
    bool getRunStatus() override;
    DMINFO getDmInfo(int nMCB) override;

private:
    static StatusCb s_status;
    static DataCb   s_data;
    // 签名与 PFCALLBACK 一致（静态函数地址可隐式转为 PFCALLBACK）。
    static void trampoline(UINT msg, WPARAM wParam, LPARAM lParam);
};
