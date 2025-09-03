#include "widget.h"
#include "ui_widget.h"
#include "screen.h"
#include <QCamera>
#include <QCameraViewfinder>
#include <QCameraImageCapture>
#include <QDebug>
#include <QPainter>
#include "myvideosurface.h"
#include "sendimg.h"
#include <QRegExp>
#include <QRegExpValidator>
#include <QMessageBox>
#include <QScrollBar>
#include <QHostAddress>
#include <QTextCodec>
#include "logqueue.h"
#include <QDateTime>
#include <QCompleter>
#include <QStringListModel>
#include <QSound>
#include <winsock2.h>
#include "deviceshower.h"
QRect  Widget::pos = QRect(-1, -1, -1, -1);

extern LogQueue *logqueue;
extern QUEUE_DATA<MESG> queue_send;

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{


    //开启日志线程
    logqueue = new LogQueue();
    logqueue->start();

    qDebug() << "main: " <<QThread::currentThread();
    qRegisterMetaType<MSG_TYPE>();

    WRITE_LOG("-------------------------Application Start---------------------------");
    WRITE_LOG("main UI thread id: 0x%p", QThread::currentThreadId());
    //ui界面
    _createmeet = false;
    _openCamera = false;
    _joinmeet = false;
    Widget::pos = QRect(0.1 * Screen::width, 0.1 * Screen::height, 0.8 * Screen::width, 0.8 * Screen::height);

    ui->setupUi(this);

    ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());

    this->setGeometry(Widget::pos);
    this->setMinimumSize(QSize(Widget::pos.width() * 0.7, Widget::pos.height() * 0.7));
    this->setMaximumSize(QSize(Widget::pos.width(), Widget::pos.height()));


    ui->exitmeetBtn->setDisabled(true);
    ui->joinmeetBtn->setDisabled(true);
    ui->createmeetBtn->setDisabled(true);
    ui->openAudio->setDisabled(true);
    ui->openVedio->setDisabled(true);
    ui->sendmsg->setDisabled(true);
    mainip = 0; //主屏幕显示的用户IP图像

    //-------------------局部线程----------------------------
    //创建传输视频帧线程
    _sendImg = new SendImg();
    _imgThread = new QThread();
    _sendImg->moveToThread(_imgThread); //新起线程接受视频帧
    _sendImg->start();
    //_imgThread->start();
    //处理每一帧数据

    //--------------------------------------------------


    //数据处理（局部线程）
    _mytcpSocket = new MyTcpSocket(); // 底层线程专管发送
    connect(_mytcpSocket, SIGNAL(sendTextOver()), this, SLOT(textSend()));
    connect(_mytcpSocket, SIGNAL(reconnected()), this, SLOT(onReconnected()));
    //connect(_mytcpSocket, SIGNAL(socketerror(QAbstractSocket::SocketError)), this, SLOT(mytcperror(QAbstractSocket::SocketError)));


    //----------------------------------------------------------
    //文本传输(局部线程)
    _sendText = new SendText();
    _textThread = new QThread();
    _sendText->moveToThread(_textThread);
    _textThread->start(); // 加入线程
    _sendText->start(); // 发送

    connect(this, SIGNAL(PushText(MSG_TYPE,QString)), _sendText, SLOT(push_Text(MSG_TYPE,QString)));
    //-----------------------------------------------------------

    //配置摄像头
    _camera = new QCamera(this);
    //摄像头出错处理
    connect(_camera, SIGNAL(error(QCamera::Error)), this, SLOT(cameraError(QCamera::Error)));
    _imagecapture = new QCameraImageCapture(_camera);
    _myvideosurface = new MyVideoSurface(this);


    connect(_myvideosurface, SIGNAL(frameAvailable(QVideoFrame)), this, SLOT(cameraImageCapture(QVideoFrame)));
    connect(this, SIGNAL(pushImg(QImage)), _sendImg, SLOT(ImageCapture(QImage)));


    //监听_imgThread退出信号
    connect(_imgThread, SIGNAL(finished()), _sendImg, SLOT(clearImgQueue()));


    //------------------启动接收数据线程-------------------------
    _recvThread = new RecvSolve();
    connect(_recvThread, SIGNAL(datarecv(MESG*)), this, SLOT(datasolve(MESG*)), Qt::QueuedConnection);
    _recvThread->start();

    //预览窗口重定向在MyVideoSurface
    _camera->setViewfinder(_myvideosurface);
    _camera->setCaptureMode(QCamera::CaptureStillImage);

    //音频
    _ainput = new AudioInput();
    _ainputThread = new QThread();
    _ainput->moveToThread(_ainputThread);


    _aoutput = new AudioOutput();
    _ainputThread->start(); //获取音频，发送
    _aoutput->start(); //播放

    connect(this, SIGNAL(startAudio()), _ainput, SLOT(startCollect()));
    connect(this, SIGNAL(stopAudio()), _ainput, SLOT(stopCollect()));
    connect(_ainput, SIGNAL(audioinputerror(QString)), this, SLOT(audioError(QString)));
    connect(_aoutput, SIGNAL(audiooutputerror(QString)), this, SLOT(audioError(QString)));
    connect(_aoutput, SIGNAL(speaker(QString)), this, SLOT(speaks(QString)));


    //设置滚动条
    ui->scrollArea->verticalScrollBar()->setStyleSheet("QScrollBar:vertical { width:8px; background:rgba(0,0,0,0%); margin:0px,0px,0px,0px; padding-top:9px; padding-bottom:9px; } QScrollBar::handle:vertical { width:8px; background:rgba(0,0,0,25%); border-radius:4px; min-height:20; } QScrollBar::handle:vertical:hover { width:8px; background:rgba(0,0,0,50%); border-radius:4px; min-height:20; } QScrollBar::add-line:vertical { height:9px;width:8px; border-image:url(:/images/a/3.png); subcontrol-position:bottom; } QScrollBar::sub-line:vertical { height:9px;width:8px; border-image:url(:/images/a/1.png); subcontrol-position:top; } QScrollBar::add-line:vertical:hover { height:9px;width:8px; border-image:url(:/images/a/4.png); subcontrol-position:bottom; } QScrollBar::sub-line:vertical:hover { height:9px;width:8px; border-image:url(:/images/a/2.png); subcontrol-position:top; } QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:rgba(0,0,0,10%); border-radius:4px; }");
    ui->listWidget->setStyleSheet("QScrollBar:vertical { width:8px; background:rgba(0,0,0,0%); margin:0px,0px,0px,0px; padding-top:9px; padding-bottom:9px; } QScrollBar::handle:vertical { width:8px; background:rgba(0,0,0,25%); border-radius:4px; min-height:20; } QScrollBar::handle:vertical:hover { width:8px; background:rgba(0,0,0,50%); border-radius:4px; min-height:20; } QScrollBar::add-line:vertical { height:9px;width:8px; border-image:url(:/images/a/3.png); subcontrol-position:bottom; } QScrollBar::sub-line:vertical { height:9px;width:8px; border-image:url(:/images/a/1.png); subcontrol-position:top; } QScrollBar::add-line:vertical:hover { height:9px;width:8px; border-image:url(:/images/a/4.png); subcontrol-position:bottom; } QScrollBar::sub-line:vertical:hover { height:9px;width:8px; border-image:url(:/images/a/2.png); subcontrol-position:top; } QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:rgba(0,0,0,10%); border-radius:4px; }");

    QFont te_font = this->font();
    te_font.setFamily("MicrosoftYaHei");
    te_font.setPointSize(12);

    ui->listWidget->setFont(te_font);

    ui->tabWidget->setCurrentIndex(1);
    ui->tabWidget->setCurrentIndex(0);
    if (ui->deviceMonitorBtn) ui->deviceMonitorBtn->setEnabled(false);
}


