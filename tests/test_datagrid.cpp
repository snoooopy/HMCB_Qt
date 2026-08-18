// DataGridWidget：网格生命周期 / 几何 / 表头行标 / 列宽（对照 Windows MCB_TESTView）
// 契约：
//   - 设备就绪前不建表（模型 0×0、水平表头隐藏 → 区域空白，同 Windows DVREADY 前空 CListCtrl）
//   - setGeometry(channels,rowNum,admCount)（DVREADY 后调用，几何来自 GETDMINFO=MCBCtrl.ini）
//     每通道 2 行（数据行 + 空间隔行，同 Windows InsertItem(2i+2/2i+3)）
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

// 就绪后建表：128 通道 × 2 行、11 列、3 页（25 ADM）
void TestGrid::geometryBuilds() {
    DataGridWidget g;
    g.setGeometry(128, 32, 25);
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->rowCount(), 128 * 2);
    QCOMPARE(m->columnCount(), 11);           // 行标列 + 每页 10 ADM（VIEWADM_PG）
    QCOMPARE(g.pageCount(), 3);
    QCOMPARE(g.currentPage(), 0);
    QCOMPARE(m->headerData(0, Qt::Horizontal).toString(), QStringLiteral("Vo(DEC)"));
    QCOMPARE(m->headerData(1, Qt::Horizontal).toString(), QStringLiteral("ADM00"));
    QCOMPARE(m->headerData(10, Qt::Horizontal).toString(), QStringLiteral("ADM09"));
}

// 行标对照 Windows " CH%u-%02u"（列号-行号，间隔行无标）
void TestGrid::rowLabels() {
    DataGridWidget g;
    g.setGeometry(128, 32, 25);
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->item(0, 0)->text(), QStringLiteral(" CH0-00"));
    QCOMPARE(m->item(2 * 33, 0)->text(), QStringLiteral(" CH1-01"));   // r=33 → 1*32+1
    QCOMPARE(m->item(2 * 127, 0)->text(), QStringLiteral(" CH3-31"));
    QVERIFY(m->item(1, 0)->text().isEmpty());   // 间隔行
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

// 首帧后数据落在数据行（2r），间隔行保持空
void TestGrid::dataAppears() {
    DataGridWidget g;
    g.setGeometry(128, 32, 25);
    const auto f = makeFrame(25, 128, 1000000);
    g.updateFrame(0, f);
    QTest::qWait(320);                       // 250ms 合并刷新
    auto* m = g.findChild<QStandardItemModel*>();
    QCOMPARE(m->item(0, 1)->text(), QStringLiteral("1000000"));       // channels[0]
    QCOMPARE(m->item(2 * 5, 3)->text(), QStringLiteral("1000261"));   // module=2,r=5 → idx=2*128+5
    QVERIFY(m->item(1, 1)->text().isEmpty());
    QVERIFY(m->item(2 * 5 + 1, 3)->text().isEmpty());
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
