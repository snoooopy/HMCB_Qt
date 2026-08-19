#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/ImageDataWriter.h"

using ImageDataWriter::Format;
using ImageDataWriter::Options;

// Save Image Data 三格式写盘（对照 Windows OnSavedata，MCB_TESTView.cpp:4134-4595）：
// ORI = PtT_file_head(128B)+PTT_det_saveinfo(128B)+小端 long 数据（每帧列前 16B datainfo）；
// IMG = 36B 大端头 + 大端像素（16bit 右移对齐 / 32bit 左对齐）+ 扩展行；
// CSV = SN,M{j}CH{l}-{k:02d},...,EXCH0..4；附加 .acce/.Ain0/.godown.csv。
class TestImageData : public QObject {
    Q_OBJECT

private:
    // 合成帧：v(i,ch) = base + 100*i + ch
    static QVector<QVector<uint32_t>> makeFrames(int nFrames, int nChn, int base = 1000) {
        QVector<QVector<uint32_t>> fs;
        for (int i = 0; i < nFrames; ++i) {
            QVector<uint32_t> f(nChn);
            for (int ch = 0; ch < nChn; ++ch) f[ch] = uint32_t(base + 100 * i + ch);
            fs.append(f);
        }
        return fs;
    }
    static Mcb::FrameEx makeEx(int ain0, int accE, int godown) {
        Mcb::FrameEx e;
        e.ex[0] = ain0; e.ex[1] = 2; e.ex[2] = 3; e.ex[3] = 4; e.ex[4] = 5;
        e.accE = accE; e.godown = godown;
        return e;
    }
    static QByteArray readFile(const QString& path) {
        QFile f(path);
        f.open(QIODevice::ReadOnly);
        return f.readAll();
    }
    static quint32 le32(const QByteArray& b, int off) {
        return quint32(quint8(b[off])) | quint32(quint8(b[off + 1])) << 8
             | quint32(quint8(b[off + 2])) << 16 | quint32(quint8(b[off + 3])) << 24;
    }
    static quint16 le16(const QByteArray& b, int off) {
        return quint16(quint8(b[off])) | quint16(quint8(b[off + 1])) << 8;
    }
    static quint16 be16(const QByteArray& b, int off) {
        return quint16(quint8(b[off])) << 8 | quint16(quint8(b[off + 1]));
    }
    static quint32 be32(const QByteArray& b, int off) {
        return quint32(quint8(b[off])) << 24 | quint32(quint8(b[off + 1])) << 16
             | quint32(quint8(b[off + 2])) << 8 | quint32(quint8(b[off + 3]));
    }

private slots:
    // ORI：头字段 + 全列数据布局（ADM×行 按列取，每帧列前 16B datainfo）
    void oriHeaderAndLayout() {
        QTemporaryDir dir;
        const QString path = dir.filePath("t.ori");
        Options opt; opt.nAdm = 2; opt.nRowNum = 8; opt.nColNum = 2;   // nChn=32, n=16
        const auto frames = makeFrames(2, 32);
        QVERIFY(ImageDataWriter::save(path, Format::Ori, frames, {}, opt));

        const QByteArray b = readFile(path);
        // fileLength = 256 + (16 + xLength*4)*yLength = 256 + 80*4
        QCOMPARE(b.size(), 576);
        QCOMPARE(le32(b, 0), quint32(0x0069726f));   // tag "ori\0"
        QCOMPARE(le32(b, 8), quint32(576));          // fileLength
        QCOMPARE(le32(b, 12), quint32(256));         // headLength
        QCOMPARE(le32(b, 24), quint32(256));         // dataOffset
        QCOMPARE(le16(b, 36), quint16(2));           // pixelType DWORD
        QCOMPARE(le16(b, 38), quint16(24));          // pixelBits
        QCOMPARE(le16(b, 40), quint16(4));           // pixelBytes
        QCOMPARE(le32(b, 64), quint32(16));          // xLength = n
        QCOMPARE(le32(b, 68), quint32(4));           // yLength = nDatanum*nColNum
        QCOMPARE(le32(b, 94), quint32(16));          // DataInfoLength
        // 数据区 256：帧 i × 列 l = 16B datainfo + 16 值（ch = j*16+l*8+k）
        QCOMPARE(le32(b, 256 + 16), quint32(1000));       // 帧0列0 首值 ch0
        QCOMPARE(le32(b, 256 + 16 + 8 * 4), quint32(1016)); // 帧0列0 第9值 ch16
        QCOMPARE(le32(b, 256 + 80 + 16), quint32(1008));   // 帧0列1 首值 ch8
        QCOMPARE(le32(b, 256 + 160 + 16), quint32(1100));  // 帧1列0 首值
        QCOMPARE(le32(b, 256 + 160 + 16 + 8 * 4), quint32(1116));
        // datainfo 16 字节全零
        for (int i = 0; i < 16; ++i) QCOMPARE(quint8(b[256 + i]), quint8(0));
    }