void Widget::cameraImageCapture(QVideoFrame frame)
{
    //    qDebug() << QThread::currentThreadId() << this;

    if(frame.isValid() && frame.isReadable())
    {
        QImage videoImg = QImage(frame.bits(), frame.width(), frame.height(), QVideoFrame::imageFormatFromPixelFormat(frame.pixelFormat()));

        QTransform matrix;
        matrix.rotate(180.0);

        QImage img =  videoImg.transformed(matrix, Qt::FastTransformation).scaled(ui->mainshow_label->size());

        if(partner.size() > 1)
        {
            emit pushImg(img);
        }

        if(_mytcpSocket->getlocalip() == mainip)
        {
            ui->mainshow_label->setPixmap(QPixmap::fromImage(img).scaled(ui->mainshow_label->size()));
        }

        Partner *p = partner[_mytcpSocket->getlocalip()];
        if(p) p->setpic(img);

        //qDebug()<< "format: " <<  videoImg.format() << "size: " << videoImg.size() << "byteSIze: "<< videoImg.sizeInBytes();
    }
    frame.unmap();
}

void Widget::on_deviceMonitorBtn_clicked()
{
    if (!m_deviceShower)
    {
        m_deviceShower = new DeviceShower(nullptr); // 顶级窗口
        m_deviceShower->setWindowFlag(Qt::Window, true);
        m_deviceShower->setWindowTitle("Sensor Monitor");
        m_deviceShower->setFixedSize(680, 480);
        m_deviceShower->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(m_deviceShower, &QObject::destroyed, this, [this]() { m_deviceShower = nullptr; });
    }
    m_deviceShower->show();
    m_deviceShower->raise();
    m_deviceShower->activateWindow();
}

