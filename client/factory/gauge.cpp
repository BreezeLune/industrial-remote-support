#include "gauge.h"
#include <QPainter>
#include <QConicalGradient>
#include <QtMath>

Gauge::Gauge(QWidget *parent)
    : QWidget(parent),
      m_value(0),
      m_min(0),
      m_max(100),
      m_units(""),
      m_color(Qt::cyan)
{
    setMinimumSize(100, 100);
}

void Gauge::setValue(double value) {
    if(value < m_min) value = m_min;
    if(value > m_max) value = m_max;
    m_value = value;
    update();
}

void Gauge::setMinMax(double min, double max) {
    m_min = min;
    m_max = max;
    update();
}

void Gauge::setUnits(const QString &units) {
    m_units = units;
    update();
}

void Gauge::setColor(const QColor &color) {
    m_color = color;
    update();
}

void Gauge::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int side = qMin(w, h);
    painter.translate(w/2, h/2);
    painter.scale(side/200.0, side/200.0); // scale to 200x200

    // Draw background circle
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(32, 37, 56));
    painter.drawEllipse(-90, -90, 180, 180);

    // Draw progress arc
    double spanAngle = 360.0 * (m_value - m_min) / (m_max - m_min);

    QPen pen(m_color, 15, Qt::SolidLine, Qt::FlatCap);
    painter.setPen(pen);
    painter.drawArc(-80, -80, 160, 160, 90*16, -spanAngle*16); // start at top, clockwise

    // Draw inner circle
    painter.setBrush(QColor(10, 15, 26));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(-65, -65, 130, 130);

    // Draw value text
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(20);
    painter.setFont(font);
    painter.drawText(-50, -20, 100, 40, Qt::AlignCenter, QString::number(m_value, 'f', 1));

    // Draw units text
    font.setPointSize(10);
    painter.setFont(font);
    painter.drawText(-50, 20, 100, 20, Qt::AlignCenter, m_units);
}


