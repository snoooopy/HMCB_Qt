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

    // 双击表头循环显示模式 Raw→Vzero→Vnoise→Raw（对照 OnDblclkListData）
    connect(m_view->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
            this, [this](int) {
        setDisplayMode(static_cast<DisplayMode>((static_cast<int>(m_mode) + 1) % 3));
    });
}

void DataGridWidget::setDisplayFormat(DisplayFormat f) {
    if (m_fmt == f) return;
    m_fmt = f;
    refreshCurrent();
}

void DataGridWidget::setDisplayMode(DisplayMode m) {
    if (m_mode == m) return;
    m_mode = m;
    refreshCurrent();
}

void DataGridWidget::showMeasurement(const MeasurementEngine* eng, int nMCB, int function) {
    m_eng = eng;
    m_measMCB = nMCB;
    m_measFn = function;
    setDisplayMode(function == 0 ? DisplayMode::Vzero : DisplayMode::Vnoise);
}

void DataGridWidget::setThresholds(uint32_t lo, uint32_t hi) {
    m_lo = lo; m_hi = hi;
    refreshCurrent();
}

void DataGridWidget::setRows(int rows) {
    if (rows <= 0 || rows == m_rows) return;
    m_rows = rows;
    rebuildModel();
    refreshHeader();
    refreshCurrent();
}

int DataGridWidget::pageCount() const {
    return (kTotalCols + kPageCols - 1) / kPageCols;
}

void DataGridWidget::setPage(int page) {
    page = qBound(0, page, pageCount() - 1);
    if (page == m_page) return;
    m_page = page;
    refreshHeader();
    refreshCurrent();
}

void DataGridWidget::nextPage() { setPage(m_page + 1); }
void DataGridWidget::prevPage() { setPage(m_page - 1); }

void DataGridWidget::updateFrame(int nMCB, const Mcb::McbFrame& frame) {
    m_last = frame.channels;
    m_lastMCB = nMCB;
    if (m_mode == DisplayMode::Raw) refreshFromCache();   // 测量模式下不被新帧覆盖
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

void DataGridWidget::refreshCurrent() {
    if (m_mode == DisplayMode::Raw) refreshFromCache();
    else                            refreshMeasurement();
}

void DataGridWidget::refreshMeasurement() {
    if (!m_eng) return;
    const auto& cfg = m_eng->config();
    const int start = m_page * kPageCols;
    const QBrush pass(QColor(220, 255, 220));   // 合格：浅绿
    const QBrush fail(QColor(200, 200, 200));   // 不合格：灰底
    const QColor  failTxt(255, 0, 0);           // 不合格：红字
    for (int c = 0; c < kPageCols; ++c) {
        const int module = start + c;
        for (int r = 0; r < m_rows; ++r) {
            QStandardItem* it = m_model->item(r, c + 1);
            if (!it) continue;
            const int idx = module * m_rows + r;
            double value = 0; double lo = 0, hi = 0;
            if (m_mode == DisplayMode::Vzero) {
                const auto& z = m_eng->vz(m_measMCB);
                if (idx >= z.size()) { it->setText(QString()); continue; }
                value = (cfg.vzFormat == 1) ? z[idx] / cfg.data2mV : z[idx];
                lo = cfg.vzMin; hi = cfg.vzMax;
                it->setText(QString::number(value, 'f', cfg.vzFormat == 1 ? 2 : 0));
            } else {
                const auto& n = m_eng->vn(m_measMCB);
                if (idx >= n.size()) { it->setText(QString()); continue; }
                value = MeasurementEngine::toDisplayFormat(n[idx].fwhm, cfg.vnFormat, cfg.adBit);
                if (m_measFn == 3) { lo = cfg.vndMin; hi = cfg.vndMax; }
                else               { lo = cfg.vnMin;  hi = cfg.vnMax;  }
                it->setText(QString::number(value, 'f', cfg.adBit <= 16 ? 1 : 0));
            }
            const bool good = MeasurementEngine::passFail(value, lo, hi);
            it->setBackground(good ? pass : fail);
            it->setForeground(good ? QColor(0, 0, 0) : failTxt);
        }
    }
}

QString DataGridWidget::formatValue(uint32_t v) const {
    switch (m_fmt) {
    case DisplayFormat::Hex:
        return QStringLiteral("0x%1").arg(v, 4, 16, QChar('0')).toUpper();
    case DisplayFormat::mV: {
        // 20bit 标度：2^20 ↔ 4096 mV（与 Mock/引擎 data2mV 一致）
        const double mv = v * 4096.0 / 1048576.0;
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
