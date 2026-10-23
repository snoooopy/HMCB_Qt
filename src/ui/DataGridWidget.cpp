#include "DataGridWidget.h"
#include <cmath>
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
#include <QTimer>

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
    m_prevBtn = prev;
    m_nextBtn = next;
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
    // 列宽一次性自适应后转 Interactive：ResizeToContents 模式下每个
    // dataChanged 都整列重算宽度（文本测量），帧刷新时会饱和 GUI 线程
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_view->verticalHeader()->setVisible(false);
    // 设备就绪前不建表（对照 Windows：DVREADY 前 CListCtrl 无列无行 → 空白）
    m_view->horizontalHeader()->hide();
    lay->addWidget(m_view, 1);

    // 脏帧合并刷新：首帧启动定时器，250ms 后刷一次（期间新帧只标脏）
    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    m_flushTimer->setInterval(kFlushMs);
    connect(m_flushTimer, &QTimer::timeout, this, [this] {
        if (m_dirty) {
            m_dirty = false;
            refreshFromCache();
            maybeResize();   // 首帧/格式切换后一次性列宽自适应（消除 … 省略号）
        }
    });

    // 双击表头：循环显示模式 Raw→Vzero→Vnoise→Inl→Raw；Inl 内再循环 5 子视图
    connect(m_view->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
            this, [this](int) {
        if (m_mode == DisplayMode::Inl) {
            m_inlSub = (m_inlSub + 1) % 5;
            refreshCurrent();
            refreshHeader();
        } else {
            setDisplayMode(static_cast<DisplayMode>((static_cast<int>(m_mode) + 1) % 4));
        }
    });
}

void DataGridWidget::setDisplayFormat(DisplayFormat f) {
    if (m_fmt == f) return;
    m_fmt = f;
    if (m_built) refreshHeader();   // 列头 "Vo(HEX/DEC/mV)" 联动
    refreshCurrent();
    m_sizedDirty = true;            // HEX/DEC/mV 文本宽度不同
    maybeResize();
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
    m_inlSub = 0;
    m_mode = (function == 0) ? DisplayMode::Vzero
            : (function == 2) ? DisplayMode::Inl
            : (function == 4) ? DisplayMode::VzeroX   // X-DIF 显示净信号 VzX
            : DisplayMode::Vnoise;
    refreshHeader();
    refreshCurrent();
}

void DataGridWidget::setThresholds(uint32_t lo, uint32_t hi) {
    m_lo = lo; m_hi = hi;
    refreshCurrent();
}

void DataGridWidget::setGeometry(int channels, int rowNum, int admCount) {
    if (channels <= 0 || rowNum <= 0 || admCount <= 0) return;
    m_rows = channels;
    m_rowNum = rowNum;
    m_admCount = admCount;
    m_built = true;
    m_page = 0;
    m_hadData = false;
    m_sizedDirty = true;
    rebuildModel();
    refreshHeader();
    m_view->horizontalHeader()->show();
    refreshCurrent();
    maybeResize();
}

int DataGridWidget::pageCount() const {
    // 数据列 = ADM 数 + 尾列 ExDATA（对照 Windows FillDataList：ADM 之后一位为 ExDATA）
    return m_built ? (m_admCount + 1 + kPageCols - 1) / kPageCols : 0;
}

void DataGridWidget::setAdBit(int bit) {
    if (bit <= 0) return;
    m_adBit = bit;
    // 对照 MCB_TESTView.cpp:461-472（GETDMADBIT 后的 m_DATA2mV switch）
    m_data2mV = MeasurementEngine::data2mVFromAdBit(bit);
    // exdata 专用标度（对照 :2112-2114）：<24bit 为 (16777216>>(24-bit))/10000
    m_exD2mV = (bit < 24)
        ? double(int32_t(16777216) >> (24 - bit)) / 10000.0
        : 16777216.0 / 10000.0;
    refreshCurrent();
    m_sizedDirty = true;    // mV 文本宽度随系数变化
    maybeResize();
}

int DataGridWidget::pageDataCols() const {
    if (!m_built) return 0;
    const int total = m_admCount + 1;   // + ExDATA 尾列
    return qMin(kPageCols, total - m_page * kPageCols);
}

void DataGridWidget::setPage(int page) {
    if (!m_built) return;
    page = qBound(0, page, pageCount() - 1);
    if (page == m_page) return;
    m_page = page;
    if (m_model->columnCount() != pageDataCols() + 1) rebuildModel();   // 尾页列数收缩
    refreshHeader();
    refreshCurrent();
}

void DataGridWidget::nextPage() { setPage(m_page + 1); }
void DataGridWidget::prevPage() { setPage(m_page - 1); }

void DataGridWidget::updateFrame(int nMCB, const Mcb::McbFrame& frame) {
    const int b = qBound(0, nMCB, 7);
    m_frameByBoard[b] = frame;       // 各板最近帧缓存（切板立即呈现用）
    m_hasFrame[b] = true;
    m_lastMCB = nMCB;
    if (b != m_board) return;        // 非当前显示板：只缓存不刷新
    applyFrame(frame);
}

