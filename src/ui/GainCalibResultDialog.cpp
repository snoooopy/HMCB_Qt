#include "ui/GainCalibResultDialog.h"
#include <QTableWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QColor>
#include <algorithm>

GainCalibResultDialog::GainCalibResultDialog(const QVector<int>& dmCounts,
                                             const QList<QVector<uint8_t>>& gains,
                                             QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Detector Gain Automatic Calibration Result"));
    setModal(true);
    resize(360, 480);
    const int nBoards = gains.size();
    const int rows = nBoards > 0 ? *std::max_element(dmCounts.begin(), dmCounts.end()) : 0;

    auto* t = new QTableWidget(rows, nBoards + 1, this);
    auto* det = new QTableWidgetItem(QStringLiteral("Det. No."));
    det->setTextAlignment(Qt::AlignCenter);
    t->setHorizontalHeaderItem(0, det);
    for (int b = 0; b < nBoards; ++b) {
        auto* h = new QTableWidgetItem(QStringLiteral("HMCB%1").arg(b));
        h->setTextAlignment(Qt::AlignCenter);
        t->setHorizontalHeaderItem(b + 1, h);
    }
    for (int r = 0; r < rows; ++r)                       // 行首 DM 序号（0 起）
        t->setVerticalHeaderItem(r, new QTableWidgetItem(QString::asprintf("  %02d", r)));
    for (int b = 0; b < nBoards; ++b) {
        for (int r = 0; r < rows; ++r) {
            auto* c = new QTableWidgetItem;
            c->setTextAlignment(Qt::AlignCenter);
            c->setBackground(QColor(240, 247, 233));     // 对照 Windows 背景
            if (r < dmCounts.value(b, 0) && r < gains[b].size())
                c->setText(QString::asprintf("%d", gains[b][r]));
            t->setItem(r, b + 1, c);
        }
    }
    for (int c = 0; c <= nBoards; ++c) t->setColumnWidth(c, 90);
    t->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* ok = new QPushButton(QStringLiteral("OK"), this);
    ok->setFixedWidth(90);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    auto* lay = new QVBoxLayout(this);
    lay->addWidget(t, 1);
    lay->addWidget(ok, 0, Qt::AlignCenter);
}
