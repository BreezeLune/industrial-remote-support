#ifndef DEVICESHOWER_H
#define DEVICESHOWER_H

#include <QWidget>
#include <QTcpSocket>
#include <QVector>

struct SensorData {
    float pumpPressure;
    float pumpVibration;
    float motorCurrent;
    float oilLevel;
    float pumpTemp;
    float flowRate;
};

class Gauge;
class LedDisplay;

class DeviceShower : public QWidget {
    Q_OBJECT
public:
    explicit DeviceShower(QWidget *parent = nullptr);

private slots:
    void onDataReceived();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket *socket;
    QVector<Gauge*> gauges;
    QVector<LedDisplay*> leds;

    SensorData parseSensorData(const QString& csv);
    void updateSensorData(const SensorData& data);
};

#endif // DEVICESHOWER_H