    // ORI 非 allCol：nColNum 列拆 -N.ori，每文件 yLength=nDatanum
    void oriMultiColumnSplitsFiles() {
        QTemporaryDir dir;
        const QString path = dir.filePath("t.ori");
        Options opt; opt.nAdm = 2; opt.nRowNum = 8; opt.nColNum = 2; opt.allColumn = false;
        const auto frames = makeFrames(2, 32);
        QVERIFY(ImageDataWriter::save(path, Format::Ori, frames, {}, opt));

        for (int nfile = 0; nfile < 2; ++nfile) {
            const QByteArray b = readFile(dir.filePath(QStringLiteral("t-%1.ori").arg(nfile)));
            QCOMPARE(b.size(), 256 + 80 * 2);            // yLength=nDatanum
            QCOMPARE(le32(b, 68), quint32(2));
            QCOMPARE(le32(b, 256 + 16), quint32(1000 + nfile * 8));
            QCOMPARE(le32(b, 256 + 80 + 16), quint32(1100 + nfile * 8));
        }
    }

    // IMG：大端头 + 像素移位（16bit 右对齐 / 32bit 左对齐，adBit=20）
    void imgHeaderBigEndianAndShift() {
        QTemporaryDir dir;
        const auto frames = makeFrames(2, 8);
        {
            const QString path = dir.filePath("a.img");
            Options opt; opt.nAdm = 1; opt.nRowNum = 8; opt.nColNum = 1; opt.pixel16 = true;
            opt.saveExdata = false;
            QVERIFY(ImageDataWriter::save(path, Format::Img, frames, {}, opt));
            const QByteArray b = readFile(path);
            QCOMPARE(be16(b, 0), quint16(100));         // version
            QVERIFY(be32(b, 20) > quint32(1700000000)); // time.totalSeconds
            QCOMPARE(be16(b, 2), quint16(2));          // xLength = nDatanum
            QCOMPARE(be16(b, 4), quint16(8));          // yLength = n
            QCOMPARE(be16(b, 14), quint16(16));        // pixelType
            // 数据：通道×帧，16bit = v>>(adBit-16)
            QCOMPARE(b.size(), 36 + 8 * 2 * 2);
            QCOMPARE(be16(b, 36), quint16(1000 >> 4));  // ch0 帧0
            QCOMPARE(be16(b, 38), quint16(1100 >> 4));  // ch0 帧1
            QCOMPARE(be16(b, 40), quint16(1001 >> 4));  // ch1 帧0
        }
        {
            const QString path = dir.filePath("b.img");
            Options opt; opt.nAdm = 1; opt.nRowNum = 8; opt.nColNum = 1; opt.pixel16 = false;
            opt.saveExdata = false;
            QVERIFY(ImageDataWriter::save(path, Format::Img, frames, {}, opt));
            const QByteArray b = readFile(path);
            QCOMPARE(be16(b, 14), quint16(32));        // pixelType 32bit
            QCOMPARE(b.size(), 36 + 8 * 2 * 4);
            // 32bit = v<<(32-adBit)：quint32 钳位（Windows unsigned long 32 位语义）
            QCOMPARE(be32(b, 36), quint32(quint32(1000) << 12));
            QCOMPARE(be32(b, 40), quint32(quint32(1100) << 12));
            QCOMPARE(be32(b, 44), quint32(quint32(1001) << 12));
        }
    }

    // IMG bSaveExdata：探测器数据前 4 行 = accE/godown/Ain0>>shift/0
    void imgExdataRows() {
        QTemporaryDir dir;
        const QString path = dir.filePath("t.img");
        Options opt; opt.nAdm = 1; opt.nRowNum = 8; opt.nColNum = 1; opt.pixel16 = true;
        opt.saveExdata = true;
        QVector<Mcb::FrameEx> ex{makeEx(0x1234, 1875, 0), makeEx(0x40, 62, 1)};
        QVERIFY(ImageDataWriter::save(path, Format::Img, makeFrames(2, 8), ex, opt));
        const QByteArray b = readFile(path);
        QCOMPARE(be16(b, 4), quint16(8 + 4));          // yLength = n + 4 行
        QCOMPARE(be16(b, 36), quint16(1875));           // 行0 accE 帧0
        QCOMPARE(be16(b, 38), quint16(62));             // 行0 accE 帧1
        QCOMPARE(be16(b, 40), quint16(0));              // 行1 godown 帧0
        QCOMPARE(be16(b, 42), quint16(1));              // 行1 godown 帧1
        QCOMPARE(be16(b, 44), quint16(0x1234 >> 4));    // 行2 Ain0>>(20-16)
        QCOMPARE(be16(b, 46), quint16(0x40 >> 4));
        QCOMPARE(be16(b, 48), quint16(0));              // 行3 恒 0
        QCOMPARE(be16(b, 50), quint16(0));
    }

