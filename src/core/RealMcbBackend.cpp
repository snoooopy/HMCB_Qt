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
std::atomic<int> RealMcbBackend::s_chn{0};

void RealMcbBackend::setChannelCount(int n) {
    if (n > 0) s_chn.store(n);
}

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
        constexpr int kMaxCh = MAX_MCBADNUM * MAX_DMROWNUM * MAX_DMCOLNUM;
        // 实际通道数（DVREADY 注入；0=未注入时保持全量上限行为）。
        const int n = s_chn.load(std::memory_order_relaxed);
        const int N = (n > 0) ? qMin(n, kMaxCh) : kMaxCh;
        f.channels.resize(N);
        for (int i = 0; i < N; i++) f.channels[i] = d->addata[i].ul;
        // 注：库输出已按 d=col*rowNum+dm*128+row 重排（对照 MCB_TESTView 数据回调），
        // 直拷前 N 个即为有效通道序。
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

void RealMcbBackend::setFndmPara(const QVector<FndmParaBuilder::Ch>& para) {
    // 对照 Windows SetMCBCommand(MCB_CMD_SETFNDMPARA, &FNDMPARA)：
    // LPARAM 指向 nChannels + ChannelPara[]。结构 ~229KB 用 static（栈放不下，
    // 且库 default 分支起线程转发时指针须存活）。Linux 库该 case 实现被注释
    // （MCBCtrl.cpp SETFNDMPARA 分支 / MCBThread.cpp:149 均为空），按契约转发。
    static FNDMPARA p;                      // UI 线程单调用点，无重入
    memset(&p, 0, sizeof(p));
    const int n = qMin<int>(para.size(), int(sizeof(p.ChannelPara) / sizeof(p.ChannelPara[0])));
    p.nChannels = static_cast<UINT>(n);
    for (int i = 0; i < n; ++i) {
        p.ChannelPara[i].nFNDM    = para[i].fndm;
        p.ChannelPara[i].nType    = para[i].type;
        p.ChannelPara[i].nRow     = para[i].row;
        p.ChannelPara[i].nColnum  = para[i].colnum;
        p.ChannelPara[i].nAdjMode = para[i].adjMode;
        p.ChannelPara[i].nValue   = para[i].value;
    }
    SetMCBCommand(MCB_CMD_SETFNDMPARA, reinterpret_cast<LPARAM>(&p));
}
void RealMcbBackend::setFndmDeMode(bool on) {
    SetMCBCommand(MCB_CMD_SETDEMODE, static_cast<LPARAM>(on ? 1 : 0));
}
void RealMcbBackend::setFndmNsType(int ns) {
    SetMCBCommand(MCB_CMD_SETNSTYPE, static_cast<LPARAM>(ns));
}
QVector<FndmParaBuilder::Ch> RealMcbBackend::getFndmPara() {
    // 对照 UpdateCurrentFndmPara：GETFNDMPARA LPARAM 指向接收结构。
    // Linux 库该 case 被注释（不回填），返回空 QVector；库实现恢复后此
    // 处即可拿到回读值。
    static FNDMPARA p;
    memset(&p, 0, sizeof(p));
    SetMCBCommand(MCB_CMD_GETFNDMPARA, reinterpret_cast<LPARAM>(&p));
    QVector<FndmParaBuilder::Ch> v;
    const int n = qMin<UINT>(p.nChannels, UINT(sizeof(p.ChannelPara) / sizeof(p.ChannelPara[0])));
    for (int i = 0; i < int(n); ++i)
        v.append(FndmParaBuilder::Ch(p.ChannelPara[i].nFNDM, p.ChannelPara[i].nType,
                                     p.ChannelPara[i].nRow, p.ChannelPara[i].nColnum,
                                     p.ChannelPara[i].nAdjMode, p.ChannelPara[i].nValue));
    return v;
}

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
