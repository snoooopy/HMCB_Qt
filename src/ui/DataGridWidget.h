#pragma once
#include <QWidget>
#include <cstdint>
#include <QVector>
#include "core/McbTypes.h"

class QTableView;
class QStandardItemModel;
class QLabel;

// 中央数据网格：QTableView 展示一帧通道数据。
// 列 = 当前页的若干 AD 模块（MAX_MCBADNUM=64，每页 10 个，共 7 页），
// 行 = 每模块通道数（MAX_DMROWNUM*MAX_DMCOLNUM=1024，可按实际几何收缩）。
// 显示格式 HEX/DEC/mV；按上下限阈值着色（Phase1 默认全通过）。
class DataGridWidget : public QWidget {
    Q_OBJECT
public:
    enum class DisplayFormat { Hex, Dec, mV };

    explicit DataGridWidget(QWidget* parent = nullptr);

    void setDisplayFormat(DisplayFormat f);
    DisplayFormat displayFormat() const { return m_fmt; }

    void setThresholds(uint32_t lo, uint32_t hi);  // 着色区间（闭区间内=通过色）
    void setRows(int rows);                          // 按设备几何设置每模块通道行数

    int  pageCount() const;
    int  currentPage() const { return m_page; }
    void setPage(int page);

public slots:
    void nextPage();
    void prevPage();
    void updateFrame(int nMCB, const Mcb::McbFrame& frame);

private:
    QTableView* m_view = nullptr;
    QStandardItemModel* m_model = nullptr;
    QLabel* m_pageLabel = nullptr;

    DisplayFormat m_fmt = DisplayFormat::Dec;
    int m_page = 0;
    static constexpr int kPageCols = 10;       // 每页 AD 模块数
    static constexpr int kTotalCols = 64;      // MAX_MCBADNUM
    int m_rows = 128 * 8;                       // MAX_DMROWNUM*MAX_DMCOLNUM
    uint32_t m_lo = 0, m_hi = 4095;             // 默认阈值=全程通过

    QVector<uint32_t> m_last;
    int m_lastMCB = -1;

    void rebuildModel();
    void refreshFromCache();
    void refreshHeader();
    QString formatValue(uint32_t v) const;
    static QString colorFor(uint32_t v, uint32_t lo, uint32_t hi);
};