Widget::~Widget()
{
    //终止底层发送与接收线程

    if(_mytcpSocket->isRunning())
    {
        _mytcpSocket->stopImmediately();
        _mytcpSocket->wait();
    }

    //终止接收处理线程
    if(_recvThread->isRunning())
    {
        _recvThread->stopImmediately();
        _recvThread->wait();
    }

    if(_imgThread->isRunning())
    {
        _imgThread->quit();
        _imgThread->wait();
    }

    if(_sendImg->isRunning())
    {
        _sendImg->stopImmediately();
        _sendImg->wait();
    }

    if(_textThread->isRunning())
    {
        _textThread->quit();
        _textThread->wait();
    }

    if(_sendText->isRunning())
    {
        _sendText->stopImmediately();
        _sendText->wait();
    }

    if (_ainputThread->isRunning())
    {
        _ainputThread->quit();
        _ainputThread->wait();
    }

    if (_aoutput->isRunning())
    {
        _aoutput->stopImmediately();
        _aoutput->wait();
    }
    WRITE_LOG("-------------------Application End-----------------");

    //关闭日志
    if(logqueue->isRunning())
    {
        logqueue->stopImmediately();
        logqueue->wait();
    }

    delete ui;
}

void Widget::on_createmeetBtn_clicked()
{
    if(m_userRole == "expert"){
        QMessageBox::information(this, "提示","无创建会议功能");
    }
    try {
        WRITE_LOG("Attempting to create meeting...");
        // 原有代码...
        if(false == _createmeet)
        {
            ui->createmeetBtn->setDisabled(true);
            ui->openAudio->setDisabled(true);
            ui->openVedio->setDisabled(true);
            ui->exitmeetBtn->setDisabled(true);
            emit PushText(CREATE_MEETING); //将 “创建会议"加入到发送队列
            WRITE_LOG("create meeting");
        }

    } catch(const std::exception& e) {
        WRITE_LOG("Exception in create meeting: %s", e.what());
        QMessageBox::critical(this, "Error", QString("Error: %1").arg(e.what()));
    }


}

void Widget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    /*
     * 触发事件(3条， 一般使用第二条进行触发)
     * 1. 窗口部件第一次显示时，系统会自动产生一个绘图事件。从而强制绘制这个窗口部件，主窗口起来会绘制一次
     * 2. 当重新调整窗口部件的大小时，系统也会产生一个绘制事件--QWidget::update()或者QWidget::repaint()
     * 3. 当窗口部件被其它窗口部件遮挡，然后又再次显示出来时，就会对那些隐藏的区域产生一个绘制事件
    */
}


//退出会议（1，加入的会议， 2，自己创建的会议）
void Widget::on_exitmeetBtn_clicked()
{
    if(_camera->status() == QCamera::ActiveStatus)
    {
        _camera->stop();
    }

    ui->createmeetBtn->setDisabled(true);
    ui->exitmeetBtn->setDisabled(true);
    _createmeet = false;
    _joinmeet = false;
    //-----------------------------------------
    //清空partner
    clearPartner();
    // 关闭套接字

    //关闭socket
    _mytcpSocket->disconnectFromHost();
    _mytcpSocket->wait();

    ui->outlog->setText(tr("已退出会议"));

    ui->connServer->setDisabled(false);
    ui->groupBox_2->setTitle(QString("主屏幕"));
    //    ui->groupBox->setTitle(QString("副屏幕"));
    //清除聊天记录
    while(ui->listWidget->count() > 0)
    {
        QListWidgetItem *item = ui->listWidget->takeItem(0);
        ChatMessage *chat = (ChatMessage *) ui->listWidget->itemWidget(item);
        delete item;
        delete chat;
    }
    iplist.clear();
    ui->plainTextEdit->setCompleter(iplist);


    WRITE_LOG("exit meeting");

    QMessageBox::warning(this, "Information", "退出会议" , QMessageBox::Yes, QMessageBox::Yes);

    //-----------------------------------------
}

