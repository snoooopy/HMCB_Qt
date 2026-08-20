// DataGridWidget：网格生命周期 / 几何 / 表头行标 / 列宽（对照 Windows MCB_TESTView）
// 契约：
//   - 设备就绪前不建表（模型 0×0、水平表头隐藏 → 区域空白，同 Windows DVREADY 前空 CListCtrl）
//   - setGeometry(channels,rowNum,admCount)（DVREADY 后调用，几何来自 GETDMINFO=MCBCtrl.ini）
//   - 行标 " CH{c}-{rr}"、列头 Vo(格式)/ADM{页*10+c:02d}、尾页 ADM 之后为 ExDATA
//   - 首帧数据 + 格式切换后列宽自适应（不出现 … 省略号）
#include <QtTest>
#include <QApplication>
#include <QTableView>
#include <QHeaderView>
#include <QStandardItemModel>
#include "ui/DataGridWidget.h"

class TestGrid : public QObject {
    Q_OBJECT
private slots:
    void initialBlank();
    void geometryBuilds();
    void rowLabels();
    void lastPageHeaders();
    void dataAppears();
    void columnWidthShowsFull();
    void formatSwitchRelabels();
    void sixteenBitMvScale();
    void actualColumnsNoGhostAdm();
    void exDataFirstFiveRows();
    void hexSixDigits();
    void lastPageFewerColumns();

private:
    Mcb::McbFrame makeFrame(int adm, int rows, uint32_t seed);
};

// 初始状态：模型空、表头隐藏（空白区域）
void TestGrid::initialBlank() {
    DataGridWidget g;
    auto* m = g.findChild<QStandardItemModel*>();
    QVERIFY(m);
    QCOMPARE(m->rowCount(), 0);
    QCOMPARE(m->columnCount(), 0);
    auto* v = g.findChild<QTableView*>();
    QVERIFY(v);
    QVERIFY(!v->horizontalHeader()->isVisibleTo(v));
    QCOMPARE(g.pageCount(), 0);
}

// 就绪后建表：128 行、11 列、3 页（25 ADM）
void TestGrid::geometryBuilds() {
    DataGridWidget g;
    g.setGeometry(128, 32, 25);
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->rowCount(), 128);
    QCOMPARE(m->columnCount(), 11);           // 行标列 + 每页 10 ADM（VIEWADM_PG）
    QCOMPARE(g.pageCount(), 3);
    QCOMPARE(g.currentPage(), 0);
    QCOMPARE(m->headerData(0, Qt::Horizontal).toString(), QStringLiteral("Vo(DEC)"));
    QCOMPARE(m->headerData(1, Qt::Horizontal).toString(), QStringLiteral("ADM00"));
    QCOMPARE(m->headerData(10, Qt::Horizontal).toString(), QStringLiteral("ADM09"));
}

// 行标对照 Windows " CH%u-%02u"（列号-行号）
void TestGrid::rowLabels() {
    DataGridWidget g;
    g.setGeometry(128, 32, 25);
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->item(0, 0)->text(), QStringLiteral(" CH0-00"));
    QCOMPARE(m->item(33, 0)->text(), QStringLiteral(" CH1-01"));   // r=33 → 1*32+1
    QCOMPARE(m->item(127, 0)->text(), QStringLiteral(" CH3-31"));
}

// 尾页：ADM20..ADM24 之后是 ExDATA，再后为空
void TestGrid::lastPageHeaders() {
    DataGridWidget g;
    g.setGeometry(128, 32, 25);
    g.setPage(2);
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->headerData(1, Qt::Horizontal).toString(), QStringLiteral("ADM20"));
    QCOMPARE(m->headerData(5, Qt::Horizontal).toString(), QStringLiteral("ADM24"));
    QCOMPARE(m->headerData(6, Qt::Horizontal).toString(), QStringLiteral("ExDATA"));
    QCOMPARE(m->headerData(7, Qt::Horizontal).toString(), QString());
}

// 首帧后数据落位：idx=module*channels+r
void TestGrid::dataAppears() {
    DataGridWidget g;
    g.setGeometry(128, 32, 25);
    const auto f = makeFrame(25, 128, 1000000);
    g.updateFrame(0, f);
    QTest::qWait(320);                       // 250ms 合并刷新
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->item(0, 1)->text(), QStringLiteral("1000000"));       // channels[0]
    QCOMPARE(m->item(5, 3)->text(), QStringLiteral("1000261"));       // module=2,r=5 → idx=2*128+5
}

// 列宽足够容纳文本（默认显示完整，无 …）
void TestGrid::columnWidthShowsFull() {
    DataGridWidget g;
    g.setGeometry(128, 32, 25);
    auto* v = g.findChild<QTableView*>();
    const auto f = makeFrame(25, 128, 1000000);
    g.updateFrame(0, f);
    QTest::qWait(320);
    const int w = v->horizontalHeader()->sectionSize(1);
    const auto txt = g.findChild<QStandardItemModel*>()->item(0, 1)->text();
    const int need = QFontMetrics(v->font()).horizontalAdvance(txt);
    QVERIFY2(w >= need + 8, qPrintable(QStringLiteral("列宽 %1 < 文本需 %2").arg(w).arg(need + 8)));
}

