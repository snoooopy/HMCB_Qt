#pragma once
#include <QWidget>
#include <cstdint>
#include <QVector>
#include "core/McbTypes.h"
#include "core/MeasurementEngine.h"

class QTableView;
class QStandardItemModel;
class QLabel;
class QTimer;

// 中央数据网格：QTableView 展示一帧通道数据。
// 生命周期对照 Windows MCB_TESTView：设备就绪（DVREADY）前不建表（区域空白），
// 就绪后按 GETDMINFO 几何建表（数据格空），首帧数据到达后填充。
// 行 = 每模块通道数（rowNum*colNum，行标 " CH{c}-{rr}"）；
// 列 = 当前页的 ADM 模块（每页 VIEWADM_PG=10 个，列头 ADMxx，尾页补 ExDATA）。
// 显示格式 HEX/DEC/mV；按上下限阈值着色（Phase1 默认全通过）。
class DataGridWidget : public QWidget {
    Q_OBJECT
public:
    enum class DisplayFormat { Hex, Dec, mV };
    enum class DisplayMode { Raw, Vzero, VzeroX, Vnoise, Inl };

    explicit DataGridWidget(QWidget* parent = nullptr);

    void setDisplayFormat(DisplayFormat f);
    DisplayFormat displayFormat() const { return m_fmt; }

    void setDisplayMode(DisplayMode m);
    DisplayMode displayMode() const { return m_mode; }
    // 注入测量结果并切到 Vz/VzX/Vn 模式刷新（function: 0 Zero 1/3 Noise 4 X-DIF）
    void showMeasurement(const MeasurementEngine* eng, int nMCB, int function);

    void setThresholds(uint32_t lo, uint32_t hi);  // 着色区间（闭区间内=通过色）

    // 设备就绪后按 GETDMINFO 几何建表：channels=rowNum*colNum、admCount=DM 模块数
    void setGeometry(int channels, int rowNum, int admCount);

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
    static constexpr int kPageCols = 10;       // 每页 ADM 数（VIEWADM_PG）
    int m_rows = 0;                             // 每模块通道数（=rowNum*colNum，未建表 0）
    int m_rowNum = 32;                          // 每 DM 行数（行标 CH{c}-{rr}）
    int m_admCount = 0;                         // ADM 模块数（列方向）
    bool m_built = false;                       // DVREADY 前不建表
    bool m_hadData = false;                     // 首帧后触发一次列宽自适应
    bool m_sizedDirty = false;                  // 待一次性列宽自适应
    uint32_t m_lo = 0, m_hi = 4095;             // 默认阈值=全程通过

    QVector<uint32_t> m_last;
    int m_lastMCB = -1;
    // 原始帧刷新节流：连续采集 10fps×1280 格全量 setText 会让 GUI 线程饱和
    // （曾致"开始采集后无响应"），改为脏标记 + 250ms 合并刷新
    QTimer* m_flushTimer = nullptr;
    bool m_dirty = false;
    static constexpr int kFlushMs = 250;

    DisplayMode m_mode = DisplayMode::Raw;
    const MeasurementEngine* m_eng = nullptr;
    int m_measMCB = 0, m_measFn = 0;
    int m_inlSub = 0;   // Inl 模式 5 子视图：0 INL‰ 1 DIV% 2 r 3 a 4 b

    void rebuildModel();
    void refreshFromCache();
    void refreshMeasurement();
    void refreshCurrent();   // 按当前模式派发
    void refreshHeader();
    void maybeResize();      // 一次性列宽自适应（防 … 省略号；不用 ResizeToContents 常驻防 GUI 饱和）
    QString formatValue(uint32_t v) const;
    static QString colorFor(uint32_t v, uint32_t lo, uint32_t hi);
};
