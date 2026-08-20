#pragma once
#include "IMcbBackend.h"
#include <atomic>

// 真实硬件后端：直接调用 libmcbctrl 的 SetMCBCommand，并注册静态 trampoline 作为 PFCALLBACK。
// trampoline 在库的工作线程触发，解析 MCB_MSG_STATUS / MCB_MSG_DATA 后回调上层。
// 注意：getter 在库未 initialize 前调用可能不安全，仅在 deviceReady 后使用。
class RealMcbBackend : public IMcbBackend {
public:
    void setStatusCallback(StatusCb cb) override { s_status = cb; }
    void setDataCallback(DataCb cb) override     { s_data = cb; }

    // 帧通道数注入（DVREADY 后由 GETDMINFO 的 nAllChnNum 告知）：trampoline 只拷
    // 实际通道数。MCBData.addata 是 64*128*8=65536 的固定上限数组且无长度字段，
    // 全量拷贝在 250fps 实机下是 64MB/s 拷贝洪水（2026-08-20 真机卡顿根因）。
    void setChannelCount(int n) override;

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
    void setDm2TypeRange(const QVector<uint8_t>& range65) override;
    void setNewIp(const quint8 addr[12]) override;
    void dumpEepromKeyData() override;
    void dumpAllEcsRegisters() override;
    void setPotmt(uint32_t potmt) override;
    void setIntt(uint32_t intt) override;
    void setFndmPara(const QVector<FndmParaBuilder::Ch>& para) override;
    void setFndmDeMode(bool on) override;
    void setFndmNsType(int ns) override;
    QVector<FndmParaBuilder::Ch> getFndmPara() override;

    uint32_t getMcbNum() override;
    bool getDeviceReady() override;
    bool getRunStatus() override;
    DMINFO getDmInfo(int nMCB) override;

private:
    static StatusCb s_status;
    static DataCb   s_data;
    // 0=未注入（按结构上限全量拷贝）；库数据线程读 vs Controller 线程写，原子。
    static std::atomic<int> s_chn;

public:
    // 签名与 PFCALLBACK 一致（静态函数地址可隐式转为 PFCALLBACK）。
    // public 供 test_liblink 直接构造 MCBData 验证通道拷贝数。
    static void trampoline(UINT msg, WPARAM wParam, LPARAM lParam);
};
