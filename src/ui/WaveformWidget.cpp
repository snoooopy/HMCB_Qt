#include "WaveformWidget.h"
#include <QPainter>
#include <QResizeEvent>

WaveformWidget::WaveformWidget(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(false);
    setMinimumSize(360, 240);
}

void WaveformWidget::setRange(int x0, int x1, int y0, int y1) {
    m_x0 = x0; m_x1 = qMax(1, x1); m_y0 = y0; m_y1 = qMax(1, y1);
    m_dirty = true; update();
}
void WaveformWidget::setPointMode(bool p) { m_point = p; m_dirty = true; update(); }
void WaveformWidget::setRmsMode(bool r)   { m_rmsMode = r; m_dirty = true; update(); }
void WaveformWidget::setData(const QVector<double>& dataMv, const QVector<double>& dataRms) {
    m_data = dataMv; m_rms = dataRms; m_dirty = true; update();
}

void WaveformWidget::paintEvent(QPaintEvent*) {
    if (m_buf.size() != size() || m_dirty) redraw();
    QPainter p(this);
    p.drawPixmap(0, 0, m_buf);
}

void WaveformWidget::redraw() {
    m_cx = qMax(64, ((width()  - m_offX - 10) / 64) * 64);
    m_cy = qMax(50, ((height() - m_offY - 20) / 50) * 50);
    m_buf = QPixmap(size());
    m_buf.fill(Qt::black);
    QPainter p(&m_buf);
    p.setRenderHint(QPainter::Antialiasing, false);
    const int left = m_offX, top = m_offY, right = m_offX + m_cx, bottom = m_offY + m_cy;

    // 网格（白）
    p.setPen(QColor(255, 255, 255));
    for (int i = 0; i <= 8; ++i) { int x = left + i * m_cx / 8;  p.drawLine(x, top, x, bottom); }
    for (int i = 0; i <= 5; ++i) { int y = top + i * m_cy / 5;  p.drawLine(left, y, right, y); }

    // 刻度文字：X 黄，Y 品红
    QFont f(QStringLiteral("Times New Roman"), 10); p.setFont(f);
    p.setPen(QColor(255, 255, 0));
    for (int i = 0; i <= 8; ++i) {
        int x = left + i * m_cx / 8;
        p.drawText(x - 16, bottom + 14,
                   QString::asprintf("%uCH", (unsigned)(m_x0 + i * (m_x1 - m_x0) / 8)));
    }
    p.setPen(QColor(255, 0, 255));
    for (int i = 0; i <= 5; ++i) {
        int y = bottom - i * m_cy / 5;
        p.drawText(2, y + 4, QString::asprintf("%4umV", (unsigned)(m_y0 + i * (m_y1 - m_y0) / 5)));
    }

    // 数据（暗绿竖条 + 黄点）；RMS 模式用 m_rms（σ，量程放大显示）
    const QVector<double>& d = m_rmsMode ? m_rms : m_data;
    const double yRange = m_rmsMode ? qMax(1.0, (double)m_y1 / 100.0) : (double)m_y1;
    const int n = qMin(m_x1, (int)d.size());
    p.setPen(QColor(0, 75, 0));
    for (int i = m_x0; i < n; ++i) {
        int x = left + (i - m_x0) * m_cx / m_x1;
        double dv = d.value(i);
        int dy = (int)((dv - (m_rmsMode ? 0.0 : (double)m_y0)) * m_cy / yRange);
        dy = qBound(0, dy, m_cy);
        int y = top + m_cy - dy;
        if (!m_point) p.drawLine(x + 1, y, x + 1, top + m_cy);
    }
    p.setPen(QColor(255, 255, 0));
    p.setBrush(QColor(255, 255, 0));
    for (int i = m_x0; i < n; ++i) {
        int x = left + (i - m_x0) * m_cx / m_x1;
        double dv = d.value(i);
        int dy = (int)((dv - (m_rmsMode ? 0.0 : (double)m_y0)) * m_cy / yRange);
        dy = qBound(0, dy, m_cy);
        int y = top + m_cy - dy;
        p.drawRect(x, y, 2, 2);
    }
    m_dirty = false;
}
