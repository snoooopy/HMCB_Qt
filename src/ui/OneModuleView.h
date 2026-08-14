#pragma once
#include <QDockWidget>
#include <QVector>
#include <cstdint>
class QTableWidget;
class QSpinBox;
class QPushButton;
class MeasurementEngine;

// 单模块视图：当前板单个 AD 模块(DM)的 nRow×nColumn 物理网格（对照 CDlgDView）。
// 仅 HEX/DEC，无 mV/高亮/分页；模块号编辑框 + SELECT MODULE 切换。
class OneModuleView : public QDockWidget {
    Q_OBJECT
public:
    explicit OneModuleView(QWidget* parent = nullptr);
    void setEngine(const MeasurementEngine* e) { m_eng = e; }
    void setBoard(int nMCB) { m_board = nMCB; }
    void setModuleGeometry(int rows, int cols, int dmNum);  // 按设备几何配置网格

public slots:
    void refresh();

private:
    QTableWidget* m_table;
    QSpinBox* m_module;
    const MeasurementEngine* m_eng = nullptr;
    int m_board = 0, m_rows = 8, m_cols = 6, m_dmNum = 1;
    bool m_hex = true;
    void rebuildHeader();
};
