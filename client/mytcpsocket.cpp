#include "mytcpsocket.h"
#include "netheader.h"
#include <QHostAddress>
#include <QtEndian>
#include <QMetaObject>
#include <QMutexLocker>
#include <QDateTime>

extern QUEUE_DATA<MESG> queue_send;
extern QUEUE_DATA<MESG> queue_recv;
extern QUEUE_DATA<MESG> audio_recv;




void MyTcpSocket::stopImmediately()
{
    {
        QMutexLocker lock(&m_lock);
        if(m_isCanRun == true) m_isCanRun = false;
    }
    //关闭read
    _sockThread->quit();
    _sockThread->wait();
}

void MyTcpSocket::closeSocket()
{
    if (_socktcp && _socktcp->isOpen())
    {
        _socktcp->close();
    }

    // 确保在正确的线程中停止定时器
    if (thread() == QThread::currentThread()) {
        if (_heartbeatTimer && _heartbeatTimer->isActive()) {
            _heartbeatTimer->stop();
        }
        if (_reconnectTimer && _reconnectTimer->isActive()) {
            _reconnectTimer->stop();
        }
    } else {
        QMetaObject::invokeMethod(this, [this]() {
            if (_heartbeatTimer && _heartbeatTimer->isActive()) {
                _heartbeatTimer->stop();
            }
            if (_reconnectTimer && _reconnectTimer->isActive()) {
                _reconnectTimer->stop();
            }
        }, Qt::QueuedConnection);
    }
}

void MyTcpSocket::sendHeartbeat()
{
    qDebug() << "[Heartbeat Debug] Entering sendHeartbeat function";
    qDebug() << "[Heartbeat Debug] _isConnected:" << _isConnected;
    qDebug() << "[Heartbeat Debug] _socktcp pointer:" << _socktcp;

    // 检查是否已连接
    if (!_isConnected || _socktcp == nullptr || _socktcp->state() != QAbstractSocket::ConnectedState) {
        //qDebug() << "[Heartbeat Debug] Conditions not met for sending heartbeat: _isConnected=" << _isConnected
        //         << ", socket state=" << (_socktcp ? _socktcp->state() : -1);
        return;
    }

    // 检查最后一次心跳发送时间，如果小于10秒，则跳过
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 timeDiff = currentTime - _lastHeartbeatTime;
    //qDebug() << "[Heartbeat Debug] Time since last heartbeat: " << timeDiff << "ms";

    // 如果超过10秒没有收到心跳响应，则认为连接断开
    if (timeDiff > 10000) {
        qDebug() << "[Heartbeat Error] No heartbeat response for over 10s, connection may be broken";
        _isConnected = false;

        // 确保在正确的线程中操作定时器
        if (thread() == QThread::currentThread()) {
            // 停止心跳定时器
            if (_heartbeatTimer && _heartbeatTimer->isActive()) {
                _heartbeatTimer->stop();
                qDebug() << "[Heartbeat Debug] Heartbeat timer stopped";
            }
            // 启动重连定时器，下次心跳周期(5秒)后会触发重连
            // 这样可以确保15秒(10+5)后再重连
            _reconnectTimer->start(5000);
        } else {
            QMetaObject::invokeMethod(this, [this]() {
                if (_heartbeatTimer && _heartbeatTimer->isActive()) {
                    _heartbeatTimer->stop();
                }
                _reconnectTimer->start(5000);
            }, Qt::QueuedConnection);
        }
        return;
    }

    // 创建心跳包
    MESG* heartbeatMsg = (MESG*)malloc(sizeof(MESG));
    if (heartbeatMsg == nullptr) {
        qDebug() << "[Heartbeat Error] Failed to allocate memory for heartbeat message";
        return;
    }

    // 填充心跳包数据
    memset(heartbeatMsg, 0, sizeof(MESG));
    heartbeatMsg->msg_type = HEARTBEAT;
    heartbeatMsg->len = 0;
    heartbeatMsg->data = nullptr;

    // 发送心跳包
    sendData(heartbeatMsg);

   // qDebug() << "[Heartbeat Debug] Heartbeat sent successfully";
    // 记录发送时间
    _lastHeartbeatSentTime = currentTime;
    //qDebug() << "[Heartbeat Debug] Recorded heartbeat send time: " << _lastHeartbeatSentTime;
}

MyTcpSocket::MyTcpSocket(QObject *par):QThread(par)
{
    qDebug() << "[Initialization Debug] Creating MyTcpSocket instance";

    qRegisterMetaType<QAbstractSocket::SocketError>();
    _socktcp = nullptr;

    _sockThread = new QThread(); //发送数据线程
    this->moveToThread(_sockThread);
    connect(_sockThread, SIGNAL(finished()), this, SLOT(closeSocket()));
    sendbuf =(uchar *) malloc(4 * MB);
    recvbuf = (uchar*)malloc(4 * MB);
    hasrecvive = 0;

    // 注意：定时器不能在此处创建，因为构造函数在主线程中运行
    // 但MyTcpSocket本身是一个QThread子类，会在另一个线程中运行
    _heartbeatTimer = nullptr;
    _reconnectTimer = nullptr;
    _reconnectInterval = 1;
    _lastHeartbeatTime = 0;

    _isConnected = false;
    qDebug() << "[Initialization Debug] Setting initial _isConnected to false";

    qDebug() << "[Initialization Debug] Setting initial _lastHeartbeatTime to 0";

    _roomNo = 0;

    // 启动线程，但定时器的创建和连接将在线程的run函数中完成
}


