#pragma once
#include <QWidget>
#include <QVector>
#include <QPixmap>

// 波形：单帧全通道剖面图（X=通道号，Y=mV 或噪声 σ）。QPainter 双缓冲。
// 配色与坐标映射对照 Windows DrawWave.cpp。
class WaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget* parent = nullptr);

    void setRange(int x0, int x1, int y0, int y1);  // 通道范围 / mV 范围
    void setPointMode(bool p);                        // true=点 false=柱
    void setRmsMode(bool r);                          // true=画 σ(RMS) false=画 mV
    void setData(const QVector<double>& dataMv, const QVector<double>& dataRms);

    QSize sizeHint() const override { return {520, 320}; }

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override { m_dirty = true; update(); }

private:
    QVector<double> m_data, m_rms;
    int m_x0 = 0, m_x1 = 512, m_y0 = 0, m_y1 = 5000;
    bool m_point = false, m_rmsMode = false;
    QPixmap m_buf;
    bool m_dirty = true;
    const int m_offX = 50, m_offY = 25;
    int m_cx = 0, m_cy = 0;
    void redraw();
};
