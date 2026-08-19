#include "ImageDataWriter.h"
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <QDateTime>
#include <cstring>

namespace ImageDataWriter {
namespace {

// ---- ORI 文件头 PtT_file_head（MCB_TESTView.h:58-117，自然对齐 128B）。
//      Windows DWORD=4B，而库头 typedef 的 DWORD 在 LP64 下是 8B —— 故用显式
//      quint32/quint16 复刻 MSVC x86 布局（自然对齐，maxPixel 落在 48 处 8 对齐）----
struct OriFileHead {
    quint32 tag;
    quint16 version, minVersion;
    quint32 fileLength, headLength, extInfoLength, markInfoLength;
    quint32 dataOffset, extInfoOffset, markInfoOffset;
    quint16 pixelType, pixelBits, pixelBytes, pixelOrder, compression, reserve;
    double maxPixel, minPixel;
    quint32 xLength, yLength, zLength;
    float fPixelSizeX, fPixelSizeY, fPixelSizeZ;
    quint32 nDataInfoLength;
    quint16 dataType, DataInfoLength;
    quint32 filePad[8];
};
static_assert(sizeof(OriFileHead) == 128, "PtT_file_head must be 128 bytes");
static_assert(sizeof(double) == 8 && sizeof(float) == 4, "IEEE754 layout");

constexpr quint32 kTagOri = 0x0069726f;   // PTC_FILE_ORI_SUF "ori\0"
constexpr quint16 kPixelTypeDword = 2;    // PTC_PIXEL_TYPE_DWORD
constexpr int kHeadLen = 128 + 128;     // fhead + fsinfo（Windows memset 后未填字段=全零）
constexpr int kDataInfoLen = 16;

// PgTimage_head 以 writeRawData 原样写盘 → 辅助函数直接产出大端字节（=htons/htonl）
void put16(char* p, quint16 v) { p[0] = char(v >> 8); p[1] = char(v & 0xff); }
void put32(char* p, quint32 v) {
    p[0] = char(v >> 24); p[1] = char((v >> 16) & 0xff);
    p[2] = char((v >> 8) & 0xff); p[3] = char(v & 0xff);
}

// 帧内通道下标（对照 pAd_data+(j*m_nDMChNum+l*nRowNum+k)+i*nChnNum）
inline uint32_t chanOf(const QVector<uint32_t>& f, int adm, int col, int row,
                       int dmCh, int nRow) {
    return f.value(uint32_t(adm * dmCh + col * nRow + row));
}

// -N.ori / -N.img 拆分名（对照 filename0.Delete(len-4,4) + "-%d.ext"）
QString splitName(const QString& path, int nfile) {
    const QString ext = path.mid(path.size() - 4);       // ".ori"/".img"
    return path.left(path.size() - 4) + QStringLiteral("-%1").arg(nfile) + ext;
}

bool writeOriFile(const QString& path, const QVector<QVector<uint32_t>>& frames,
                  const QVector<Mcb::FrameEx>& ex, const Options& opt, int nfile) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);

    const int nDatanum = frames.size();
    const int nChn = frames.isEmpty() ? 0 : frames.first().size();
    const int n = nChn / qMax(1, opt.nColNum);
    const int cols = opt.allColumn ? opt.nColNum : 1;    // yLength 维度

    OriFileHead h{};
    h.tag = kTagOri;
    h.headLength = kHeadLen;
    h.dataOffset = kHeadLen;
    h.pixelType = kPixelTypeDword;
    h.pixelBits = 24;
    h.pixelBytes = 4;
    h.xLength = quint32(n + (opt.oriGM ? 16 : 0));
    h.yLength = quint32(nDatanum * cols);
    h.DataInfoLength = kDataInfoLen;
    h.fileLength = quint32(kHeadLen + (kDataInfoLen + h.xLength * 4) * h.yLength);
    ds.writeRawData(reinterpret_cast<const char*>(&h), sizeof h);
    ds.writeRawData(QByteArray(kHeadLen / 2, char(0)).constData(), kHeadLen / 2);  // fsinfo 全零

