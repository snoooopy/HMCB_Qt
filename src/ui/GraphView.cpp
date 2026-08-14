#include "GraphView.h"
#include "WaveformWidget.h"
#include "core/MeasurementEngine.h"
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QTimer>

GraphView::GraphView(QWidget* parent) : QDockWidget(QStringLiteral("波形 Graph"), parent) {
    auto* host = new QWidget(this);
    auto* lay = new QVBoxLayout(host);

    auto* bar = new QHBoxLayout;
    m_x0 = new QSpinBox; m_x0->setRange(0, 8192); m_x0->setValue(0);
    m_x1 = new QSpinBox; m_x1->setRange(1, 8192); m_x1->setValue(512);
    m_y0 = new QSpinBox; m_y0->setRange(0, 5000); m_y0->setValue(0);
    m_y1 = new QSpinBox; m_y1->setRange(1, 5000); m_y1->setValue(5000);
    bar->addWidget(new QLabel("X0")); bar->addWidget(m_x0);
    bar->addWidget(new QLabel("X1")); bar->addWidget(m_x1);
    bar->addWidget(new QLabel("Y0")); bar->addWidget(m_y0);
    bar->addWidget(new QLabel("Y1")); bar->addWidget(m_y1);
    auto* apply = new QPushButton("Apply");
    m_point = new QCheckBox("Point");
    m_rms   = new QCheckBox("RMS");
    bar->addWidget(apply); bar->addWidget(m_point); bar->addWidget(m_rms);
    lay->addLayout(bar);

    m_wave = new WaveformWidget;
    lay->addWidget(m_wave);
    setWidget(host);

    connect(apply, &QPushButton::clicked, this, &GraphView::applyRange);
    connect(m_point, &QCheckBox::toggled, m_wave, &WaveformWidget::setPointMode);
    connect(m_rms,   &QCheckBox::toggled, m_wave, &WaveformWidget::setRmsMode);
    applyRange();

    auto* t = new QTimer(this);
    t->setInterval(400);
    connect(t, &QTimer::timeout, this, &GraphView::refresh);
    t->start();
}

void GraphView::applyRange() {
    // 钳位对照 DlgGView：X1 取 8 的倍数，Y1 取 5 的倍数
    int x1 = (m_x1->value() / 8) * 8;       if (x1 < 8) x1 = 8;
    int y1 = (m_y1->value() / 5) * 5;       if (y1 < 5) y1 = 5;
    m_wave->setRange(m_x0->value(), x1, m_y0->value(), y1);
}

void GraphView::refresh() {
    if (!m_eng) return;
    const auto& frame = m_eng->latestFrame(m_board);
    const double d2m = m_eng->config().data2mV;
    QVector<double> mv(frame.size());
    for (int i = 0; i < frame.size(); ++i) mv[i] = frame[i] / d2m;
    m_wave->setData(mv, m_eng->rms256(m_board));
}