void Widget::on_openVedio_clicked()
{
    if(_camera->status() == QCamera::ActiveStatus)
    {
        _camera->stop();
        WRITE_LOG("close camera");
        if(_camera->error() == QCamera::NoError)
        {
            _imgThread->quit();
            _imgThread->wait();
            ui->openVedio->setText("开启摄像头");
            emit PushText(CLOSE_CAMERA);
        }
        closeImg(_mytcpSocket->getlocalip());
    }
    else
    {
        _camera->start(); //开启摄像头
        WRITE_LOG("open camera");
        if(_camera->error() == QCamera::NoError)
        {
            _imgThread->start();
            ui->openVedio->setText("关闭摄像头");
        }
    }
}


void Widget::on_openAudio_clicked()
{
    if (!_createmeet && !_joinmeet) return;
    if (ui->openAudio->text().toUtf8() == QString(OPENAUDIO).toUtf8())
    {
        emit startAudio();
        ui->openAudio->setText(QString(CLOSEAUDIO).toUtf8());
    }
    else if(ui->openAudio->text().toUtf8() == QString(CLOSEAUDIO).toUtf8())
    {
        emit stopAudio();
        ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    }
}

void Widget::closeImg(quint32 ip)
{
    if (!partner.contains(ip))
    {
        qDebug() << "close img error";
        return;
    }
    Partner * p = partner[ip];
    p->setpic(QImage(":/myImage/1.jpg"));

    if(mainip == ip)
    {
        ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.jpg").scaled(ui->mainshow_label->size())));
    }
}

