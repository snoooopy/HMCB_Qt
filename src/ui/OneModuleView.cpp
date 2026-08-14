#include "OneModuleView.h"
#include "core/MeasurementEngine.h"
#include <QTableWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QHeaderView>
#include <QTableWidgetItem>

OneModuleView::OneModuleView(QWidget* parent)
    : QDockWidget(QStringLiteral("单模块 Module DataView"), parent) {
    auto* host = new QWidget(this);
    auto* lay = new QVBoxLayout(host);

    auto* bar = new QHBoxLayout;
    bar->addWidget(new QLabel("Module:"));
    m_module = new QSpinBox; m_module->setRange(0, 0); m_module->setValue(0);
    auto* sel = new QPushButton("SELECT MODULE");
    bar->addWidget(m_module); bar->addWidget(sel); bar->addStretch(1);
    lay->addLayout(bar);

    m_table = new QTableWidget;
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setVisible(false);
    m_table->verticalHeader()->setVisible(false);
    // 双击 (0,0) 切 HEX/DEC（对照 DlgDView OnDblclkListData）
    connect(m_table, &QTableWidget::cellDoubleClicked, this,
            [this](int r, int c) { if (r == 0 && c == 0) { m_hex = !m_hex; refresh(); } });
    lay->addWidget(m_table);
    setWidget(host);

    connect(sel, &QPushButton::clicked, this, &OneModuleView::refresh);
    setModuleGeometry(8, 6, 1);
}

void OneModuleView::setModuleGeometry(int rows, int cols, int dmNum) {
    m_rows = qMax(1, rows); m_cols = qMax(1, cols); m_dmNum = qMax(1, dmNum);
    m_module->setRange(0, m_dmNum - 1);
    rebuildHeader();
    refresh();
}

void OneModuleView::rebuildHeader() {
    m_table->setRowCount(m_rows + 1);
    m_table->setColumnCount(m_cols + 1);
    m_table->setItem(0, 0, new QTableWidgetItem(m_hex ? QStringLiteral("HEX") : QStringLiteral("DEC")));
    for (int c = 0; c < m_cols; ++c)
        m_table->setItem(0, c + 1, new QTableWidgetItem(QString("C%1").arg(c)));
    for (int r = 0; r < m_rows; ++r)
        m_table->setItem(r + 1, 0, new QTableWidgetItem(QString("R%1").arg(r)));
}

void OneModuleView::refresh() {
    if (!m_eng) return;
    rebuildHeader();
    const auto& ch = m_eng->latestFrame(m_board);
    const int dm = m_module->value();
    for (int c = 0; c < m_cols; ++c) {
        for (int r = 0; r < m_rows; ++r) {
            const int idx = dm * m_cols * m_rows + c * m_rows + r;  // 列主序，对照 DlgDView:168
            QString txt;
            if (idx < ch.size())
                txt = m_hex ? QString("0x%1").arg(ch[idx], 4, 16, QChar('0')).toUpper()
                            : QString::number(ch[idx]);
            m_table->setItem(r + 1, c + 1, new QTableWidgetItem(txt));
        }
    }
}