    const int dmCh = nChn / qMax(1, opt.nAdm);           // 每 ADM 通道数（m_nDMChNum）
    for (int i = 0; i < nDatanum; ++i) {
        const int lMax = opt.allColumn ? opt.nColNum : 1;
        for (int l = 0; l < lMax; ++l) {
            const int col = opt.allColumn ? l : nfile;   // 非全列：单文件只写本列
            ds.writeRawData(QByteArray(kDataInfoLen, char(0)).constData(), kDataInfoLen);
            for (int j = 0; j < opt.nAdm; ++j)
                for (int k = 0; k < opt.nRowNum; ++k)
                    ds << quint32(chanOf(frames[i], j, col, k, dmCh, opt.nRowNum));
            if (opt.oriGM) {                             // 16 通道数据同 Ain0（对照 ex_data[0][0][i]）
                const quint32 gm = quint32(ex.value(i).ex[0]);
                for (int j = 0; j < 16; ++j) ds << gm;
            }
            if (!opt.allColumn) break;                   // 对照 if(!m_bImgAllColumn) break;
        }
    }
    return true;
}

bool writeImgFile(const QString& path, const QVector<QVector<uint32_t>>& frames,
                  const QVector<Mcb::FrameEx>& ex, const Options& opt, int nfile) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::BigEndian);

    const int nDatanum = frames.size();
    const int nChn = frames.isEmpty() ? 0 : frames.first().size();
    const int n = nChn / qMax(1, opt.nColNum);
    const int cols = opt.allColumn ? opt.nColNum : 1;

    // PgTimage_head：10×WORD + time{DWORD+6×WORD} = 36B，字段逐一 htons/htonl
    char head[36];
    std::memset(head, 0, sizeof head);
    put16(head + 0, quint16(100));                       // version = PgCSCAN_IMG_VERSION
    put16(head + 14, quint16(opt.pixel16 ? 16 : 32));    // pixelType（10×WORD 第 8 个）
    const QDateTime now = QDateTime::currentDateTime();
    put32(head + 20, quint32(QDateTime::currentSecsSinceEpoch()));
    put16(head + 24, quint16(now.date().year()));
    put16(head + 26, quint16(now.date().month()));
    put16(head + 28, quint16(now.date().day()));
    put16(head + 30, quint16(now.time().hour()));
    put16(head + 32, quint16(now.time().minute()));
    put16(head + 34, quint16(now.time().second()));
    put16(head + 2, quint16(nDatanum * cols));           // xLength
    put16(head + 4, quint16(n + (opt.oriGM ? 16 : 0) + (opt.saveExdata ? 4 : 0)));   // yLength
    ds.writeRawData(head, sizeof head);

    const int dmCh = nChn / qMax(1, opt.nAdm);
    const int shiftDown = (opt.adBit <= 16) ? 0 : opt.adBit - 16;
    const int shiftUp = 32 - opt.adBit;
    const auto exdataVal = [&](int row, int j) -> quint32 {
        if (row == 0) return quint32(ex.value(j).accE);  // 高低能指示
        if (row == 1) return quint32(ex.value(j).godown);// 钩铛信号
        if (row == 2) return quint32(ex.value(j).ex[0]); // Ain0
        return 0;
    };

    // bSaveExdata：4 行 × 帧 × 列（accE/godown 原值，Ain 移位，末行 0）
    if (opt.saveExdata) {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < nDatanum; ++j)
                for (int l = 0; l < cols; ++l) {
                    if (opt.pixel16) {
                        quint16 d = quint16(exdataVal(i, j) >> ((i == 2) ? shiftDown : 0));
                        ds << d;
                    } else {
                        quint32 v = exdataVal(i, j);
                        if (i == 2) v <<= shiftUp;       // quint32 钳位=Windows 32 位 unsigned long
                        ds << v;
                    }
                }
    }
    // 探测器数据：每 ADM×行 一"行" × 帧 × 列
    for (int i = 0; i < opt.nAdm; ++i)
        for (int k = 0; k < opt.nRowNum; ++k)
            for (int j = 0; j < nDatanum; ++j)
                for (int l = 0; l < cols; ++l) {
                    const int col = opt.allColumn ? l : nfile;
                    const quint32 v = chanOf(frames[j], i, col, k, dmCh, opt.nRowNum);
                    if (opt.pixel16) ds << quint16(v >> shiftDown);
                    else            ds << quint32(quint32(v) << shiftUp);
                }
    // bOriGM：16 行 Ain0（16bit 移位除 bFND 外；32bit 原值）
    if (opt.oriGM) {
        const int gmShift = (opt.adBit <= 16 || opt.fnd) ? 0 : opt.adBit - 16;
        for (int i = 0; i < 16; ++i)
            for (int j = 0; j < nDatanum; ++j)
                for (int l = 0; l < cols; ++l) {
                    const quint32 v = quint32(ex.value(j).ex[0]);
                    if (opt.pixel16) ds << quint16(v >> gmShift);
                    else            ds << v;
                }
    }
    return true;
}