void DataGridWidget::setCurrentBoard(int board) {
    board = qBound(0, board, 7);
    if (board == m_board) return;
    m_board = board;
    if (m_hasFrame[m_board])         // 立即呈现该板最近一帧
        applyFrame(m_frameByBoard[m_board]);
}

void DataGridWidget::applyFrame(const Mcb::McbFrame& frame) {
    m_last = frame.channels;
    m_lastMCB = frame.nMCB;
    // ExDATA 列数据源（前 5 行）：对照 FrameEx 填充链
    m_ex[0] = int32_t(frame.ain0);
    m_ex[1] = int32_t(frame.ptsmH >> 16);
    m_ex[2] = int32_t(frame.ptsmH & 0xffff);
    m_ex[3] = int32_t(frame.ptsmL >> 16);
    m_ex[4] = int32_t(frame.ptsmL & 0xffff);
    m_hasEx = true;
    // 测量模式下不被新帧覆盖；Raw 模式节流合并刷新（见 m_flushTimer）
    if (m_mode == DisplayMode::Raw) {
        m_dirty = true;
        if (!m_flushTimer->isActive()) m_flushTimer->start();
    }
}

void DataGridWidget::rebuildModel() {
    m_model->clear();
    // 每通道 1 行：行标 " CH{c}-{rr}" + 当前页各数据列（ADM+尾列 ExDATA，
    // 不足一页按实际收缩——实机 1 DM 时仅 ADM00+ExDATA，不建幽灵空列）。
    // 注：Windows InitialDataList 每通道后另插一个空行（InsertItem(2i+3)），
    // 但 MFC 列表无网格线，空行只是行距效果；Qt 带网格线会显成"缺数据空格"，
    // 故不复刻。
    const int cols = pageDataCols();
    m_model->setRowCount(m_rows);
    m_model->setColumnCount(cols + 1);   // 第 0 列为行标
    for (int r = 0; r < m_rows; ++r) {
        auto* idx = new QStandardItem(
            QStringLiteral(" CH%1-%2")
                .arg(r / m_rowNum)
                .arg(r % m_rowNum, 2, 10, QChar('0')));
        idx->setEditable(false);
        idx->setBackground(QColor(230, 230, 230));
        m_model->setItem(r, 0, idx);
        for (int c = 0; c < cols; ++c) {
            auto* it = new QStandardItem;
            it->setEditable(false);
            m_model->setItem(r, c + 1, it);
        }
    }
}

void DataGridWidget::refreshHeader() {
    if (!m_built) return;
    const QString f = (m_fmt == DisplayFormat::Hex) ? QStringLiteral("Vo(HEX)")
                    : (m_fmt == DisplayFormat::mV)  ? QStringLiteral("Vo(mV)")
                                                    : QStringLiteral("Vo(DEC)");
    m_model->setHeaderData(0, Qt::Horizontal, f);
    const int start = m_page * kPageCols;
    for (int c = 0; c < pageDataCols(); ++c) {
        const int g = start + c;
        QString h;   // 对照 ChangeDataPage：ADM 之后第一位 ExDATA
        if (g < m_admCount)       h = QStringLiteral("ADM%1").arg(g, 2, 10, QChar('0'));
        else if (g == m_admCount) h = QStringLiteral("ExDATA");
        m_model->setHeaderData(c + 1, Qt::Horizontal, h);
    }
    // 翻页边界（对照 OnUpdateViewBack/OnUpdateViewForword）
    if (m_prevBtn) m_prevBtn->setEnabled(m_page > 0);
    if (m_nextBtn) m_nextBtn->setEnabled(m_page < pageCount() - 1);
    m_pageLabel->setText(QStringLiteral("Page %1/%2").arg(m_page + 1).arg(pageCount()));
    if (m_mode == DisplayMode::Inl) {   // 对照 m_nINLDataType 五态
        static const char* sub[] = {"INL(permille)", "DIV(%)", "r", "a", "b"};
        m_pageLabel->setText(m_pageLabel->text() + QStringLiteral("  ") + sub[m_inlSub]);
    }
}