void Widget::on_connServer_clicked()
{
    QString ip = ui->ip->text(), port = ui->port->text();
    ui->outlog->setText("正在连接到" + ip + ":" + port);
    repaint();

    QRegExp ipreg("((2{2}[0-3]|2[01][0-9]|1[0-9]{2}|0?[1-9][0-9]|0{0,2}[1-9])\\.)((25[0-5]|2[0-4][0-9]|[01]?[0-9]{0,2})\\.){2}(25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})");

    QRegExp portreg("^([0-9]|[1-9]\\d|[1-9]\\d{2}|[1-9]\\d{3}|[1-5]\\d{4}|6[0-4]\\d{3}|65[0-4]\\d{2}|655[0-2]\\d|6553[0-5])$");
    QRegExpValidator ipvalidate(ipreg), portvalidate(portreg);
    int pos = 0;
    if(ipvalidate.validate(ip, pos) != QValidator::Acceptable)
    {
        QMessageBox::warning(this, "Input Error", "Ip Error", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }
    if(portvalidate.validate(port, pos) != QValidator::Acceptable)
    {
        QMessageBox::warning(this, "Input Error", "Port Error", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }

    if(_mytcpSocket ->connectToServer(ip, port, QIODevice::ReadWrite))
    {
        ui->outlog->setText("成功连接到" + ip + ":" + port);
        ui->openAudio->setDisabled(true);
        ui->openVedio->setDisabled(true);

        if(m_userRole == "factory"){
            ui->createmeetBtn->setDisabled(false);
        }else{
             ui->createmeetBtn->setDisabled(true);
            QMessageBox::information(this, "提示","无创建会议功能");
        }

        ui->exitmeetBtn->setDisabled(true);
        ui->joinmeetBtn->setDisabled(false);
        WRITE_LOG("succeeed connecting to %s:%s", ip.toStdString().c_str(), port.toStdString().c_str());
        QMessageBox::warning(this, "Connection success", "成功连接服务器" , QMessageBox::Yes, QMessageBox::Yes);
        ui->sendmsg->setDisabled(false);
        ui->connServer->setDisabled(true);
    }
    else
    {
        ui->outlog->setText("连接失败,请重新连接...");
        WRITE_LOG("failed to connenct %s:%s", ip.toStdString().c_str(), port.toStdString().c_str());
        QMessageBox::warning(this, "Connection error", _mytcpSocket->errorString() , QMessageBox::Yes, QMessageBox::Yes);
    }
}


void Widget::cameraError(QCamera::Error)
{
    QMessageBox::warning(this, "Camera error", _camera->errorString() , QMessageBox::Yes, QMessageBox::Yes);
}

void Widget::audioError(QString err)
{
    QMessageBox::warning(this, "Audio error", err, QMessageBox::Yes);
}

void Widget::datasolve(MESG *msg)
{
    if(msg->msg_type == CREATE_MEETING_RESPONSE)
    {
        int roomno;
        memcpy(&roomno, msg->data, msg->len);

        if(roomno != 0)
        {

            // 设置房主身份
            _isHost = true;


            // 保存房间信息，用于断线重连
            _mytcpSocket->setRoomInfo(roomno);

            QMessageBox::information(this, "Room No", QString("房间号：%1").arg(roomno), QMessageBox::Yes, QMessageBox::Yes);

            //ui->groupBox_2->setTitle(QString("主屏幕(房间号: %1)").arg(roomno));
            ui->outlog->setText(QString("创建成功 房间号: %1").arg(roomno) );
            _createmeet = true;
            ui->exitmeetBtn->setDisabled(false);
            ui->openVedio->setDisabled(false);
            ui->joinmeetBtn->setDisabled(true);

            WRITE_LOG("succeed creating room %d", roomno);
            //添加用户自己
            addPartner(_mytcpSocket->getlocalip());
            mainip = _mytcpSocket->getlocalip();
           ui->groupBox_2->setTitle(QString("主屏幕(房间号: %1) - %2").arg(roomno).arg(QHostAddress(mainip).toString()));
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.jpg").scaled(ui->mainshow_label->size())));
            if (ui->deviceMonitorBtn) ui->deviceMonitorBtn->setEnabled(true);
        }
        else
        {
            _createmeet = false;
            QMessageBox::information(this, "Room Information", QString("无可用房间"), QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("无可用房间"));
            ui->createmeetBtn->setDisabled(false);
            WRITE_LOG("no empty room");
        }
    }
    else if(msg->msg_type == JOIN_MEETING_RESPONSE)
    {
        qint32 c;
        memcpy(&c, msg->data, msg->len);
        if(c == 0)
        {
            QMessageBox::information(this, "Meeting Error", tr("会议不存在") , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("会议不存在"));
            WRITE_LOG("meeting not exist");
            ui->exitmeetBtn->setDisabled(true);
            ui->openVedio->setDisabled(true);
            ui->joinmeetBtn->setDisabled(false);
            ui->connServer->setDisabled(true);
            _joinmeet = false;
        }
        else if(c == -1)
        {
            QMessageBox::warning(this, "Meeting information", "成员已满，无法加入" , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("成员已满，无法加入"));
            WRITE_LOG("full room, cannot join");
        }
        else if (c > 0)
        {
            // 保存房间信息，用于断线重连
            int roomno = ui->meetno->text().toInt();
            _mytcpSocket->setRoomInfo(roomno);

            QMessageBox::warning(this, "Meeting information", "加入成功" , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("加入成功"));
            WRITE_LOG("succeed joining room");
            //添加用户自己
            addPartner(_mytcpSocket->getlocalip());
            mainip = _mytcpSocket->getlocalip();
            ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.jpg").scaled(ui->mainshow_label->size())));
            ui->joinmeetBtn->setDisabled(true);
            ui->exitmeetBtn->setDisabled(false);
            ui->createmeetBtn->setDisabled(true);
            _joinmeet = true;
            if (ui->deviceMonitorBtn) ui->deviceMonitorBtn->setEnabled(true);
        }
    }
    else if(msg->msg_type == IMG_RECV)
    {
        QHostAddress a(msg->ip);
        qDebug() << a.toString();
        QImage img;
        img.loadFromData(msg->data, msg->len);
        if(partner.count(msg->ip) == 1)
        {
            Partner* p = partner[msg->ip];
            p->setpic(img);
        }
        else
        {
            Partner* p = addPartner(msg->ip);
            p->setpic(img);
        }

        if(msg->ip == mainip)
        {
            ui->mainshow_label->setPixmap(QPixmap::fromImage(img).scaled(ui->mainshow_label->size()));
        }
        repaint();
    }
    else if(msg->msg_type == TEXT_RECV)
    {
        QString str = QString::fromStdString(std::string((char *)msg->data, msg->len));
        //qDebug() << str;
        QString time = QString::number(QDateTime::currentDateTimeUtc().toTime_t());
        ChatMessage *message = new ChatMessage(ui->listWidget);
        QListWidgetItem *item = new QListWidgetItem();
        dealMessageTime(time);
        dealMessage(message, item, str, time, QHostAddress(msg->ip).toString() ,ChatMessage::User_She);
        if(str.contains('@' + QHostAddress(_mytcpSocket->getlocalip()).toString()))
        {
            QSound::play(":/myEffect/2.wav");
        }
    }
    else if(msg->msg_type == PARTNER_JOIN)
    {
        Partner* p = addPartner(msg->ip);
        if(p)
        {
            p->setpic(QImage(":/myImage/1.jpg"));
            ui->outlog->setText(QString("%1 join meeting").arg(QHostAddress(msg->ip).toString()));
            iplist.append(QString("@") + QHostAddress(msg->ip).toString());
            ui->plainTextEdit->setCompleter(iplist);
        }
    }
    else if(msg->msg_type == PARTNER_EXIT)
    {
        removePartner(msg->ip);
        if(mainip == msg->ip)
        {
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.jpg").scaled(ui->mainshow_label->size())));
        }
        if(iplist.removeOne(QString("@") + QHostAddress(msg->ip).toString()))
        {
            ui->plainTextEdit->setCompleter(iplist);
        }
        else
        {
            qDebug() << QHostAddress(msg->ip).toString() << "not exist";
            WRITE_LOG("%s not exist",QHostAddress(msg->ip).toString().toStdString().c_str());
        }
        ui->outlog->setText(QString("%1 exit meeting").arg(QHostAddress(msg->ip).toString()));
    }
    else if (msg->msg_type == CLOSE_CAMERA)
    {
        closeImg(msg->ip);
    }
    else if (msg->msg_type == PARTNER_JOIN2)
    {
        uint32_t ip;
        int other = msg->len / sizeof(uint32_t), pos = 0;
        for (int i = 0; i < other; i++)
        {
            memcpy_s(&ip, sizeof(uint32_t), msg->data + pos , sizeof(uint32_t));
            pos += sizeof(uint32_t);
            Partner* p = addPartner(ip);
            if (p)
            {
                p->setpic(QImage(":/myImage/1.jpg"));
                iplist << QString("@") + QHostAddress(ip).toString();
            }
        }
        ui->plainTextEdit->setCompleter(iplist);
        ui->openVedio->setDisabled(false);
    }
    else if(msg->msg_type == RemoteHostClosedError)
    {

        clearPartner();
        _mytcpSocket->disconnectFromHost();
        _mytcpSocket->wait();
        ui->outlog->setText(QString("关闭与服务器的连接"));
        ui->createmeetBtn->setDisabled(true);
        ui->exitmeetBtn->setDisabled(true);
        ui->connServer->setDisabled(false);
        ui->joinmeetBtn->setDisabled(true);
        //清除聊天记录
        while(ui->listWidget->count() > 0)
        {
            QListWidgetItem *item = ui->listWidget->takeItem(0);
            ChatMessage *chat = (ChatMessage *)ui->listWidget->itemWidget(item);
            delete item;
            delete chat;
        }
        iplist.clear();
        ui->plainTextEdit->setCompleter(iplist);
        if(_createmeet || _joinmeet) QMessageBox::warning(this, "Meeting Information", "会议结束" , QMessageBox::Yes, QMessageBox::Yes);
        if (ui->deviceMonitorBtn) ui->deviceMonitorBtn->setEnabled(false);
    }
    else if(msg->msg_type == OtherNetError)
    {
        QMessageBox::warning(NULL, "Network Error", "网络异常" , QMessageBox::Yes, QMessageBox::Yes);
        clearPartner();
        _mytcpSocket->disconnectFromHost();
        _mytcpSocket->wait();
        ui->outlog->setText(QString("网络异常......"));
    }else if (msg->msg_type == KICK_OUT_RESPONSE) {
        handleKickResponse(msg);
    }
    else if (msg->msg_type == KICK_OUT_NOTIFY) {
        handleKickNotify(msg);
    }
    if(msg->data)
    {
        free(msg->data);
        msg->data = NULL;
    }
    if(msg)
    {
        free(msg);
        msg = NULL;
    }
}

void Widget::onReconnected()
{
    // 重连成功后更新UI状态
    ui->outlog->setText("成功重新连接到服务器");
    ui->createmeetBtn->setDisabled(false);
    ui->joinmeetBtn->setDisabled(false);
    ui->sendmsg->setDisabled(false);
    ui->connServer->setDisabled(true);

    // 如果之前在房间中，等待自动加入房间的响应
    int roomNo = _mytcpSocket->getRoomNo();
    if (roomNo > 0)
    {
        ui->outlog->setText(QString("正在重新加入房间 %1...").arg(roomNo));
    }

    WRITE_LOG("Reconnected to server successfully");
}

Partner* Widget::addPartner(quint32 ip)
{
    QMutexLocker locker(&partnerMutex); // 添加互斥锁保护partner容器的访问
    if (partner.contains(ip)) return NULL;

    // 判断是否是自己
    bool isSelf = (ip == _mytcpSocket->getlocalip());

    Partner *p = new Partner(ui->scrollAreaWidgetContents ,ip);
    if (p == NULL)
    {
        qDebug() << "new Partner error";
        return NULL;
    }
    else
    {

        // 在这里连接信号
        connect(p, &Partner::kickRequested, this, &Widget::handleKickRequest);

        connect(p, SIGNAL(sendip(quint32)), this, SLOT(recvip(quint32)));
        partner.insert(ip, p);
        ui->verticalLayout_3->addWidget(p, 1);

        //当有人员加入时，开启滑动条滑动事件，开启输入(只有自己时，不打开)
        if (partner.size() > 1)
        {
            connect(this, SIGNAL(volumnChange(int)), _ainput, SLOT(setVolumn(int)), Qt::UniqueConnection);
            connect(this, SIGNAL(volumnChange(int)), _aoutput, SLOT(setVolumn(int)), Qt::UniqueConnection);
            ui->openAudio->setDisabled(false);
            ui->sendmsg->setDisabled(false);
            _aoutput->startPlay();
        }
        return p;
    }
}

void Widget::removePartner(quint32 ip)
{
    QMutexLocker locker(&partnerMutex); // 添加互斥锁保护partner容器的访问
    if(partner.contains(ip))
    {
        Partner *p = partner[ip];
        disconnect(p, SIGNAL(sendip(quint32)), this, SLOT(recvip(quint32)));
        ui->verticalLayout_3->removeWidget(p);
        delete p;
        partner.remove(ip);

        //只有自已一个人时，关闭传输音频
        if (partner.size() <= 1)
        {
            disconnect(_ainput, SLOT(setVolumn(int)));
            disconnect(_aoutput, SLOT(setVolumn(int)));
            _ainput->stopCollect();
            _aoutput->stopPlay();
            ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
            ui->openAudio->setDisabled(true);
        }
    }
}

void Widget::clearPartner()
{
    QMutexLocker locker(&partnerMutex); // 添加互斥锁保护partner容器的访问
    ui->mainshow_label->setPixmap(QPixmap());
    if(partner.size() == 0) return;

    QMap<quint32, Partner*>::iterator iter =   partner.begin();
    while (iter != partner.end())
    {
        quint32 ip = iter.key();
        iter++;
        Partner *p =  partner.take(ip);
        ui->verticalLayout_3->removeWidget(p);
        delete p;
        p = nullptr;
    }

    //关闭传输音频
    disconnect(_ainput, SLOT(setVolumn(int)));
    disconnect(_aoutput, SLOT(setVolumn(int)));
    //关闭音频播放与采集
    _ainput->stopCollect();
    _aoutput->stopPlay();
    ui->openAudio->setText(QString(CLOSEAUDIO).toUtf8());
    ui->openAudio->setDisabled(true);


    //关闭图片传输线程
    if(_imgThread->isRunning())
    {
        _imgThread->quit();
        _imgThread->wait();
    }
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui->openVedio->setDisabled(true);
    if (ui->deviceMonitorBtn) ui->deviceMonitorBtn->setEnabled(false);
}

void Widget::recvip(quint32 ip)
{
    if (partner.contains(mainip))
    {
        Partner* p = partner[mainip];
        p->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(0, 0 , 255, 0.7)");
    }
    if (partner.contains(ip))
    {
        Partner* p = partner[ip];
        p->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(255, 0 , 0, 0.7)");
    }
    ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.jpg").scaled(ui->mainshow_label->size())));
    mainip = ip;
    ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
    qDebug() << ip;
}

/*
 * 加入会议
 */

void Widget::on_joinmeetBtn_clicked()
{
    QString roomNo = ui->meetno->text();

    QRegExp roomreg("^[1-9][0-9]{1,4}$");
    QRegExpValidator  roomvalidate(roomreg);
    int pos = 0;
    if(roomvalidate.validate(roomNo, pos) != QValidator::Acceptable)
    {
        QMessageBox::warning(this, "RoomNo Error", "房间号不合法" , QMessageBox::Yes, QMessageBox::Yes);
    }
    else
    {
        //加入发送队列
        emit PushText(JOIN_MEETING, roomNo);
    }
}

void Widget::on_horizontalSlider_valueChanged(int value)
{
    emit volumnChange(value);
}

void Widget::speaks(QString ip)
{
    ui->outlog->setText(QString(ip + " 正在说话").toUtf8());
}

void Widget::on_sendmsg_clicked()
{
    QString msg = ui->plainTextEdit->toPlainText().trimmed();
    if(msg.size() == 0)
    {
        qDebug() << "empty";
        return;
    }
    qDebug()<<msg;
    ui->plainTextEdit->setPlainText("");
    QString time = QString::number(QDateTime::currentDateTimeUtc().toTime_t());
    ChatMessage *message = new ChatMessage(ui->listWidget);
    QListWidgetItem *item = new QListWidgetItem();
    dealMessageTime(time);
    dealMessage(message, item, msg, time, QHostAddress(_mytcpSocket->getlocalip()).toString() ,ChatMessage::User_Me);
    emit PushText(TEXT_SEND, msg);
    ui->sendmsg->setDisabled(true);
}

void Widget::dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type)
{
    ui->listWidget->addItem(item);
    messageW->setFixedWidth(ui->listWidget->width());
    QSize size = messageW->fontRect(text);
    item->setSizeHint(size);
    messageW->setText(text, time, size, ip, type);
    ui->listWidget->setItemWidget(item, messageW);
}

void Widget::dealMessageTime(QString curMsgTime)
{
    bool isShowTime = false;
    if(ui->listWidget->count() > 0) {
        QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1);
        ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem);
        int lastTime = messageW->time().toInt();
        int curTime = curMsgTime.toInt();
        qDebug() << "curTime lastTime:" << curTime - lastTime;
        isShowTime = ((curTime - lastTime) > 60); // 两个消息相差一分钟
        //        isShowTime = true;
    } else {
        isShowTime = true;
    }
    if(isShowTime) {
        ChatMessage* messageTime = new ChatMessage(ui->listWidget);
        QListWidgetItem* itemTime = new QListWidgetItem();
        ui->listWidget->addItem(itemTime);
        QSize size = QSize(ui->listWidget->width() , 40);
        messageTime->resize(size);
        itemTime->setSizeHint(size);
        messageTime->setText(curMsgTime, curMsgTime, size);
        ui->listWidget->setItemWidget(itemTime, messageTime);
    }
}

