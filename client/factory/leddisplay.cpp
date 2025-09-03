#include "LedDisplay.h"
#include <QPainter>

LedDisplay::LedDisplay(QWidget *parent)
    : QWidget(parent), ledColor(Qt::green), showLed(true)
{
    setMinimumHeight(60);
    setMinimumWidth(100);
    text = "0.0";
    label = "";

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &LedDisplay::blink);
    timer->start(1000); // slower blinking (1s interval)
}

void LedDisplay::setText(const QString &value) {
    text = value;
    update();
}

void LedDisplay::setColor(const QColor &color) {
    ledColor = color;
    update();
}

void LedDisplay::setLabel(const QString &labelText) {
    label = labelText;
    update();
}

void LedDisplay::setBlinkInterval(int ms) {
    timer->setInterval(ms);
}

void LedDisplay::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw dark background
    painter.setBrush(QColor(20, 20, 20));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 8, 8);

    // Draw label above number
    if (!label.isEmpty()) {
        QFont labelFont = painter.font();
        labelFont.setBold(true);
        labelFont.setPixelSize(height() * 0.2);
        painter.setFont(labelFont);
        painter.setPen(Qt::white);
        painter.drawText(QRect(0, 0, width(), height() * 0.3), Qt::AlignCenter, label);
    }

    // Draw LED text slightly lower
    if (showLed) {
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(height() * 0.3); // smaller numbers
        painter.setFont(font);
        painter.setPen(ledColor);
        painter.drawText(QRect(0, height() * 0.3, width(), height() * 0.7), Qt::AlignCenter, text);
    }
}

void LedDisplay::blink() {
    showLed = !showLed;
    update();
}

