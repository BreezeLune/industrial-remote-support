#pragma once
#include <QWidget>
#include <QTimer>

class LedDisplay : public QWidget {
    Q_OBJECT
public:
    explicit LedDisplay(QWidget *parent = nullptr);

    void setText(const QString &value);
    void setColor(const QColor &color);
    void setLabel(const QString &labelText);
    void setBlinkInterval(int ms); // optional

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void blink();

private:
    QString text;
    QString label;
    QColor ledColor;
    bool showLed;
    QTimer *timer;
};