void DataGridWidget::refreshFromCache() {
    if (!m_built || m_last.isEmpty()) return;
    const int start = m_page * kPageCols;
    const QBrush pass(QColor(220, 255, 220));   // 浅绿
    const QBrush fail(QColor(255, 215, 215));   // 浅红
    for (int c = 0; c < pageDataCols(); ++c) {
        const int module = start + c;
        if (module == m_admCount) {   // ExDATA 列：仅前 5 行，无阈值着色（对照 :2110）
            for (int r = 0; r < m_rows; ++r) {
                QStandardItem* it = m_model->item(r, c + 1);
                if (!it) continue;
                if (m_hasEx && r < 5) {
                    it->setText(formatEx(m_ex[r]));
                    it->setBackground(QBrush());
                } else {
                    it->setText(QString());
                }
            }
            continue;
        }
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
    if (!m_hadData) {          // 首帧数据到位：触发一次列宽自适应
        m_hadData = true;
        m_sizedDirty = true;
    }
}

void DataGridWidget::maybeResize() {
    if (!m_built || !m_sizedDirty) return;
    m_sizedDirty = false;
    // 一次性列宽自适应：数据完整显示（不出现 …）；不可用 ResizeToContents
    // 常驻模式（每个 dataChanged 整列文本测量会饱和 GUI 线程，见构造函数注释）
    m_view->resizeColumnsToContents();
}

void DataGridWidget::refreshCurrent() {
    if (m_mode == DisplayMode::Raw) refreshFromCache();
    else                            refreshMeasurement();
}

void DataGridWidget::refreshMeasurement() {
    if (!m_built || !m_eng) return;
    const auto& cfg = m_eng->config();
    const int start = m_page * kPageCols;
    const QBrush pass(QColor(220, 255, 220));   // 合格：浅绿
    const QBrush fail(QColor(200, 200, 200));   // 不合格：灰底
    const QColor  failTxt(255, 0, 0);           // 不合格：红字
    for (int c = 0; c < pageDataCols(); ++c) {
        const int module = start + c;
        for (int r = 0; r < m_rows; ++r) {
            QStandardItem* it = m_model->item(r, c + 1);
            if (!it) continue;
            const int idx = module * m_rows + r;
            double value = 0; double lo = 0, hi = 0;
            if (m_mode == DisplayMode::Vzero || m_mode == DisplayMode::VzeroX) {
                // X-DIF 显示 VzX（净信号，可负），格式与 Vzero 同（HEX/DEC/mV）
                const auto& z = (m_mode == DisplayMode::VzeroX)
                              ? m_eng->vzx(m_measMCB) : m_eng->vz(m_measMCB);
                if (idx >= z.size()) { it->setText(QString()); continue; }
                value = (cfg.vzFormat == 1) ? z[idx] / cfg.data2mV : z[idx];
                lo = cfg.vzMin; hi = cfg.vzMax;
                it->setText(QString::number(value, 'f', cfg.vzFormat == 1 ? 2 : 0));
            } else if (m_mode == DisplayMode::Inl) {
                const auto& inlV = m_eng->inl(m_measMCB);
                const auto& dif  = m_eng->inlDiff(m_measMCB);
                if (idx >= inlV.size() || idx >= dif.size()) { it->setText(QString()); continue; }
                const auto& A = m_eng->inlA(m_measMCB);
                const auto& B = m_eng->inlB(m_measMCB);
                const auto& R = m_eng->inlR(m_measMCB);
                switch (m_inlSub) {
                case 0: it->setText(QString::asprintf("%.3f", inlV[idx] * 1000.0)); break;
                case 1: it->setText(QString::asprintf("%.1f", (dif[idx] - 1.0) * 100.0)); break;
                case 2: it->setText(QString::asprintf("%.5f", R.value(idx))); break;
                case 3: it->setText(QString::asprintf("%.4f", A.value(idx))); break;
                default: it->setText(QString::asprintf("%.3f", B.value(idx))); break;
                }
                const bool good = MeasurementEngine::passFail(inlV[idx] * 1000.0, 0, cfg.inlMax)
                               && MeasurementEngine::passFail(std::fabs(dif[idx] - 1.0) * 100.0, 0, cfg.diffMax);
                it->setBackground(good ? pass : fail);
                it->setForeground(good ? QColor(0, 0, 0) : failTxt);
                continue;
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
        // %06X：六位补零大写、无 0x 前缀（对照 Windows %06X 与截图 00FFFF）
        return QStringLiteral("%1").arg(v, 6, 16, QChar('0')).toUpper();
    case DisplayFormat::mV:
        // 系数按实际 AD 位数（setAdBit 注入；对照 Windows %.3f）
        return QString::number(double(v) / m_data2mV, 'f', 3);
    case DisplayFormat::Dec:
    default:
        return QString::number(v);
    }
}

QString DataGridWidget::formatEx(int32_t v) const {
    // 对照 MCB_TESTView.cpp:2110-2140：exdata 与 ADC 同三态，但 mV 标度独立
    switch (m_fmt) {
    case DisplayFormat::Hex:
        return QStringLiteral("%1").arg(uint32_t(v), 6, 16, QChar('0')).toUpper();
    case DisplayFormat::mV:
        return QString::number(double(v) / m_exD2mV, 'f', 3);
    case DisplayFormat::Dec:
    default:
        return QString::number(uint32_t(v));
    }
}

QString DataGridWidget::colorFor(uint32_t, uint32_t, uint32_t) {
    return {};   // 预留：后续按阈值导出颜色规则
}