bool writeCsvFile(const QString& path, const QVector<QVector<uint32_t>>& frames,
                  const QVector<Mcb::FrameEx>& ex, const Options& opt) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream ts(&file);
    const int nDatanum = frames.size();
    const int dmCh = frames.isEmpty() ? 0 : frames.first().size() / qMax(1, opt.nAdm);

    ts << "SN";
    for (int j = 0; j < opt.nAdm; ++j)
        for (int l = 0; l < opt.nColNum; ++l)
            for (int k = 0; k < opt.nRowNum; ++k)
                ts << QStringLiteral(",M%1CH%2-%3").arg(j).arg(l).arg(k, 2, 10, QLatin1Char('0'));
    for (int i = 0; i < 5; ++i) ts << QStringLiteral(",EXCH%1").arg(i);
    ts << "\n";
    for (int i = 0; i < nDatanum; ++i) {
        ts << i;
        for (int j = 0; j < opt.nAdm; ++j)
            for (int l = 0; l < opt.nColNum; ++l)
                for (int k = 0; k < opt.nRowNum; ++k)
                    ts << ',' << chanOf(frames[i], j, l, k, dmCh, opt.nRowNum);
        for (int j = 0; j < 5; ++j) ts << ',' << ex.value(i).ex[j];
        ts << "\n";
    }
    return true;
}

void writeSidecars(const QString& path, const QVector<Mcb::FrameEx>& ex, int nDatanum) {
    // .acce / .Ain0 逐帧文本；.godown.csv 变化点（dlast 初 0xff，GODOWNNUM=65536*8）
    {
        QFile f(path + QStringLiteral(".acce"));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            for (int i = 0; i < nDatanum; ++i) ts << ex.value(i).accE << "\n";
        }
    }
    {
        QFile f(path + QStringLiteral(".Ain0"));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            for (int i = 0; i < nDatanum; ++i) ts << ex.value(i).ex[0] << "\n";
        }
    }
    {
        QFile f(path + QStringLiteral(".godown.csv"));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            int dlast = 0xff;
            const int n = qMin(nDatanum, 65536 * 8);
            for (int i = 0; i < n; ++i)
                if (dlast != ex.value(i).godown) {
                    ts << i << ',' << ex.value(i).godown << "\n";
                    dlast = ex.value(i).godown;
                }
        }
    }
}

} // namespace

bool save(const QString& path, Format fmt, const QVector<QVector<uint32_t>>& frames,
          const QVector<Mcb::FrameEx>& ex, const Options& opt, QString* err) {
    if (frames.isEmpty()) { if (err) *err = QStringLiteral("尚无采集数据"); return false; }
    const int nFiles = (fmt != Format::Csv && !opt.allColumn) ? opt.nColNum : 1;
    bool ok = true;
    for (int nfile = 0; nfile < nFiles && ok; ++nfile) {
        const QString p = nFiles > 1 ? splitName(path, nfile) : path;
        if (fmt == Format::Ori)          ok = writeOriFile(p, frames, ex, opt, nfile);
        else if (fmt == Format::Img)     ok = writeImgFile(p, frames, ex, opt, nfile);
        else                             ok = writeCsvFile(p, frames, ex, opt);
        if (!ok && err) *err = QStringLiteral("Open File Error: %1").arg(p);
    }
    if (ok && opt.saveExdata)
        writeSidecars(path, ex, frames.size());
    return ok;
}

} // namespace ImageDataWriter