void Widget::textSend()
{
    qDebug() << "send text over";
    QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1);
    ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem);
    messageW->setTextSuccess();
    ui->sendmsg->setDisabled(false);
}

void Widget::setTcpSocket(MyTcpSocket *socket)
{
    m_tcpSocket = socket; // 把外部的socket保存到类内
}
void Widget::setUserRole(const QString &role)
{
    m_userRole = role;

    // 根据角色设置界面元素权限
    if (role == "factory") {
        // 工厂角色可以创建房间
        ui->createmeetBtn->setEnabled(true);
        ui->joinmeetBtn->setEnabled(true);
    } else if (role == "expert") {
        // 专家角色只能加入房间
        ui->createmeetBtn->setEnabled(false);
        ui->joinmeetBtn->setEnabled(true);
    } else {
        // 未知角色，禁用所有功能
        ui->createmeetBtn->setEnabled(false);
        ui->joinmeetBtn->setEnabled(false);
    }
}

// 当用户选择踢人时
void Widget::handleKickRequest(quint32 ip) {
    if (!_isHost) {
        QMessageBox::warning(this, "权限不足", "只有房主可以踢人");
        return;
    }

    MESG* msg = (MESG*)malloc(sizeof(MESG));
    memset(msg, 0, sizeof(MESG));
    msg->msg_type = KICK_OUT;
    msg->len = sizeof(quint32);
    msg->data = (uchar*)malloc(msg->len);
    // 1. 将IP地址从主机字节序转换成网络字节序
    quint32 ip_net_order = htonl(ip);

    // 2. 将转换后的网络字节序IP复制到消息数据中
    memcpy(msg->data, &ip_net_order, msg->len);
    queue_send.push_msg(msg);

}

// 处理服务器对踢人请求的响应
void Widget::handleKickResponse(MESG* msg) {
    if (msg->len > 0) {
        int result = *(int*)msg->data;
        if (result == 1) {
            QMessageBox::information(this, "成功", "用户已被踢出");
        } else {
            QMessageBox::warning(this, "失败", "踢人操作失败");
        }
    }
}

// 处理服务器广播的踢人通知
void Widget::handleKickNotify(MESG* msg) {
    quint32 kickedIp;
    memcpy(&kickedIp, msg->data, sizeof(quint32));

    // 被踢的是自己
    if (kickedIp == _mytcpSocket->getlocalip()) {
        QMessageBox::information(this, "通知", "您已被踢出会议");
        on_exitmeetBtn_clicked(); // 执行退出会议流程
    }
    // 被踢的是其他用户
    else {
        removePartner(kickedIp);
        ui->outlog->setText(QString("%1 已被踢出会议").arg(QHostAddress(kickedIp).toString()));
    }
}