void MyTcpSocket::errorDetect(QAbstractSocket::SocketError error)
{
    qDebug() <<"Sock error" <<QThread::currentThreadId();
    MESG * msg = (MESG *) malloc(sizeof (MESG));
    if (msg == NULL)
    {
        qDebug() << "errdect malloc error";
    }
    else
    {
        memset(msg, 0, sizeof(MESG));
        if (error == QAbstractSocket::RemoteHostClosedError)
        {
            msg->msg_type = RemoteHostClosedError;
        }
        else
        {
            msg->msg_type = OtherNetError;
        }
        queue_recv.push_msg(msg);
    }

    // 如果连接错误且不是正在重连中，则启动重连
    if (_isConnected && !_reconnectTimer->isActive())
    {
        _isConnected = false;

        // 确保在正确的线程中操作定时器
        if (thread() == QThread::currentThread()) {
            if (_heartbeatTimer && _heartbeatTimer->isActive()) {
                _heartbeatTimer->stop();
            }
            _reconnectInterval = 1;
            _reconnectTimer->start(_reconnectInterval * 1000);
        } else {
            QMetaObject::invokeMethod(this, [this]() {
                if (_heartbeatTimer && _heartbeatTimer->isActive()) {
                    _heartbeatTimer->stop();
                }
                _reconnectInterval = 1;
                _reconnectTimer->start(_reconnectInterval * 1000);
            }, Qt::QueuedConnection);
        }

        qDebug() << "Network error detected, starting reconnect process";
    }
}

void MyTcpSocket::handleReconnect()
{
    qDebug() << "[Reconnect Debug] Entering handleReconnect";

    if (!_isConnected && !_serverIp.isEmpty() && !_serverPort.isEmpty())
    {
        qDebug() << "[Reconnect Debug] Not connected, attempting reconnection to" << _serverIp << ":" << _serverPort;

        // 先关闭当前连接
        if (_socktcp && _socktcp->isOpen())
        {
            qDebug() << "[Reconnect Debug] Closing and deleting existing socket";
            _socktcp->close();
            delete _socktcp;
            _socktcp = nullptr;
        }

        qDebug() << "[Reconnect Debug] Attempting to reconnect to server... (interval: " << _reconnectInterval << "s)";

        // 尝试重连
        if (connectServer(_serverIp, _serverPort, QIODevice::ReadWrite))
        {
            qDebug() << "[Reconnect Debug] Reconnection successful";

            // 发送重连信号
            emit reconnected();

            // 发送AUTH消息进行认证
            MESG* authMsg = (MESG*)malloc(sizeof(MESG));
            if (authMsg)
            {
                qDebug() << "[Reconnect Debug] Preparing AUTH message";
                memset(authMsg, 0, sizeof(MESG));
                authMsg->msg_type = AUTH;
                authMsg->len = 0;
                authMsg->data = nullptr;
                sendData(authMsg);
                qDebug() << "[Reconnect Debug] Sent AUTH message after reconnection";
            }

            // 如果之前在房间中，自动重新加入
            if (_roomNo > 0)
            {
                qDebug() << "[Reconnect Debug] Auto joining room" << _roomNo << "after reconnection";

                // 构造JOIN_MEETING消息
                MESG* joinMsg = (MESG*)malloc(sizeof(MESG));
                if (joinMsg)
                {
                    memset(joinMsg, 0, sizeof(MESG));
                    joinMsg->msg_type = JOIN_MEETING;
                    joinMsg->len = sizeof(int);
                    joinMsg->data = (uchar*)malloc(joinMsg->len);

                    if (joinMsg->data)
                    {
                        memcpy(joinMsg->data, &_roomNo, joinMsg->len);
                        sendData(joinMsg);
                        qDebug() << "[Reconnect Debug] Sent JOIN_MEETING message for room" << _roomNo;
                    }
                }
            }
        }
        else
        {
            qDebug() << "[Reconnect Debug] Reconnection failed, will try again";

            // 增加重连间隔，但不超过15秒
            _reconnectInterval *= 2;
            if (_reconnectInterval > 15)
            {
                _reconnectInterval = 15;
                qDebug() << "[Reconnect Debug] Reconnect interval capped at 15 seconds";
            }

            // 设置下次重连时间
            qDebug() << "[Reconnect Debug] Starting reconnect timer with interval" << _reconnectInterval * 1000 << "ms";

            // 确保在正确的线程中操作定时器
            if (thread() == QThread::currentThread()) {
                _reconnectTimer->start(_reconnectInterval * 1000);
            } else {
                QMetaObject::invokeMethod(this, [this]() {
                    _reconnectTimer->start(_reconnectInterval * 1000);
                }, Qt::QueuedConnection);
            }
        }
    }
    else
    {
        qDebug() << "[Reconnect Debug] Conditions not met for reconnection: isConnected=" << _isConnected
                 << ", serverIp empty=" << _serverIp.isEmpty()
                 << ", serverPort empty=" << _serverPort.isEmpty();
    }

    qDebug() << "[Reconnect Debug] Exiting handleReconnect";
}



void MyTcpSocket::sendData(MESG* send)
{
    if (_socktcp->state() == QAbstractSocket::UnconnectedState)
    {
        emit sendTextOver();
        if (send->data) free(send->data);
        if (send) free(send);
        return;
    }
    quint64 bytestowrite = 0;
    //构造消息头
    sendbuf[bytestowrite++] = '$';

    //消息类型
    qToBigEndian<quint16>(send->msg_type, sendbuf + bytestowrite);
    bytestowrite += 2;

    //发送者ip
    quint32 ip = _socktcp->localAddress().toIPv4Address();
    qToBigEndian<quint32>(ip, sendbuf + bytestowrite);
    bytestowrite += 4;

    // 对于所有消息类型都正确设置数据大小
    // 包括HEARTBEAT(10)、AUTH(12)等所有消息类型
    qToBigEndian<quint32>(send->len, sendbuf + bytestowrite);
    bytestowrite += 4;

    // 只有JOIN_MEETING需要特殊处理数据格式
    if (send->msg_type == JOIN_MEETING)
    {
        uint32_t room;
        memcpy(&room, send->data, send->len);
        qToBigEndian<quint32>(room, send->data);
    }

    //将数据拷入sendbuf
    memcpy(sendbuf + bytestowrite, send->data, send->len);
    bytestowrite += send->len;
    sendbuf[bytestowrite++] = '#'; //结尾字符

    //----------------write to server-------------------------
    qint64 hastowrite = bytestowrite;
    qint64 ret = 0, haswrite = 0;
    while ((ret = _socktcp->write((char*)sendbuf + haswrite, hastowrite - haswrite)) < hastowrite)
    {
        if (ret == -1 && _socktcp->error() == QAbstractSocket::TemporaryError)
        {
            ret = 0;
        }
        else if (ret == -1)
        {
            qDebug() << "network error";
            break;
        }
        haswrite += ret;
        hastowrite -= ret;
    }

    _socktcp->waitForBytesWritten();

    if(send->msg_type == TEXT_SEND)
    {
        emit sendTextOver(); //成功往内核发送文本信息
    }

    if (send->msg_type == KICK_OUT) {
        // 特殊处理：IP需要网络字节序
        quint32 ip;
        memcpy(&ip, send->data, sizeof(quint32));
        qToBigEndian<quint32>(ip, send->data);
    }


    if (send->data)
    {
        free(send->data);
    }
    //free
    if (send)
    {
        free(send);
    }
}

