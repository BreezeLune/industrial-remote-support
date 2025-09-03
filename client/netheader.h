#ifndef NETHEADER_H
#define NETHEADER_H
#include <QMetaType>
#include <QMutex>
#include <QQueue>
#include <QImage>
#include <QWaitCondition>
#define QUEUE_MAXSIZE 1500
#ifndef MB
#define MB 1024*1024
#endif

#ifndef KB
#define KB 1024
#endif

#ifndef WAITSECONDS
#define WAITSECONDS 2
#endif

#ifndef OPENVIDEO
#define OPENVIDEO "打开视频"
#endif

#ifndef CLOSEVIDEO
#define CLOSEVIDEO "关闭视频"
#endif

#ifndef OPENAUDIO
#define OPENAUDIO "打开音频"
#endif

#ifndef CLOSEAUDIO
#define CLOSEAUDIO "关闭音频"
#endif


#ifndef MSG_HEADER
#define MSG_HEADER 11
#endif

enum MSG_TYPE
{

    IMG_SEND = 0,
    IMG_RECV,
    AUDIO_SEND,
    AUDIO_RECV,
    TEXT_SEND,
    TEXT_RECV,
    CREATE_MEETING,
    EXIT_MEETING,
    JOIN_MEETING,
    CLOSE_CAMERA,
    AUTH_B,           // 认证请求
    AUTH_OK_FACTORY,
    AUTH_OK_EXPERT,
    AUTH_OK_ADMIN,       // 认证成功
    AUTH_FAILED,    // 认证失败
    AUTH_REGISTER,  // 注册请求
    AUTH_REGISTER_OK,    // 注册成功
    AUTH_REGISTER_FAILED,// 注册失败
    CREATE_MEETING_RESPONSE = 20,
    PARTNER_EXIT = 21,
    PARTNER_JOIN = 22,
    JOIN_MEETING_RESPONSE = 23,
    PARTNER_JOIN2 = 24,
    // 管理员审核相关（客户端自增定义，需与服务端一致）
    GET_PENDING_USERS = 60,
    APPROVE_USER = 61,
    REJECT_USER = 62,
    GET_PENDING_USERS_RESPONSE = 63,
    APPROVE_USER_RESPONSE = 64,
    REJECT_USER_RESPONSE = 65,
    //心跳类型
    HEARTBEAT = 99,       // 客户端-服务端
    HEARTBEAT_ACK = 98,    // 服务端-客户端
    AUTH = 97,
    RemoteHostClosedError = 40,
    OtherNetError = 41,
    KICK_OUT = 25,            // 踢人请求（客户端->服务器）
    KICK_OUT_RESPONSE = 26,    // 踢人响应（服务器->客户端）
    KICK_OUT_NOTIFY = 27       // 踢人通知（服务器->所有客户端）
};
Q_DECLARE_METATYPE(MSG_TYPE);
// 角色枚举
enum RoleType {
    ROLE_UNKNOWN,
    ROLE_FACTORY,
    ROLE_EXPERT,
    ROLE_ADMIN
};
struct MESG //消息结构体
{
    MSG_TYPE msg_type;
    uchar* data;
    long len;
    quint32 ip;
};
Q_DECLARE_METATYPE(MESG *);

//-------------------------------

template<class T>
struct QUEUE_DATA //消息队列
{
private:
    QMutex send_queueLock;
    QWaitCondition send_queueCond;
    QQueue<T*> send_queue;
public:
    void push_msg(T* msg)
    {
        send_queueLock.lock();
        while(send_queue.size() > QUEUE_MAXSIZE)
        {
            send_queueCond.wait(&send_queueLock);
        }
        send_queue.push_back(msg);
        send_queueLock.unlock();
        send_queueCond.wakeOne();
    }

    T* pop_msg()
    {
        send_queueLock.lock();
        while(send_queue.size() == 0)
        {
            bool f = send_queueCond.wait(&send_queueLock, WAITSECONDS * 1000);
            if(f == false)
            {
                send_queueLock.unlock();
                return NULL;
            }
        }
        T* send = send_queue.front();
        send_queue.pop_front();
        send_queueLock.unlock();
        send_queueCond.wakeOne();
        return send;
    }

    void clear()
    {
        send_queueLock.lock();
        send_queue.clear();
        send_queueLock.unlock();
    }
};

struct Log
{
    char *ptr;
    uint len;
};



void log_print(const char *, const char *, int, const char *, ...);
#define WRITE_LOG(LOGTEXT, ...) do{ \
    log_print(__FILE__, __FUNCTION__, __LINE__, LOGTEXT, ##__VA_ARGS__);\
}while(0);



#endif // NETHEADER_H
