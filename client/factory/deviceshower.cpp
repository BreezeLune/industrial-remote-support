#include "deviceshower.h"
#include "LedDisplay.h"
#include "gauge.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QLineEdit>
#include <QPixmap>
#include <QDebug>

// Helper function to create a styled card with a label
static QFrame* makeCard(const QString &title) {
    QFrame *card = new QFrame();
    card->setStyleSheet("background:#202538; border-radius:8px;");
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(8, 8, 8, 8);

    QLabel *label = new QLabel(title);
    label->setStyleSheet("color:white; font-weight:bold; font-size:12px;");
    label->setAlignment(Qt::AlignCenter);

    cardLayout->addWidget(label);
    return card;
}

DeviceShower::DeviceShower(QWidget *parent) : QWidget(parent) {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // 数据名称和单位
    const QString gaugeNames[3] = { "泵出口压力", "泵轴振动", "流体流量" };
    const QString gaugeUnits[3] = { "MPa", "mm/s", "m³/h" };
    const QString ledNames[3]   = { "泵电机电流", "油箱液位", "泵体温度" };
    const QString ledUnits[3]   = { "A", "%", "°C" };

    // === Buttons row ===
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);

    QPushButton *equipButton = new QPushButton("Equipment");
    QPushButton *terminalButton = new QPushButton("Terminal");
    QPushButton *malfunctionButton = new QPushButton("Malfunction Info");

    QString buttonStyle = "QPushButton { background:#202538; color:white; border-radius:8px; font-weight:bold; padding:6px; }"
                          "QPushButton:hover { background:#303654; }";

    equipButton->setStyleSheet(buttonStyle);
    terminalButton->setStyleSheet(buttonStyle);
    malfunctionButton->setStyleSheet(buttonStyle);

    equipButton->setFixedSize(150, 40);
    terminalButton->setFixedSize(150, 40);
    malfunctionButton->setFixedSize(150, 40);

    buttonLayout->addWidget(equipButton);
    buttonLayout->addWidget(terminalButton);
    buttonLayout->addWidget(malfunctionButton);

    // Equipment menu
    QMenu *equipMenu = new QMenu(this);
    equipMenu->setStyleSheet(
        "QMenu { background-color: #202538; border: 1px solid #303654; border-radius: 8px; }"
        "QMenu::item { color: white; padding: 6px 12px; }"
        "QMenu::item:selected { background-color: #303654; }"
    );
    equipMenu->addAction(new QAction("Equipment 1", this));
    equipMenu->addAction(new QAction("Equipment 2", this));
    equipMenu->addAction(new QAction("Equipment 3", this));
    equipButton->setMenu(equipMenu);

    // === First row: Gauges ===
    QWidget *gaugeRowWidget = new QWidget();
    QHBoxLayout *gaugeLayoutRow = new QHBoxLayout(gaugeRowWidget);
    gaugeLayoutRow->setSpacing(12);
    gaugeLayoutRow->setContentsMargins(16, 0, 16, 0);

    gauges.clear();
    for (int i = 0; i < 3; i++) {
        Gauge *g = new Gauge();
        g->setMinMax(0, 200);
        g->setValue(0); // initial value
        g->setUnits(gaugeUnits[i]);
        g->setColor(QColor(0, 190, 255));

        QFrame *gCard = new QFrame();
        gCard->setStyleSheet("background:#202538; border-radius:8px;");
        QVBoxLayout *gCardLayout = new QVBoxLayout(gCard);
        gCardLayout->setContentsMargins(8, 8, 8, 8);
        gCardLayout->addWidget(g);

        // 在仪表下方添加中文名称和单位标签
        QLabel *label = new QLabel(gaugeNames[i] + " (" + gaugeUnits[i] + ")");
        label->setStyleSheet("color:white; font-size:12px;");
        label->setAlignment(Qt::AlignCenter);
        gCardLayout->addWidget(label);

        gaugeLayoutRow->addWidget(gCard);
        gauges.append(g);
    }

    // === Second row: LEDs ===
    QWidget *ledRowWidget = new QWidget();
    QHBoxLayout *ledLayoutRow = new QHBoxLayout(ledRowWidget);
    ledLayoutRow->setSpacing(12);
    ledLayoutRow->setContentsMargins(16, 0, 16, 0);

    leds.clear();
    for (int i = 0; i < 3; i++) {
        LedDisplay *led = new LedDisplay();
        led->setText("--");
        led->setColor(QColor(0, 255, 0));
        led->setLabel(ledNames[i] + " (" + ledUnits[i] + ")");
        led->setBlinkInterval(2000);

        QFrame *ledCard = new QFrame();
        ledCard->setStyleSheet("background:#202538; border-radius:8px;");
        QVBoxLayout *ledCardLayout = new QVBoxLayout(ledCard);
        ledCardLayout->setContentsMargins(8, 8, 8, 8);
        ledCardLayout->addWidget(led);

        ledLayoutRow->addWidget(ledCard);
        leds.append(led);
    }

    // === Third row: custom cards ===
    QHBoxLayout *customLayoutRow = new QHBoxLayout();
    customLayoutRow->setSpacing(12);

    // 1. 图片卡片
    QFrame *photoCard = new QFrame();
    photoCard->setStyleSheet("background:#202538; border-radius:8px;");
    QVBoxLayout *photoLayout = new QVBoxLayout(photoCard);
    photoLayout->setContentsMargins(8, 8, 8, 8);
    QLabel *photoLabel = new QLabel();
    photoLabel->setPixmap(QPixmap(":/photo.jpg").scaled(120, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    photoLabel->setAlignment(Qt::AlignCenter);
    photoLayout->addWidget(photoLabel);
    QLabel *photoText = new QLabel("现场画面录制", photoCard);
    photoText->setStyleSheet("color:white; font-size:12px;");
    photoText->setAlignment(Qt::AlignCenter);
    photoLayout->addWidget(photoText);
    customLayoutRow->addWidget(photoCard);

    // 2. 终端卡片（白色背景，红色字体，支持消息显示）
QFrame *terminalCard = new QFrame();
terminalCard->setStyleSheet("background:#202538; border-radius:8px;");
QVBoxLayout *terminalLayout = new QVBoxLayout(terminalCard);
terminalLayout->setContentsMargins(8, 8, 8, 8);
QLabel *terminalTitle = new QLabel("控制指令");
terminalTitle->setStyleSheet("color:white; font-weight:bold;");
terminalLayout->addWidget(terminalTitle);
QLineEdit *terminalEdit = new QLineEdit();
terminalEdit->setPlaceholderText("输入命令...");
terminalEdit->setStyleSheet("color:white;");
terminalLayout->addWidget(terminalEdit);
QPushButton *sendBtn = new QPushButton("发送");
sendBtn->setStyleSheet("color:black; background:gray; border-radius:6px;");
terminalLayout->addWidget(sendBtn);

// 新增：显示已发送消息的标签
QLabel *terminalMsg = new QLabel();
terminalMsg->setStyleSheet("color:black; font-size:13px;");
terminalMsg->setAlignment(Qt::AlignLeft);
terminalLayout->addWidget(terminalMsg);

// 点击发送按钮时显示消息
connect(sendBtn, &QPushButton::clicked, this, [terminalEdit, terminalMsg]() {
    QString msg = terminalEdit->text();
    if (!msg.isEmpty()) {
        terminalMsg->setText("已发送: " + msg);
        terminalEdit->clear();
    }
});

customLayoutRow->addWidget(terminalCard);

    // 3. 故障信息卡片
    QFrame *faultCard = new QFrame();
    faultCard->setStyleSheet("background:#202538; border-radius:8px;");
    QVBoxLayout *faultLayout = new QVBoxLayout(faultCard);
    faultLayout->setContentsMargins(8, 8, 8, 8);
    QLabel *faultTitle = new QLabel("故障信息");
    faultTitle->setStyleSheet("color:white; font-weight:bold;");
    faultLayout->addWidget(faultTitle);
    QLabel *faultInfo = new QLabel("故障：泵体温度过高");
    faultInfo->setStyleSheet("color:#ff5555; font-size:13px;");
    faultLayout->addWidget(faultInfo);
    customLayoutRow->addWidget(faultCard);

    // === Assemble main layout ===
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(gaugeRowWidget);
    mainLayout->addWidget(ledRowWidget);
    mainLayout->addLayout(customLayoutRow);

    setStyleSheet("background-color:#0a0f1a;");

    // === TCP Socket setup ===
    socket = new QTcpSocket(this);

    // Connect signals BEFORE connectToHost
    connect(socket, &QTcpSocket::readyRead, this, &DeviceShower::onDataReceived);
    connect(socket, &QTcpSocket::connected, this, [](){
        qDebug() << "Connected to server!";
    });
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onSocketError(QAbstractSocket::SocketError)));

    // Change IP if server is on another computer
    socket->connectToHost("39.106.12.91", 8081); // <-- Set your server IP here
}