// HEX/DEC/mV 切换：Vo() 表头联动
void TestGrid::formatSwitchRelabels() {
    DataGridWidget g;
    g.setGeometry(128, 32, 25);
    g.setDisplayFormat(DataGridWidget::DisplayFormat::Hex);
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->headerData(0, Qt::Horizontal).toString(), QStringLiteral("Vo(HEX)"));
    g.setDisplayFormat(DataGridWidget::DisplayFormat::mV);
    QCOMPARE(m->headerData(0, Qt::Horizontal).toString(), QStringLiteral("Vo(mV)"));
}

// mV 换算按实际 AD 位数（对照 MCB_TESTView.cpp:461-472 的 GETDMADBIT switch：
// 16→65536/5000、20→1048576/4096、24→16777216/5000）。实机 CX1664DE 为 16bit，
// 旧实现硬编码 20bit（÷256），16bit 显示值偏大 19.5 倍。
void TestGrid::sixteenBitMvScale() {
    DataGridWidget g;
    g.setGeometry(128, 64, 1);
    g.setAdBit(16);
    g.setDisplayFormat(DataGridWidget::DisplayFormat::mV);
    Mcb::McbFrame f = makeFrame(1, 128, 0);
    f.channels[0] = 65536;                   // 满量程 → 5000 mV（Windows %.3f）
    f.channels[1] = 32768;                   // 半量程 → 2500 mV
    g.updateFrame(0, f);
    QTest::qWait(320);
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->item(0, 1)->text(), QStringLiteral("5000.000"));
    QCOMPARE(m->item(1, 1)->text(), QStringLiteral("2500.000"));
}

// 实机几何（1 DM）：只建 ADM00 + ExDATA 两数据列，无幽灵 ADM01-09 空列
// （对照 Windows 截图：仅 Vo(HEX)/ADM00/ExDATA 三列）
void TestGrid::actualColumnsNoGhostAdm() {
    DataGridWidget g;
    g.setGeometry(128, 64, 1);
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->columnCount(), 3);           // 行标 + ADM00 + ExDATA
    QCOMPARE(m->headerData(1, Qt::Horizontal).toString(), QStringLiteral("ADM00"));
    QCOMPARE(m->headerData(2, Qt::Horizontal).toString(), QStringLiteral("ExDATA"));
    QCOMPARE(g.pageCount(), 1);
}

// ExDATA 列仅前 5 行显示帧扩展数据（对照 MCB_TESTView.cpp:2110 `i<5` 与
// FrameEx 填充链：ex[0]=ain0、[1]=ptsmH>>16、[2]=ptsmH&0xffff、
// [3]=ptsmL>>16、[4]=ptsmL&0xffff），第 6 行起空白
void TestGrid::exDataFirstFiveRows() {
    DataGridWidget g;
    g.setGeometry(128, 64, 1);
    g.setDisplayFormat(DataGridWidget::DisplayFormat::Hex);
    Mcb::McbFrame f = makeFrame(1, 128, 0);
    f.ain0  = 0x008000;
    f.ptsmH = 0x00F00000;
    f.ptsmL = 0x000064F0;
    g.updateFrame(0, f);
    QTest::qWait(320);
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->item(0, 2)->text(), QStringLiteral("008000"));   // ain0（截图同值）
    QCOMPARE(m->item(1, 2)->text(), QStringLiteral("0000F0"));   // ptsmH>>16
    QCOMPARE(m->item(2, 2)->text(), QStringLiteral("000000"));   // ptsmH&0xffff
    QCOMPARE(m->item(3, 2)->text(), QStringLiteral("000000"));   // ptsmL>>16
    QCOMPARE(m->item(4, 2)->text(), QStringLiteral("0064F0"));   // ptsmL&0xffff
    QCOMPARE(m->item(5, 2)->text(), QString());                  // 第 6 行起空白
}

// HEX 格式 %06X 六位大写（对照 Windows %06X 与截图 00FFFF），无 0x 前缀
void TestGrid::hexSixDigits() {
    DataGridWidget g;
    g.setGeometry(128, 64, 1);
    g.setDisplayFormat(DataGridWidget::DisplayFormat::Hex);
    Mcb::McbFrame f = makeFrame(1, 128, 0);
    f.channels[0] = 0xFFFF;
    g.updateFrame(0, f);
    QTest::qWait(320);
    QCOMPARE(g.findChild<QStandardItemModel*>()->item(0, 1)->text(),
             QStringLiteral("00FFFF"));
}

// 尾页列数按剩余 ADM 收缩（25 ADM + ExDATA=26 列 → 页 2 仅 6 数据列）
void TestGrid::lastPageFewerColumns() {
    DataGridWidget g;
    g.setGeometry(128, 32, 25);
    g.setPage(2);
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->columnCount(), 7);           // 行标 + ADM20..24 + ExDATA
    QCOMPARE(m->headerData(6, Qt::Horizontal).toString(), QStringLiteral("ExDATA"));
}

Mcb::McbFrame TestGrid::makeFrame(int adm, int rows, uint32_t base) {
    Mcb::McbFrame f;
    f.nMCB = 0;
    f.scounter = 1;
    f.channels.resize(adm * rows);
    for (int i = 0; i < adm * rows; ++i)
        f.channels[i] = base + i;            // 确定值便于断言（7 位 DEC 文本，列宽有区分度）
    return f;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);            // QWidget 需要 QApplication
    TestGrid t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_datagrid.moc"