/*
 * 发送线程
 */
void MyTcpSocket::run()
{
    qDebug() << "[Thread Debug] MyTcpSocket thread started";
    //qDebug() << "send data" << QThread::currentThreadId();

    // 在当前线程中创建定时器（这是正确的做法，因为定时器必须在创建它的线程中启动和停止）
    // 不设置父对象，避免跨线程父对象错误
    if (_heartbeatTimer == nullptr) {
        qDebug() << "[Thread Debug] Creating _heartbeatTimer in MyTcpSocket thread";
        _heartbeatTimer = new QTimer(nullptr);
        // 确保定时器在正确的线程中
        _heartbeatTimer->setSingleShot(false);
        qDebug() << "[Timer Debug] Connecting _heartbeatTimer to sendHeartbeat() with Qt::UniqueConnection";
        connect(_heartbeatTimer, &QTimer::timeout, this, &MyTcpSocket::sendHeartbeat, Qt::UniqueConnection);
    } else {
        qDebug() << "[Thread Debug] _heartbeatTimer already exists, ensuring only one connection";
        // 断开所有已有连接，然后重新建立一个连接
        disconnect(_heartbeatTimer, &QTimer::timeout, this, &MyTcpSocket::sendHeartbeat);
        connect(_heartbeatTimer, &QTimer::timeout, this, &MyTcpSocket::sendHeartbeat, Qt::UniqueConnection);
    }
    if (_reconnectTimer == nullptr) {
        qDebug() << "[Thread Debug] Creating _reconnectTimer in MyTcpSocket thread";
        _reconnectTimer = new QTimer(nullptr);
        _reconnectTimer->setSingleShot(false);
        qDebug() << "[Timer Debug] Connecting _reconnectTimer to handleReconnect() with Qt::UniqueConnection";
        connect(_reconnectTimer, &QTimer::timeout, this, &MyTcpSocket::handleReconnect, Qt::UniqueConnection);
    } else {
        qDebug() << "[Thread Debug] _reconnectTimer already exists, ensuring only one connection";
        // 断开所有已有连接，然后重新建立一个连接
        disconnect(_reconnectTimer, &QTimer::timeout, this, &MyTcpSocket::handleReconnect);
        connect(_reconnectTimer, &QTimer::timeout, this, &MyTcpSocket::handleReconnect, Qt::UniqueConnection);
    }

    m_isCanRun = true; //标记可以运行
    /*
    *$_MSGType_IPV4_MSGSize_data_# //
    * 1 2 4 4 MSGSize 1
    *底层写数据线程
    */
    int counter = 0;
    for(;;)
    {
        {
            QMutexLocker locker(&m_lock);
            if(m_isCanRun == false)
            {
                qDebug() << "[Thread Debug] Thread stopping as m_isCanRun is false";
                // 停止并清理定时器
                if (_heartbeatTimer && _heartbeatTimer->isActive()) {
                    _heartbeatTimer->stop();
                }
                if (_reconnectTimer && _reconnectTimer->isActive()) {
                    _reconnectTimer->stop();
                }
                return; //在每次循环判断是否可以运行，如果不行就退出循环
            }
        }

        counter++;
        if (counter % 1000 == 0) { // 大约每10秒输出一次线程状态
            qDebug() << "[Thread Debug] Thread is running. Current status: _isConnected=" << _isConnected
                     << ", _heartbeatTimer active=" << (_heartbeatTimer ? _heartbeatTimer->isActive() : false)
                     << ", _lastHeartbeatTime=" << _lastHeartbeatTime;
        }

        //构造消息体
        MESG * send = queue_send.pop_msg();
        if(send == NULL)
        {
            msleep(10); // 防止CPU占用过高
            continue;
        }
        QMetaObject::invokeMethod(this, "sendData", Q_ARG(MESG*, send));
    }

    qDebug() << "[Thread Debug] MyTcpSocket thread stopped";
}


qint64 MyTcpSocket::readn(char * buf, quint64 maxsize, int n)
{
    quint64 hastoread = n;
    quint64 hasread = 0;
    do
    {
        qint64 ret  = _socktcp->read(buf + hasread, hastoread);
        if(ret < 0)
        {
            return -1;
        }
        if(ret == 0)
        {
            return hasread;
        }
        hasread += ret;
        hastoread -= ret;
    }while(hastoread > 0 && hasread < maxsize);
    return hasread;
}