SensorData DeviceShower::parseSensorData(const QString& csv) {
    SensorData data;
    QStringList parts = csv.trimmed().split(",");
    if (parts.size() == 6) {
        data.pumpPressure = parts[0].toFloat();
        data.pumpVibration = parts[1].toFloat();
        data.motorCurrent = parts[2].toFloat();
        data.oilLevel = parts[3].toFloat();
        data.pumpTemp = parts[4].toFloat();
        data.flowRate = parts[5].toFloat();
    }
    return data;
}

void DeviceShower::onDataReceived() {
    while (socket->canReadLine()) {
        QString line = socket->readLine();
        qDebug() << "Received from server:" << line;
        SensorData data = parseSensorData(line);
        updateSensorData(data);
    }
}

void DeviceShower::updateSensorData(const SensorData& data) {
    // Gauge 0: Pressure
    // Gauge 1: Vibration
    // Gauge 2: Flow Rate
    if (gauges.size() >= 3) {
        gauges[0]->setValue(data.pumpPressure);
        gauges[1]->setValue(data.pumpVibration);
        gauges[2]->setValue(data.flowRate);
    }
    // LED 0: Motor Current
    // LED 1: Oil Level
    // LED 2: Pump Temp
    if (leds.size() >= 3) {
        leds[0]->setText(QString::number(data.motorCurrent, 'f', 1));
        leds[1]->setText(QString::number(data.oilLevel, 'f', 1));
        leds[2]->setText(QString::number(data.pumpTemp, 'f', 1));
    }
}
void DeviceShower::onSocketError(QAbstractSocket::SocketError socketError) {
    qDebug() << "Socket error:" << socketError << socket->errorString();
}
