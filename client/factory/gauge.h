#ifndef GAUGE_H
#define GAUGE_H

#include <QWidget>
#include <QColor>

class Gauge : public QWidget
{
    Q_OBJECT
public:
    explicit Gauge(QWidget *parent = nullptr);

    void setValue(double value);
    void setMinMax(double min, double max);
    void setUnits(const QString &units);
    void setColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_value;
    double m_min;
    double m_max;
    QString m_units;
    QColor m_color;
};

#endif // GAUGE_H


