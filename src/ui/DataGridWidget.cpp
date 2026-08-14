#include "DataGridWidget.h"
#include <QTableView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QBrush>
#include <QColor>

DataGridWidget::DataGridWidget(QWidget* parent) : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    // 顶部工具条：格式切换 + 翻页
    auto* bar = new QHBoxLayout;
    bar->addWidget(new QLabel(QStringLiteral("Format:"), this));
    auto* fmt = new QComboBox(this);
    fmt->addItem(QStringLiteral("DEC"), static_cast<int>(DisplayFormat::Dec));
    fmt->addItem(QStringLiteral("HEX"), static_cast<int>(DisplayFormat::Hex));
    fmt->addItem(QStringLiteral("mV"),  static_cast<int>(DisplayFormat::mV));
    connect(fmt, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, fmt](int) { setDisplayFormat(static_cast<DisplayFormat>(fmt->currentData().toInt())); });

    auto* prev = new QPushButton(QStringLiteral("<"), this);
    auto* next = new QPushButton(QStringLiteral(">"), this);
    m_pageLabel = new QLabel(this);
    connect(prev, &QPushButton::clicked, this, &DataGridWidget::prevPage);
    connect(next, &QPushButton::clicked, this, &DataGridWidget::nextPage);

    bar->addWidget(fmt);
    bar->addStretch(1);
    bar->addWidget(prev);
    bar->addWidget(m_pageLabel);
    bar->addWidget(next);
    lay->addLayout(bar);

    m_view = new QTableView(this);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_model = new QStandardItemModel(this);
    m_view->setModel(m_model);
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_view->verticalHeader()->setVisible(false);
    lay->addWidget(m_view, 1);

    rebuildModel();
    refreshHeader();
}

void DataGridWidget::setDisplayFormat(DisplayFormat f) {
    if (m_fmt == f) return;
    m_fmt = f;
    refreshFromCache();
}

void DataGridWidget::setThresholds(uint32_t lo, uint32_t hi) {
    m_lo = lo; m_hi = hi;
    refreshFromCache();
}

void DataGridWidget::setRows(int rows) {
    if (rows <= 0 || rows == m_rows) return;
    m_rows = rows;
    rebuildModel();
    refreshHeader();
    refreshFromCache();
}

int DataGridWidget::pageCount() const {
    return (kTotalCols + kPageCols - 1) / kPageCols;
}

void DataGridWidget::setPage(int page) {
    page = qBound(0, page, pageCount() - 1);
    if (page == m_page) return;
    m_page = page;
    refreshHeader();
    refreshFromCache();
}

void DataGridWidget::nextPage() { setPage(m_page + 1); }
void DataGridWidget::prevPage() { setPage(m_page - 1); }

void DataGridWidget::updateFrame(int nMCB, const Mcb::McbFrame& frame) {
    m_last = frame.channels;
    m_lastMCB = nMCB;
    refreshFromCache();
}

void DataGridWidget::rebuildModel() {
    m_model->clear();
    m_model->setRowCount(m_rows);
    m_model->setColumnCount(kPageCols + 1);   // 第 0 列为行索引
    for (int r = 0; r < m_rows; ++r) {
        auto* idx = new QStandardItem(QString::number(r));
        idx->setEditable(false);
        idx->setBackground(QColor(230, 230, 230));
        m_model->setItem(r, 0, idx);
        for (int c = 0; c < kPageCols; ++c) {
            auto* it = new QStandardItem;
            it->setEditable(false);
            m_model->setItem(r, c + 1, it);
        }
    }
}

void DataGridWidget::refreshHeader() {
    const int start = m_page * kPageCols;
    m_model->setHeaderData(0, Qt::Horizontal, QStringLiteral("CH"));
    for (int c = 0; c < kPageCols; ++c)
        m_model->setHeaderData(c + 1, Qt::Horizontal,
                               QStringLiteral("AD%1").arg(start + c));
    m_pageLabel->setText(QStringLiteral("Page %1/%2").arg(m_page + 1).arg(pageCount()));
}

void DataGridWidget::refreshFromCache() {
    if (m_last.isEmpty()) return;
    const int start = m_page * kPageCols;
    const QBrush pass(QColor(220, 255, 220));   // 浅绿
    const QBrush fail(QColor(255, 215, 215));   // 浅红
    for (int c = 0; c < kPageCols; ++c) {
        const int module = start + c;
        for (int r = 0; r < m_rows; ++r) {
            const int idx = module * m_rows + r;
            QStandardItem* it = m_model->item(r, c + 1);
            if (!it) continue;
            if (idx >= m_last.size()) {
                it->setText(QString());
                continue;
            }
            const uint32_t v = m_last.at(idx);
            it->setText(formatValue(v));
            it->setBackground((v >= m_lo && v <= m_hi) ? pass : fail);
        }
    }
}

QString DataGridWidget::formatValue(uint32_t v) const {
    switch (m_fmt) {
    case DisplayFormat::Hex:
        return QStringLiteral("0x%1").arg(v, 4, 16, QChar('0')).toUpper();
    case DisplayFormat::mV: {
        // Phase1 固定系数：12bit ADC，参考 3.3V
        const double mv = v / 4095.0 * 3300.0;
        return QString::number(mv, 'f', 1);
    }
    case DisplayFormat::Dec:
    default:
        return QString::number(v);
    }
}

QString DataGridWidget::colorFor(uint32_t, uint32_t, uint32_t) {
    return {};   // 预留：后续按阈值导出颜色规则
}