MyTcpSocket::~MyTcpSocket()
{
    qDebug() << "[Destruction Debug] Starting MyTcpSocket destruction";

    // 停止线程运行
    qDebug() << "[Destruction Debug] Stopping thread";
    {
        QMutexLocker locker(&m_lock);
        m_isCanRun = false;
    }

    // 确保线程已经结束
    this->quit();
    this->wait();

    // 在主线程中安全地清理资源
    // 由于定时器创建在线程中，我们需要确保在正确的线程中停止和删除它们
    if (_heartbeatTimer != nullptr)
    {
        qDebug() << "[Destruction Debug] Cleaning up _heartbeatTimer";
        // 确保在当前线程中停止定时器
        if (thread() == QThread::currentThread()) {
            _heartbeatTimer->stop();
            delete _heartbeatTimer;
            _heartbeatTimer = nullptr;
        } else {
            // 通常不应该发生，因为我们已经wait()ed线程结束
            qDebug() << "[Destruction Debug] Warning: Timer cleanup in different thread";
            _heartbeatTimer->deleteLater();
            _heartbeatTimer = nullptr;
        }
    }

    if (_reconnectTimer != nullptr)
    {
        qDebug() << "[Destruction Debug] Cleaning up _reconnectTimer";
        // 确保在当前线程中停止定时器
        if (thread() == QThread::currentThread()) {
            _reconnectTimer->stop();
            delete _reconnectTimer;
            _reconnectTimer = nullptr;
        } else {
            // 通常不应该发生，因为我们已经wait()ed线程结束
            qDebug() << "[Destruction Debug] Warning: Timer cleanup in different thread";
            _reconnectTimer->deleteLater();
            _reconnectTimer = nullptr;
        }
    }

    if(_socktcp != nullptr)
    {
        qDebug() << "[Destruction Debug] Cleaning up _socktcp";
        _socktcp->close();
        delete _socktcp;
        _socktcp = nullptr;
    }

    if(sendbuf != nullptr)
    {
        qDebug() << "[Destruction Debug] Cleaning up sendbuf";
        delete [] sendbuf;
        sendbuf = nullptr;
    }

    if(recvbuf != nullptr)
    {
        qDebug() << "[Destruction Debug] Cleaning up recvbuf";
        delete [] recvbuf;
        recvbuf = nullptr;
    }

    if(_sockThread != nullptr)
    {
        qDebug() << "[Destruction Debug] Cleaning up _sockThread";
        _sockThread->quit();
        _sockThread->wait();
        delete _sockThread;
        _sockThread = nullptr;
    }

    qDebug() << "[Destruction Debug] MyTcpSocket destruction completed";
}



bool MyTcpSocket::connectServer(QString ip, QString port, QIODevice::OpenModeFlag flag)
{
    qDebug() << "[Connection Debug] Attempting to connect to server" << ip << ":" << port;

    // 保存服务器信息，用于重连
    _serverIp = ip;
    _serverPort = port;

    if(_socktcp == nullptr) {
        qDebug() << "[Connection Debug] Creating new QTcpSocket instance";
        _socktcp = new QTcpSocket(); //tcp
    }

    qDebug() << "[Connection Debug] Calling connectToHost";
    _socktcp->connectToHost(ip, port.toUShort(), flag);

    // 连接信号槽
    qDebug() << "[Connection Debug] Connecting signals and slots";
    connect(_socktcp, SIGNAL(readyRead()), this, SLOT(recvFromSocket()), Qt::UniqueConnection); //接受数据
    //处理套接字错误
    connect(_socktcp, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(errorDetect(QAbstractSocket::SocketError)),Qt::UniqueConnection);

    qDebug() << "[Connection Debug] Waiting for connection (5000ms timeout)";
    if(_socktcp->waitForConnected(5000))
    {
        qDebug() << "[Connection Debug] Connected to server successfully!";
        _isConnected = true;
        _lastHeartbeatTime = QDateTime::currentMSecsSinceEpoch();

        qDebug() << "[Connection Debug] Setting _lastHeartbeatTime to" << _lastHeartbeatTime;
        // 启动心跳定时器，每5秒发送一次心跳包
        qDebug() << "[Connection Debug] Starting heartbeat timer with interval 5000ms";

        // 确保在当前线程中启动定时器
        if (thread() == QThread::currentThread()) {
            // 如果当前就在MyTcpSocket线程中，直接启动定时器
            if (_heartbeatTimer) {
                _heartbeatTimer->start(5000);
            } else {
                qDebug() << "[Connection Debug] _heartbeatTimer not created yet, creating it now";
                _heartbeatTimer = new QTimer(nullptr);
                _heartbeatTimer->moveToThread(this->thread());
                connect(_heartbeatTimer, &QTimer::timeout, this, &MyTcpSocket::sendHeartbeat);
                _heartbeatTimer->start(5000);
            }
        } else {
            // 如果在其他线程中，使用信号槽机制在正确的线程中启动定时器
            qDebug() << "[Connection Debug] Not in MyTcpSocket thread, using invokeMethod to start timer";
            QMetaObject::invokeMethod(this, [this]() {
                if (!_heartbeatTimer) {
                    qDebug() << "[Connection Debug] Creating _heartbeatTimer in invokeMethod";
                    _heartbeatTimer = new QTimer(nullptr);
                    _heartbeatTimer->moveToThread(this->thread());
                    connect(_heartbeatTimer, &QTimer::timeout, this, &MyTcpSocket::sendHeartbeat);
                }
                _heartbeatTimer->start(5000);
            }, Qt::QueuedConnection);
        }

        // 确保重连定时器也已创建
        if (thread() == QThread::currentThread()) {
            if (!_reconnectTimer) {
                qDebug() << "[Connection Debug] _reconnectTimer not created yet, creating it now";
                _reconnectTimer = new QTimer(nullptr);
                _reconnectTimer->moveToThread(this->thread());
                connect(_reconnectTimer, &QTimer::timeout, this, &MyTcpSocket::handleReconnect);
            }
        } else {
            QMetaObject::invokeMethod(this, [this]() {
                if (!_reconnectTimer) {
                    qDebug() << "[Connection Debug] Creating _reconnectTimer in invokeMethod";
                    _reconnectTimer = new QTimer(nullptr);
                    _reconnectTimer->moveToThread(this->thread());
                    connect(_reconnectTimer, &QTimer::timeout, this, &MyTcpSocket::handleReconnect);
                }
            }, Qt::QueuedConnection);
        }

        // 停止重连定时器
        if (thread() == QThread::currentThread()) {
            if (_reconnectTimer && _reconnectTimer->isActive()) {
                qDebug() << "[Connection Debug] Stopping active reconnect timer";
                _reconnectTimer->stop();
            }
        } else {
            QMetaObject::invokeMethod(this, [this]() {
                if (_reconnectTimer && _reconnectTimer->isActive()) {
                    _reconnectTimer->stop();
                }
            }, Qt::QueuedConnection);
        }

        _reconnectInterval = 1; // 重置重连间隔

        qDebug() << "Connected to server, heartbeat started";
        return true;
    }
    _socktcp->close();
    return false;
}


