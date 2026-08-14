#pragma once
#include <QDockWidget>
class WaveformWidget;
class MeasurementEngine;
class QSpinBox;
class QCheckBox;
class QPushButton;

// 波形停靠窗口：包 WaveformWidget + X0/X1/Y0/Y1 + Apply + Point/RMS，400ms 取数刷新。
class GraphView : public QDockWidget {
    Q_OBJECT
public:
    explicit GraphView(QWidget* parent = nullptr);
    void setEngine(const MeasurementEngine* e) { m_eng = e; }
    void setBoard(int nMCB) { m_board = nMCB; }

private:
    WaveformWidget* m_wave;
    const MeasurementEngine* m_eng = nullptr;
    int m_board = 0;
    QSpinBox *m_x0, *m_x1, *m_y0, *m_y1;
    QCheckBox *m_point, *m_rms;
    void applyRange();
    void refresh();   // 定时调用
};