    // IMG bOriGM：末尾 16 行 = ex_data[0][0]（16bit 移位 / 32bit 原值不移位）
    void imgOriGmRows() {
        QTemporaryDir dir;
        QVector<Mcb::FrameEx> ex{makeEx(0x1234, 1875, 0), makeEx(0x40, 62, 1)};
        {
            const QString path = dir.filePath("t32.img");
            Options opt; opt.nAdm = 1; opt.nRowNum = 8; opt.nColNum = 1;
            opt.pixel16 = false; opt.oriGM = true; opt.saveExdata = false;
            QVERIFY(ImageDataWriter::save(path, Format::Img, makeFrames(2, 8), ex, opt));
            const QByteArray b = readFile(path);
            QCOMPARE(be16(b, 4), quint16(8 + 16));     // yLength = n + 16
            const int gm0 = 36 + 8 * 2 * 4;             // 探测器 8 行后
            QCOMPARE(be32(b, gm0), quint32(0x1234));    // 32bit GM 原值不移位
            QCOMPARE(be32(b, gm0 + 4), quint32(0x40));
            QCOMPARE(be32(b, gm0 + 15 * 8), quint32(0x1234)); // 第 16 行同值
            QCOMPARE(b.size(), 36 + (8 + 16) * 2 * 4);
        }
        {
            const QString path = dir.filePath("t16.img");
            Options opt; opt.nAdm = 1; opt.nRowNum = 8; opt.nColNum = 1;
            opt.pixel16 = true; opt.oriGM = true; opt.saveExdata = false; opt.fnd = true;
            QVERIFY(ImageDataWriter::save(path, Format::Img, makeFrames(2, 8), ex, opt));
            const QByteArray b = readFile(path);
            const int gm0 = 36 + 8 * 2 * 2;
            QCOMPARE(be16(b, gm0), quint16(0x1234));    // bFND：16bit GM 不移位
        }
    }

    // CSV：头 SN,M{j}CH{l}-{k:02d},...,EXCH0..4；行 = SN,通道值,ex0..ex4
    void csvFormat() {
        QTemporaryDir dir;
        const QString path = dir.filePath("t.csv");
        Options opt; opt.nAdm = 1; opt.nRowNum = 8; opt.nColNum = 2;   // nChn=16
        const auto frames = makeFrames(2, 16);
        QVector<Mcb::FrameEx> ex{makeEx(11, 1875, 0), makeEx(22, 62, 1)};
        QVERIFY(ImageDataWriter::save(path, Format::Csv, frames, ex, opt));
        const QStringList lines = QString::fromUtf8(readFile(path)).split(QLatin1Char('\n'));
        QCOMPARE(lines.at(0), QStringLiteral(
            "SN,M0CH0-00,M0CH0-01,M0CH0-02,M0CH0-03,M0CH0-04,M0CH0-05,M0CH0-06,M0CH0-07,"
            "M0CH1-00,M0CH1-01,M0CH1-02,M0CH1-03,M0CH1-04,M0CH1-05,M0CH1-06,M0CH1-07,"
            "EXCH0,EXCH1,EXCH2,EXCH3,EXCH4"));
        QCOMPARE(lines.at(1), QStringLiteral(
            "0,1000,1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1011,1012,1013,1014,1015,"
            "11,2,3,4,5"));
        QCOMPARE(lines.at(2), QStringLiteral(
            "1,1100,1101,1102,1103,1104,1105,1106,1107,1108,1109,1110,1111,1112,1113,1114,1115,"
            "22,2,3,4,5"));
    }

    // bSaveExdata 附加文件：.acce/.Ain0 逐帧，.godown.csv 变化点
    void exdataSidecarFiles() {
        QTemporaryDir dir;
        const QString path = dir.filePath("t.img");
        QVector<Mcb::FrameEx> ex{makeEx(11, 1875, 0), makeEx(22, 1875, 1),
                                 makeEx(33, 62, 1), makeEx(44, 62, 0)};
        QVERIFY(ImageDataWriter::save(path, Format::Img, makeFrames(4, 8), ex, Options()));
        QCOMPARE(QString::fromUtf8(readFile(dir.filePath("t.img.acce"))),
                 QStringLiteral("1875\n1875\n62\n62\n"));
        QCOMPARE(QString::fromUtf8(readFile(dir.filePath("t.img.Ain0"))),
                 QStringLiteral("11\n22\n33\n44\n"));
        QCOMPARE(QString::fromUtf8(readFile(dir.filePath("t.img.godown.csv"))),
                 QStringLiteral("0,0\n1,1\n3,0\n"));
    }
};

QTEST_MAIN(TestImageData)
#include "test_imagedata.moc"