bool MyTcpSocket::connectToServer(QString ip, QString port, QIODevice::OpenModeFlag flag)
{
    _sockThread->start(); // 开启链接，与接受
    bool retVal;
    QMetaObject::invokeMethod(this, "connectServer", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, retVal),
                              Q_ARG(QString, ip), Q_ARG(QString, port), Q_ARG(QIODevice::OpenModeFlag, flag));

    if (retVal)
    {
        this->start() ; //写数据
        return true;
    }
    else
    {
        return false;
    }
}

QString MyTcpSocket::errorString()
{
    return _socktcp->errorString();
}

void MyTcpSocket::disconnectFromHost()
{
    //write
    if(this->isRunning())
    {
        QMutexLocker locker(&m_lock);
        m_isCanRun = false;
    }

    if(_sockThread->isRunning()) //read
    {
        _sockThread->quit();
        _sockThread->wait();
    }

    //清空 发送 队列，清空接受队列
    queue_send.clear();
    queue_recv.clear();
    audio_recv.clear();
}


quint32 MyTcpSocket::getlocalip()
{
    if(_socktcp->isOpen())
    {
        return _socktcp->localAddress().toIPv4Address();
    }
    else
    {
        return -1;
    }
}

void MyTcpSocket::setRoomInfo(int roomNo)
{
    _roomNo = roomNo;
    qDebug() << "Room info set: " << roomNo;
}

int MyTcpSocket::getRoomNo() const
{
    return _roomNo;
}

