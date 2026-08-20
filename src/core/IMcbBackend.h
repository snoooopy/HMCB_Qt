#pragma once
#include <functional>
#include <cstdint>
#include <QString>
#include <QVector>
#include "core/McbTypes.h"
#include "core/FndmParaBuilder.h"
#include "MCBCtrl.h"   // DMINFO, WPARAM

// 后端抽象：真实硬件（RealMcbBackend，封装 libmcbctrl）与仿真（MockMcbBackend）共用此接口。
// 回调可能来自非 GUI 线程，由 McbController 负责跨线程转发。
class IMcbBackend {
public:
    virtual ~IMcbBackend() = default;

    using StatusCb = std::function<void(WPARAM category, int nMCB, uint32_t code)>;
    using DataCb   = std::function<void(const Mcb::McbFrame&)>;

    virtual void setStatusCallback(StatusCb cb) = 0;
    virtual void setDataCallback(DataCb cb) = 0;

    // 生命周期
    virtual void setConfigFile(const QString& path) = 0;
    virtual void initialize() = 0;            // MCB_CMD_INITIAL
    virtual void close() = 0;                 // MCB_CMD_CLOSE
    virtual void start(uint32_t sampleCount) = 0;  // MCB_CMD_START（0=连续）
    virtual void stop() = 0;                  // MCB_CMD_STOP

    // 控制
    virtual void setTrigger(int mode) = 0;    // 0内 / 1外 / 2级联
    virtual void setRangeMode(int range) = 0;
    virtual void setTiming(uint32_t potmt, uint32_t potmw, uint32_t intt) = 0;
    virtual void setSendAllData(bool on) = 0;
    virtual void setTestMode(bool on) = 0;

    // Phase 3：INL DA 输出 / 每 DM 增益档
    virtual void setDa(uint32_t da32) = 0;                     // MCB_CMD_AOUTDASET：param=(基线<<16)|电平
    virtual void setDmRange(const QVector<uint8_t>& range65) = 0; // MCB_CMD_SETRANGE：[0]=板号,[1+j]=DM j 档

    // Phase 4：双类型档（MCB_CMD_SET2TYPERANGE）：[0]=板号,
    // [1+j]=类型0档、[1+nDMNum+j]=类型1档（各 nDMNum 个）
    virtual void setDm2TypeRange(const QVector<uint8_t>& range65) = 0;

    // Phase 4：设新 IP（MCB_CMD_SETNEWIP=0x0601）：12 字节 = IP4+子网掩码4+网关4
    virtual void setNewIp(const quint8 addr[12]) = 0;

    // Phase 4：Tools 转储（DUMPEEPROMKEYDATA/DUMPALLECSREG，均 LPARAM=1）
    virtual void dumpEepromKeyData() = 0;
    virtual void dumpAllEcsRegisters() = 0;

    // Phase 4：MCB Control 计时设置（Windows 固定发 0）
    virtual void setPotmt(uint32_t potmt) = 0;    // MCB_CMD_SETPOTMT
    virtual void setIntt(uint32_t intt) = 0;      // MCB_CMD_SETINTTM

    // Phase 4：FNDM（快中子模块）。SETFNDMPARA 组包见 FndmParaBuilder；
    // SETDEMODE LPARAM=BOOL、SETNSTYPE LPARAM=0/1（Pulsed/Continued）。
    // Linux 库 MCBThread 三 case 实现均被注释（静默忽略），照 Windows 契约转发
    virtual void setFndmPara(const QVector<FndmParaBuilder::Ch>& para) = 0;
    virtual void setFndmDeMode(bool on) = 0;      // MCB_CMD_SETDEMODE
    virtual void setFndmNsType(int ns) = 0;       // MCB_CMD_SETNSTYPE
    // MCB_CMD_GETFNDMPARA 回读。Linux 库该 case 被注释（不回填）→ 返回空
    virtual QVector<FndmParaBuilder::Ch> getFndmPara() = 0;

    // 同步查询（短暂阻塞，经返回值给出）
    virtual uint32_t getMcbNum() = 0;
    virtual bool getDeviceReady() = 0;
    virtual bool getRunStatus() = 0;
    virtual DMINFO getDmInfo(int nMCB) = 0;
    // 实际 AD 位数（MCB_CMD_GETDMADBIT）：mV 换算系数由它决定（缺省 20bit）
    virtual int getDmAdBit() { return 20; }

    // 帧通道数注入（真实后端用：DVREADY 后按 GETDMINFO.nAllChnNum 只拷实际通道，
    // 缺省全量。Mock 自产帧无需实现）
    virtual void setChannelCount(int n) { Q_UNUSED(n); }
};
