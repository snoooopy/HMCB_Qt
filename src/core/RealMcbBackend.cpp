#include "RealMcbBackend.h"
#include "MCBCtrl.h"
#include "MCB_param.h"
#include <cstring>

#ifndef MCB_CMD_TESTMODE
#define MCB_CMD_TESTMODE 0x8200
#endif
#ifndef MCB_CMD_AOUTDASET
#define MCB_CMD_AOUTDASET 0x8100
#endif
#ifndef MCB_CMD_SETRANGE
#define MCB_CMD_SETRANGE 0x0204
#endif

RealMcbBackend::StatusCb RealMcbBackend::s_status;
RealMcbBackend::DataCb   RealMcbBackend::s_data;

// 库工作线程触发：解码消息，转发为类型化回调。
void RealMcbBackend::trampoline(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == MCB_MSG_STATUS) {
        auto p = Mcb::decodeStatusParam(lParam);
        if (s_status) s_status(wParam, p.nMCB, p.code);
    } else if (msg == MCB_MSG_DATA) {
        const MCBData* d = reinterpret_cast<const MCBData*>(wParam);
        Mcb::McbFrame f;
        f.nMCB = static_cast<int>(lParam);
        f.scounter      = d->d_head.head_use.scounter;
        f.pcounter      = d->d_head.head_use.pcounter.l;
        f.accEnergy     = d->d_head.head_use.accEnergy;
        f.ain0          = d->d_head.head_use.ain0.ul;
        f.ain1          = d->d_head.head_use.ain1.ul;
        f.regError_DI   = d->d_head.head_use.regError_DI;
        f.ptsmL         = d->d_head.head_use.ptsmdata_l;   // POS: AngleCode
        f.ptsmH         = d->d_head.head_use.ptsmdata_h;   // POS: ConveyorCode
        const int N = MAX_MCBADNUM * MAX_DMROWNUM * MAX_DMCOLNUM;
        f.channels.resize(N);
        for (int i = 0; i < N; i++) f.channels[i] = d->addata[i].ul;
        // 注：样本重排顺序对照 MCB_TESTView.cpp 回调（按 column/DM/row），
        // 此处先直拷；硬件联调阶段据实校正索引。
        if (s_data) s_data(f);
    }
}

void RealMcbBackend::setConfigFile(const QString& path) {
    SetMCBCommand(MCB_CMD_SETCONFIGFILE, reinterpret_cast<LPARAM>(path.toUtf8().constData()));
}

void RealMcbBackend::initialize() {
    SetMCBCommand(MCB_CMD_INITIAL, reinterpret_cast<LPARAM>(&RealMcbBackend::trampoline));
}

void RealMcbBackend::close()            { SetMCBCommand(MCB_CMD_CLOSE, 0); }
void RealMcbBackend::start(uint32_t n)  { SetMCBCommand(MCB_CMD_START, static_cast<LPARAM>(n)); }
void RealMcbBackend::stop()             { SetMCBCommand(MCB_CMD_STOP, 0); }
void RealMcbBackend::setTrigger(int m)  { SetMCBCommand(MCB_CMD_TRIGSET, static_cast<LPARAM>(m)); }
void RealMcbBackend::setRangeMode(int r){ SetMCBCommand(MCB_CMD_SETRANGEMODE, static_cast<LPARAM>(r)); }

void RealMcbBackend::setTiming(uint32_t potmt, uint32_t potmw, uint32_t intt) {
    SetMCBCommand(MCB_CMD_SETPOTMT, static_cast<LPARAM>(potmt));
    SetMCBCommand(MCB_CMD_SETPOTMW, static_cast<LPARAM>(potmw));
    SetMCBCommand(MCB_CMD_SETINTTM, static_cast<LPARAM>(intt));
}

void RealMcbBackend::setSendAllData(bool on) { SetMCBCommand(MCB_CMD_SETALLDATA, on ? 1 : 0); }
void RealMcbBackend::setTestMode(bool on)    { SetMCBCommand(MCB_CMD_TESTMODE, on ? 1 : 0); }

void RealMcbBackend::setDa(uint32_t da32) {
    SetMCBCommand(MCB_CMD_AOUTDASET, static_cast<LPARAM>(da32));
}
void RealMcbBackend::setDmRange(const QVector<uint8_t>& range65) {
    if (range65.size() < 65) return;   // 须停止态发送，否则库报 0x1110
    SetMCBCommand(MCB_CMD_SETRANGE, reinterpret_cast<LPARAM>(range65.constData()));
}
void RealMcbBackend::setDm2TypeRange(const QVector<uint8_t>& range65) {
    // 对照 Windows OnSetdm2range：[板号][类型0×nDMNum][类型1×nDMNum]
    if (range65.size() < 1) return;
    SetMCBCommand(MCB_CMD_SET2TYPERANGE, reinterpret_cast<LPARAM>(range65.constData()));
}
void RealMcbBackend::setNewIp(const quint8 addr[12]) {
    // 对照 Windows DlgSetNewIPAddress：LPARAM 指向 IP4+掩码4+网关4；
    // 结果经 MCB_MSG_STATUS 回 0x1121 RUN / 0x1122 ERR / 0x1123 OK
    SetMCBCommand(MCB_CMD_SETNEWIP, reinterpret_cast<LPARAM>(addr));
}
// Linux 库头（libMcbctrl 20260421）缺 DUMPEEPROMKEYDATA 定义（Windows 版库=0x060E；
// 当前 Linux 库的命令 switch 也无此 case，会静默忽略）——按 Windows 契约补发。
#ifndef MCB_CMD_DUMPEEPROMKEYDATA
#define MCB_CMD_DUMPEEPROMKEYDATA (0x060E)
#endif
void RealMcbBackend::dumpEepromKeyData()   { SetMCBCommand(MCB_CMD_DUMPEEPROMKEYDATA, 1); }
void RealMcbBackend::dumpAllEcsRegisters() { SetMCBCommand(MCB_CMD_DUMPALLECSREG, 1); }
void RealMcbBackend::setPotmt(uint32_t v)  { SetMCBCommand(MCB_CMD_SETPOTMT, static_cast<LPARAM>(v)); }
void RealMcbBackend::setIntt(uint32_t v)   { SetMCBCommand(MCB_CMD_SETINTTM, static_cast<LPARAM>(v)); }

uint32_t RealMcbBackend::getMcbNum() {
    UINT n = 0;
    SetMCBCommand(MCB_CMD_GETMCBNUM, reinterpret_cast<LPARAM>(&n));
    return n;
}
bool RealMcbBackend::getDeviceReady() {
    UINT n = 0;
    SetMCBCommand(MCB_CMD_GETDVREADY, reinterpret_cast<LPARAM>(&n));
    return n != 0;
}
bool RealMcbBackend::getRunStatus() {
    UINT n = 0;
    SetMCBCommand(MCB_CMD_GETRUNSTATUS, reinterpret_cast<LPARAM>(&n));
    return n != 0;
}
DMINFO RealMcbBackend::getDmInfo(int nMCB) {
    DMINFO d{};
    d.nMCB = nMCB;
    SetMCBCommand(MCB_CMD_GETDMINFO, reinterpret_cast<LPARAM>(&d));
    return d;
}