void MyTcpSocket::recvFromSocket()
{
    //qDebug() << "recv data socket" <<QThread::currentThread();
    /*
    *$_msgtype_ip_size_data_#
    */
    qint64 availbytes = _socktcp->bytesAvailable();
    if (availbytes <=0 )
    {
        return;
    }
    qint64 ret = _socktcp->read((char *) recvbuf + hasrecvive, availbytes);
    if (ret <= 0)
    {
        qDebug() << "error or no more data";
        return;
    }
    hasrecvive += ret;

    // 查找消息起始标志 '$'，跳过前面的无效数据
    int startPos = 0;
    while (startPos < hasrecvive && recvbuf[startPos] != '$') {
        startPos++;
    }

    // 如果找不到 '$' 或者剩下的数据不够一个消息头，则返回
    if (startPos >= hasrecvive || hasrecvive - startPos < MSG_HEADER) {
        // 如果缓冲区中有数据但没有找到有效的消息开始，移动数据到缓冲区开头
        if (startPos > 0 && startPos < hasrecvive) {
            qDebug() << "[Buffer Cleanup] Skipping invalid bytes at start of buffer: " << startPos << " bytes";
            memmove_s(recvbuf, 4 * MB, recvbuf + startPos, hasrecvive - startPos);
            hasrecvive -= startPos;
        }
        return;
    }

    // 调整缓冲区指针，从真正的消息开始位置开始处理
    if (startPos > 0) {
        qDebug() << "[Buffer Cleanup] Found message start at position: " << startPos << ", adjusting buffer";
        memmove_s(recvbuf, 4 * MB, recvbuf + startPos, hasrecvive - startPos);
        hasrecvive -= startPos;
    }

    //数据包不够
    if (hasrecvive < MSG_HEADER)
    {
        return;
    }
    else
    {
        quint32 data_size;
        MSG_TYPE msgtype_local; // 使用局部变量避免重复读取类型
        uint16_t type_local;
        qFromBigEndian<uint16_t>(recvbuf + 1, 2, &type_local);
        msgtype_local = (MSG_TYPE)type_local;
        if (msgtype_local == CREATE_MEETING_RESPONSE || msgtype_local == PARTNER_JOIN2)
        {
            // 这些消息跳过IP字段，大小字段在位置7（msgType 2字节 + 4字节填充）
            qFromBigEndian<quint32>(recvbuf + 7, 4, &data_size);
        }
        else
        {
            // 其他消息有IP字段，大小字段在位置7
            qFromBigEndian<quint32>(recvbuf + 7, 4, &data_size);
        }

        qDebug() << "=== 消息解析 ===";
        qDebug() << "消息类型:" << msgtype_local;
        qDebug() << "数据长度:" << data_size;
        qDebug() << "已接收字节:" << hasrecvive;
        qDebug() << "需要字节:" << (quint64)data_size + 1 + MSG_HEADER;
        if ((quint64)data_size + 1 + MSG_HEADER <= hasrecvive) //收够一个包
        {
            qDebug() << "数据包完整，检查格式...";
            if (recvbuf[0] == '$' && recvbuf[MSG_HEADER + data_size] == '#') //且包结构正确
            {
                MSG_TYPE msgtype;
                uint16_t type;
                qFromBigEndian<uint16_t>(recvbuf + 1, 2, &type);
                msgtype = (MSG_TYPE)type;
                qDebug() << "recv data type: " << msgtype;

                // 处理心跳包确认
                if (msgtype == HEARTBEAT_ACK)
                {
                    qDebug() << "[Heartbeat Debug] Received HEARTBEAT_ACK from server";
                    // 更新心跳时间
                    qint64 previousHeartbeatTime = _lastHeartbeatTime;
                    _lastHeartbeatTime = QDateTime::currentMSecsSinceEpoch();
                    qDebug() << "[Heartbeat Debug] Updated _lastHeartbeatTime from" << previousHeartbeatTime << "to" << _lastHeartbeatTime;

                    // 计算心跳往返时间
                    if (_lastHeartbeatSentTime > 0) {
                        qint64 roundTripTime = _lastHeartbeatTime - _lastHeartbeatSentTime;
                        qDebug() << "[Heartbeat Debug] Heartbeat round trip time: " << roundTripTime << "ms";
                    }

                    // 如果之前标记为断开连接，则恢复连接状态
                    if (!_isConnected)
                    {
                        qDebug() << "[Heartbeat Debug] Connection was marked as disconnected, restoring connection state";
                        _isConnected = true;
                        qDebug() << "[Heartbeat Debug] Starting heartbeat timer with interval 5000ms";

                        // 确保在正确的线程中启动/停止定时器
                        if (thread() == QThread::currentThread()) {
                            if (_heartbeatTimer) {
                                _heartbeatTimer->start(5000);
                            }
                            if (_reconnectTimer && _reconnectTimer->isActive()) {
                                _reconnectTimer->stop();
                            }
                        } else {
                            QMetaObject::invokeMethod(this, [this]() {
                                if (_heartbeatTimer) {
                                    _heartbeatTimer->start(5000);
                                }
                                if (_reconnectTimer && _reconnectTimer->isActive()) {
                                    _reconnectTimer->stop();
                                }
                            }, Qt::QueuedConnection);
                        }

                        qDebug() << "Heartbeat acknowledged, connection restored";
                    }

                    // 移动缓冲区指针
                    qDebug() << "[Heartbeat Debug] Moving buffer pointer after HEARTBEAT_ACK processing";
                    memmove_s(recvbuf, 4 * MB, recvbuf + MSG_HEADER + data_size + 1,
                              hasrecvive - ((quint64)data_size + 1 + MSG_HEADER));
                    hasrecvive -= ((quint64)data_size + 1 + MSG_HEADER);
                    return; // 处理完心跳包后直接返回，继续接收下一个包
                }
                // 处理认证响应
                else if (msgtype == AUTH)
                {
                    // 收到AUTH响应，处理认证结果
                    qDebug() << "[Connection Debug] Received AUTH response, authentication successful";

                    // 重置连接状态
                    bool previousConnectedState = _isConnected;
                    _isConnected = true;
                    qDebug() << "[Connection Debug] Set _isConnected from" << previousConnectedState << "to true";

                    qint64 previousHeartbeatTime = _lastHeartbeatTime;
                    _lastHeartbeatTime = QDateTime::currentMSecsSinceEpoch();
                    qDebug() << "[Connection Debug] Set _lastHeartbeatTime from" << previousHeartbeatTime << "to" << _lastHeartbeatTime;

                    // 重启心跳计时器
                    if (thread() == QThread::currentThread()) {
                        if (_heartbeatTimer && !_heartbeatTimer->isActive())
                        {
                            qDebug() << "[Connection Debug] Heartbeat timer not active, starting with interval 5000ms";
                            _heartbeatTimer->start(5000); // 5秒发送一次心跳
                        } else {
                            qDebug() << "[Connection Debug] Heartbeat timer already active";
                        }
                    } else {
                        QMetaObject::invokeMethod(this, [this]() {
                            if (_heartbeatTimer && !_heartbeatTimer->isActive()) {
                                _heartbeatTimer->start(5000);
                            }
                        }, Qt::QueuedConnection);
                    }

                    // 移动缓冲区指针
                    qDebug() << "[Connection Debug] Moving buffer pointer after AUTH response processing";
                    memmove_s(recvbuf, 4 * MB, recvbuf + MSG_HEADER + data_size + 1,
                              hasrecvive - ((quint64)data_size + 1 + MSG_HEADER));
                    hasrecvive -= ((quint64)data_size + 1 + MSG_HEADER);
                    return; // 处理完认证响应后直接返回，继续接收下一个包
                }

                if (msgtype == CREATE_MEETING_RESPONSE || msgtype == JOIN_MEETING_RESPONSE || msgtype == PARTNER_JOIN2)
                {
                    if (msgtype == CREATE_MEETING_RESPONSE)
                    {
                        qDebug() << "=== 收到CREATE_MEETING_RESPONSE ===";
                        qDebug() << "数据长度:" << data_size;

                        // 打印原始数据
                        qDebug() << "原始数据:";
                        for(quint32 i = 0; i < data_size; i++) {
                            printf("%02X ", recvbuf[MSG_HEADER + i]);
                        }
                        printf("\n");

                        qint32 roomNo;
                        // 修复：CREATE_MEETING_RESPONSE跳过IP字段，数据从MSG_HEADER开始
                        // MSG_HEADER = 11，但CREATE_MEETING_RESPONSE的数据从位置11开始
                        qFromBigEndian<qint32>(recvbuf + MSG_HEADER, 4, &roomNo);
                        qDebug() << "解析的房间号:" << roomNo;

                        MESG* msg = (MESG*)malloc(sizeof(MESG));

                        if (msg == NULL)
                        {
                            qDebug() << __LINE__ << " CREATE_MEETING_RESPONSE malloc MESG failed";
                        }
                        else
                        {
                            memset(msg, 0, sizeof(MESG));
                            msg->msg_type = msgtype;
                            // 修复：使用固定的4字节长度，而不是data_size
                            msg->data = (uchar*)malloc(4);
                            if (msg->data == NULL)
                            {
                                free(msg);
                                qDebug() << __LINE__ << "CREATE_MEETING_RESPONSE malloc MESG.data failed";
                            }
                            else
                            {
                                memset(msg->data, 0, 4);
                                memcpy(msg->data, &roomNo, 4);
                                msg->len = 4;  // 修复：设置为4字节
                                qDebug() << "将房间号" << roomNo << "加入接收队列，数据长度:" << msg->len;
                                queue_recv.push_msg(msg);
                            }

                        }
                    }
                    else if (msgtype == JOIN_MEETING_RESPONSE)
                    {
                        qint32 c;
                        memcpy(&c, recvbuf + MSG_HEADER, data_size);

                        MESG* msg = (MESG*)malloc(sizeof(MESG));

                        if (msg == NULL)
                        {
                            qDebug() << __LINE__ << "JOIN_MEETING_RESPONSE malloc MESG failed";
                        }
                        else
                        {
                            memset(msg, 0, sizeof(MESG));
                            msg->msg_type = msgtype;
                            msg->data = (uchar*)malloc(data_size);
                            if (msg->data == NULL)
                            {
                                free(msg);
                                qDebug() << __LINE__ << "JOIN_MEETING_RESPONSE malloc MESG.data failed";
                            }
                            else
                            {
                                memset(msg->data, 0, data_size);
                                memcpy(msg->data, &c, data_size);

                                msg->len = data_size;
                                queue_recv.push_msg(msg);
                            }
                        }
                    }
                    else if (msgtype == PARTNER_JOIN2)
                    {
                        MESG* msg = (MESG*)malloc(sizeof(MESG));
                        if (msg == NULL)
                        {
                            qDebug() << "PARTNER_JOIN2 malloc MESG error";
                        }
                        else
                        {
                            memset(msg, 0, sizeof(MESG));
                            msg->msg_type = msgtype;
                            msg->len = data_size;
                            msg->data = (uchar*)malloc(data_size);
                            if (msg->data == NULL)
                            {
                                free(msg);
                                qDebug() << "PARTNER_JOIN2 malloc MESG.data error";
                            }
                            else
                            {
                                memset(msg->data, 0, data_size);
                                uint32_t ip;
                                int pos = 0;
                                for (size_t i = 0; i < data_size / sizeof(uint32_t); i++)
                                {
                                    qFromBigEndian<uint32_t>(recvbuf + MSG_HEADER + pos, sizeof(uint32_t), &ip);
                                    memcpy_s(msg->data + pos, data_size - pos, &ip, sizeof(uint32_t));
                                    pos += sizeof(uint32_t);
                                }
                                queue_recv.push_msg(msg);
                            }

                        }
                    }
                }
                else if (msgtype == IMG_RECV || msgtype == PARTNER_JOIN || msgtype == PARTNER_EXIT || msgtype == AUDIO_RECV || msgtype == CLOSE_CAMERA || msgtype == TEXT_RECV)
                {
                    //read ipv4
                    quint32 ip;
                    qFromBigEndian<quint32>(recvbuf + 3, 4, &ip);

                    if (msgtype == IMG_RECV)
                    {
                        //QString ss = QString::fromLatin1((char *)recvbuf + MSG_HEADER, data_len);
                        QByteArray cc((char *) recvbuf + MSG_HEADER, data_size);
                        QByteArray rc = QByteArray::fromBase64(cc);
                        QByteArray rdc = qUncompress(rc);
                        //将消息加入到接收队列
                        //                qDebug() << roomNo;

                        if (rdc.size() > 0)
                        {
                            MESG* msg = (MESG*)malloc(sizeof(MESG));
                            if (msg == NULL)
                            {
                                qDebug() << __LINE__ << " malloc failed";
                            }
                            else
                            {
                                memset(msg, 0, sizeof(MESG));
                                msg->msg_type = msgtype;
                                msg->data = (uchar*)malloc(rdc.size()); // 10 = format + width + width
                                if (msg->data == NULL)
                                {
                                    free(msg);
                                    qDebug() << __LINE__ << " malloc failed";
                                }
                                else
                                {
                                    memset(msg->data, 0, rdc.size());
                                    memcpy_s(msg->data, rdc.size(), rdc.data(), rdc.size());
                                    msg->len = rdc.size();
                                    msg->ip = ip;
                                    queue_recv.push_msg(msg);
                                }
                            }
                        }
                    }
                    else if (msgtype == PARTNER_JOIN || msgtype == PARTNER_EXIT || msgtype == CLOSE_CAMERA)
                    {
                        MESG* msg = (MESG*)malloc(sizeof(MESG));
                        if (msg == NULL)
                        {
                            qDebug() << __LINE__ << " malloc failed";
                        }
                        else
                        {
                            memset(msg, 0, sizeof(MESG));
                            msg->msg_type = msgtype;
                            msg->ip = ip;
                            queue_recv.push_msg(msg);
                        }
                    }
                    else if (msgtype == AUDIO_RECV)
                    {
                        //解压缩
                        QByteArray cc((char*)recvbuf + MSG_HEADER, data_size);
                        QByteArray rc = QByteArray::fromBase64(cc);
                        QByteArray rdc = qUncompress(rc);

                        if (rdc.size() > 0)
                        {
                            MESG* msg = (MESG*)malloc(sizeof(MESG));
                            if (msg == NULL)
                            {
                                qDebug() << __LINE__ << "malloc failed";
                            }
                            else
                            {
                                memset(msg, 0, sizeof(MESG));
                                msg->msg_type = AUDIO_RECV;
                                msg->ip = ip;

                                msg->data = (uchar*)malloc(rdc.size());
                                if (msg->data == nullptr)
                                {
                                    free(msg);
                                    qDebug() << __LINE__ << "malloc msg.data failed";
                                }
                                else
                                {
                                    memset(msg->data, 0, rdc.size());
                                    memcpy_s(msg->data, rdc.size(), rdc.data(), rdc.size());
                                    msg->len = rdc.size();
                                    msg->ip = ip;
                                    audio_recv.push_msg(msg);
                                }
                            }
                        }
                    }
                    else if(msgtype == TEXT_RECV)
                    {
                        //解压缩
                        QByteArray cc((char *)recvbuf + MSG_HEADER, data_size);
                        std::string rr = qUncompress(cc).toStdString();
                        if(rr.size() > 0)
                        {
                            MESG* msg = (MESG*)malloc(sizeof(MESG));
                            if (msg == NULL)
                            {
                                qDebug() << __LINE__ << "malloc failed";
                            }
                            else
                            {
                                memset(msg, 0, sizeof(MESG));
                                msg->msg_type = TEXT_RECV;
                                msg->ip = ip;
                                msg->data = (uchar*)malloc(rr.size());
                                if (msg->data == nullptr)
                                {
                                    free(msg);
                                    qDebug() << __LINE__ << "malloc msg.data failed";
                                }
                                else
                                {
                                    memset(msg->data, 0, rr.size());
                                    memcpy_s(msg->data, rr.size(), rr.data(), rr.size());
                                    msg->len = rr.size();
                                    queue_recv.push_msg(msg);
                                }
                            }
                        }
                    }
                }
                else if (msgtype == AUTH_REGISTER_OK) {
                    // 直接读取文本数据，不进行解压缩
                    QByteArray msgData(reinterpret_cast<const char*>(recvbuf + MSG_HEADER), data_size);
                    qDebug() << "注册成功响应:" << msgData;
                    emit registerSuccess(QString::fromUtf8(msgData));
                }
                else if (msgtype == AUTH_REGISTER_FAILED) {
                    // 直接读取文本数据，不进行解压缩
                    QByteArray errData(reinterpret_cast<const char*>(recvbuf + MSG_HEADER), data_size);
                    qDebug() << "注册失败响应:" << errData;
                    emit registerFailed(QString::fromUtf8(errData));
                }
                else if (msgtype == AUTH_OK_ADMIN) {
                    // 直接读取文本数据，不进行解压缩
                    QByteArray authData((char*)recvbuf + MSG_HEADER, data_size);
                    qDebug() << "登录成功响应:" << authData.toHex();
                    if(data_size == 0) {
                        qWarning() << "Empty role received, using fallback mechanism";
                    }
                    emit loginSuccess("admin");
                }
                else if (msgtype == AUTH_OK_FACTORY) {
                    /*
                    // 直接读取文本数据，不进行解压缩
                    QByteArray authData((char*)recvbuf + MSG_HEADER, data_size);
                    qDebug() << "登录成功响应:" << authData.toHex();
                    if(data_size == 0) {
                        qWarning() << "Empty role received, using fallback mechanism";
                    }
                    */
                    emit loginSuccess("factory");
                }
                else if (msgtype == AUTH_OK_EXPERT) {
                    // 直接读取文本数据，不进行解压缩
                    QByteArray authData((char*)recvbuf + MSG_HEADER, data_size);
                    qDebug() << "登录成功响应:" << authData.toHex();
                    if(data_size == 0) {
                        qWarning() << "Empty role received, using fallback mechanism";
                    }
                    emit loginSuccess("expert");
                }
                else if (msgtype == AUTH_FAILED) {
                    // 直接读取文本数据，不进行解压缩
                    QByteArray authData((char*)recvbuf + MSG_HEADER, data_size);
                    qDebug() << "登录失败响应:" << authData;
                    emit loginFailed(QString::fromUtf8(authData));
                }

            }
            else
            {
                // 增加详细的包错误调试信息
                qDebug() << "[Package Error] Invalid package format detected";
                qDebug() << "[Package Error] Buffer start character: '" << (char)recvbuf[0] << "' (expected '$')";
                qDebug() << "[Package Error] Buffer end character: '" << (char)(recvbuf[MSG_HEADER + data_size]) << "' (expected '#')";
                qDebug() << "[Package Error] Data size parsed: " << data_size;
                qDebug() << "[Package Error] Total bytes received: " << hasrecvive;
                qDebug() << "[Package Error] Required bytes for complete package: " << (quint64)data_size + 1 + MSG_HEADER;

                // 打印缓冲区前几个字节的十六进制表示，帮助调试
                qDebug() << "[Package Error] First 20 bytes of buffer (hex):";
                for (int i = 0; i < qMin(20, (int)hasrecvive); i++) {
                    qDebug() << QString("0x%1").arg((unsigned char)recvbuf[i], 2, 16, QLatin1Char('0'));
                }
            }
            memmove_s(recvbuf, 4 * MB, recvbuf + MSG_HEADER + data_size + 1, hasrecvive - ((quint64)data_size + 1 + MSG_HEADER));
            hasrecvive -= ((quint64)data_size + 1 + MSG_HEADER);
        }
        else
        {
            return;
        }
    }
}

