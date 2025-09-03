#ifndef MYTCPSOCKET_H
#define MYTCPSOCKET_H

#include <QThread>
#include <QTcpSocket>
#include <QMutex>
#include <QTimer>
#include "netheader.h"
#ifndef MB
#define MB (1024 * 1024)
#endif

typedef unsigned char uchar;


class MyTcpSocket: public QThread
{
    Q_OBJECT
public:
    ~MyTcpSocket();
    MyTcpSocket(QObject *par=NULL);
    bool connectToServer(QString, QString, QIODevice::OpenModeFlag);
    QString errorString();
    void disconnectFromHost();
    quint32 getlocalip();
    void sendLoginData(const QString &username, const QString &password, const QString &role); // 重载登录函数
    void sendRegisterData(const QString &username, const QString &password,
                          const QString &email, const QString &role); // 新增注册函数
    // 设置房间信息，用于重连后恢复
    void setRoomInfo(int roomNo);
    int getRoomNo() const;

signals:
    // 登录验证结果信号（成功时返回角色，失败时返回错误信息）
   Q_INVOKABLE void loginSuccess(const QString &role);  // 登录成功，携带角色信息
   Q_INVOKABLE void loginFailed(const QString &errorMsg); // 登录失败，携带错误原因
   Q_INVOKABLE void registerSuccess(const QString &msg);  // 注册成功信号
   Q_INVOKABLE void registerFailed(const QString &errorMsg);  // 注册失败信号

private:
    void run() override;
    qint64 readn(char *, quint64, int);
    QTcpSocket *_socktcp;
    QThread *_sockThread;
    uchar *sendbuf;
    uchar* recvbuf;
    quint64 hasrecvive;

    QMutex m_lock;
    volatile bool m_isCanRun;
    // 心跳相关成员变量
    QTimer *_heartbeatTimer;
    QTimer *_reconnectTimer;
    int _reconnectInterval;
    qint64 _lastHeartbeatTime;  // 改为qint64类型以匹配QDateTime::currentMSecsSinceEpoch()的返回值
    qint64 _lastHeartbeatSentTime; // 记录心跳包发送时间，用于计算往返时间
    bool _isConnected;
    // 服务器信息
    QString _serverIp;
    QString _serverPort;

    // 房间信息
    int _roomNo;

private slots:
    bool connectServer(QString, QString, QIODevice::OpenModeFlag);
    void sendData(MESG *);
    void closeSocket();
    void sendHeartbeat();
    void handleReconnect();

public slots:
    void recvFromSocket();
    void stopImmediately();
    void errorDetect(QAbstractSocket::SocketError error);
signals:
    void socketerror(QAbstractSocket::SocketError);
    void sendTextOver();
    void reconnected();
    void startSendLoop();
    void sendLoop();
};

#endif // MYTCPSOCKET_H