// mytcpsocket.cpp
// 发送登录数据（含角色）
void MyTcpSocket::sendLoginData(const QString &username, const QString &password, const QString &role) {
    MESG *msg = (MESG*)malloc(sizeof(MESG));
    if (!msg) {
        qDebug() << "malloc MESG failed";
        return;
    }
    memset(msg, 0, sizeof(MESG));
    msg->msg_type = AUTH_B;

    // 数据格式："username|password|role"（支持邮箱作为username）
    QByteArray data = QString("%1|%2|%3").arg(username).arg(password).arg(role).toUtf8();
    msg->len = data.size();
    msg->data = (uchar*)malloc(msg->len);
    if (!msg->data) {
        free(msg);
        return;
    }
    memcpy(msg->data, data.data(), msg->len);
    // 添加IP地址信息（关键修复）
    msg->ip = this->getlocalip(); // 使用本地IP地址
    queue_send.push_msg(msg);
}

// 发送注册数据
void MyTcpSocket::sendRegisterData(const QString &username, const QString &password,
                                   const QString &email, const QString &role)
{
    MESG *msg = (MESG*)malloc(sizeof(MESG));
    if (!msg) return;
    memset(msg, 0, sizeof(MESG));
    msg->msg_type = AUTH_REGISTER;
    // 数据格式："username|password|email|role"
    QByteArray data = QString("%1|%2|%3|%4").arg(username).arg(password).arg(email).arg(role).toUtf8();
    msg->len = data.size();
    msg->data = (uchar*)malloc(msg->len);
    if (!msg->data) {
        free(msg);
        return;
    }
    memcpy(msg->data, data.data(), msg->len);

    // 添加IP地址信息（关键修复）
    msg->ip = this->getlocalip(); // 使用本地IP地址

    queue_send.push_msg(msg);
}
