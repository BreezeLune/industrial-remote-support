#include "unpthread.h"
#include "msg.h"
#include "unp.h"
#include <map>
#include <vector>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <ctype.h>
#include <sys/ioctl.h>

#define SENDTHREADSIZE 5
SEND_QUEUE sendqueue; // 保存需要异步发送的消息

enum USER_TYPE {
    GUEST = 2,
    OWNER
};
//
static void dump_hex(const char* tag, const void* data, size_t len) {
    const unsigned char* p = (const unsigned char*)data;
    printf("[HEX] %s len=%zu :", tag, len);
    for (size_t i = 0; i < len; ++i) printf(" %02X", p[i]);
    printf("  |");
    for (size_t i = 0; i < len; ++i) printf("%c", isprint(p[i]) ? p[i] : '.');
    printf("|\n");
    fflush(stdout);
}

static void peek_socket(int fd) {
    unsigned char peekbuf[128];
    int avail = 0;
    ioctl(fd, FIONREAD, &avail);
    if (avail <= 0) { printf("[PEEK] fd=%d no pending bytes\n", fd); return; }
    if (avail > (int)sizeof(peekbuf)) avail = sizeof(peekbuf);
    int n = recv(fd, peekbuf, avail, MSG_PEEK);
    if (n > 0) dump_hex("SOCK-PEEK", peekbuf, n);
}
//
static volatile int maxfd = 0;
STATUS volatile roomstatus = ON;

typedef struct pool {
    fd_set fdset;
    pthread_mutex_t lock;
    int owner;
    int num;
    int status[1024 + 10];
    std::map<int, uint32_t> fdToIp;
    // 记录最近一次心跳/收包时间（秒）
    std::map<int, time_t> fdToLastSeen;

    pool() {
        memset(status, 0, sizeof(status));
        owner = 0;
        FD_ZERO(&fdset);
        lock = PTHREAD_MUTEX_INITIALIZER;
        num = 0;
    }

    void clear_room() {
        Pthread_mutex_lock(&lock);
        roomstatus = CLOSE;
        for (int i = 0; i <= maxfd; i++) {
            if (status[i] == ON) {
                Close(i);
            }
        }
        memset(status, 0, sizeof(status));
        num = 0;
        owner = 0;
        FD_ZERO(&fdset);
        fdToIp.clear();
        fdToLastSeen.clear();
        sendqueue.clear();
        Pthread_mutex_unlock(&lock);
    }
} Pool;

Pool * user_pool = new Pool();

// 前置声明
static void* accept_fd(void *arg);
static void* send_func(void *arg);
static void  fdclose(int fd, int pipefd);

void process_main(int i, int fd) { // room start
    printf("room %d starting \n", getpid());
    Signal(SIGPIPE, SIG_IGN);

    // 接收父进程传过来的客户端 fd 的线程
    pthread_t pfd;
    int *ptr = (int *)malloc(sizeof(int));
    *ptr = fd;
    Pthread_create(&pfd, NULL, accept_fd, ptr);

    // 异步发送线程池
    for (int k = 0; k < SENDTHREADSIZE; k++) {
        Pthread_create(&pfd, NULL, send_func, NULL);
    }

    const int HEARTBEAT_TIMEOUT = 15; // 心跳超时(秒)

    // 监听房间内所有已加入的 socket
    for (;;) {
        fd_set rset = user_pool->fdset;

        // 每次循环前都必须从 user_pool 重新获取 fd_set
        Pthread_mutex_lock(&user_pool->lock);
        rset = user_pool->fdset;
        Pthread_mutex_unlock(&user_pool->lock);

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int nsel = Select(maxfd + 1, &rset, NULL, NULL, &tv);

        if (nsel == 0) {
            // 周期性心跳检测：收集超时 fd，避免持锁内调用 fdclose 造成死锁
            std::vector<int> to_close;
            time_t now = time(NULL);

            Pthread_mutex_lock(&user_pool->lock);
            for (int k = 0; k <= maxfd; k++) {
                if (user_pool->status[k] == ON) {
                    auto it = user_pool->fdToLastSeen.find(k);
                    if (it != user_pool->fdToLastSeen.end() &&
                        (now - it->second) > HEARTBEAT_TIMEOUT) {
                        to_close.push_back(k);
                    }
                }
            }
            Pthread_mutex_unlock(&user_pool->lock);

            for (int fd_to_close : to_close) {
                printf("Client %d timed out. Closing connection.\n", fd_to_close);
                fdclose(fd_to_close, fd);
            }
            continue;
        }

        for (int cfd = 0; cfd <= maxfd; cfd++) {
            if (!FD_ISSET(cfd, &rset)) continue;

            // 任何可读事件都更新“最近活跃时间”
            Pthread_mutex_lock(&user_pool->lock);
            user_pool->fdToLastSeen[cfd] = time(NULL);
            Pthread_mutex_unlock(&user_pool->lock);

            char head[15] = {0};
            int ret = Readn(cfd, head, 11); // 头部固定 11 字节: '$' 2B(type) 4B(ip) 4B(len)

            if (ret <= 0) {
                if (ret == 0) {
                    printf("[ROOM INFO] Client fd=%d closed connection gracefully.\n", cfd);
                } else {
                    perror("[ROOM ERR] Readn failed");
                }
                fdclose(cfd, fd);
                if (--nsel <= 0) break;
                continue; // 处理下一个有事件的fd
            }
            printf("[ROOM DBG] fd=%d Readn ret=%zd\n", cfd, ret);
            dump_hex("ROOM HEAD", head, ret);
            // 检查头部格式
            if (ret < 11 || head[0] != '$') {
                printf("[ROOM ERR] fd=%d received malformed header. Closing connection.\n", cfd);
                fdclose(cfd, fd);
                if (--nsel <= 0) break;
                continue;
            }
            // if (ret <= 0) {
            //     printf("peer close\n");
            //     fdclose(cfd, fd);
            // } else if (ret == 11) 
            
            
                

                // 解析类型/长度
                MSG_TYPE msgtype;
                memcpy(&msgtype, head + 1, 2);
                msgtype = (MSG_TYPE)ntohs(msgtype);

                uint32_t msglen_net;
                memcpy(&msglen_net, head + 7, 4);
                uint32_t body_len = ntohl(msglen_net);

                // 心跳：读取尾部'#'后返回 ACK
                if (msgtype == HEARTBEAT) {
                    char tail;
                    // 只读一次，并检查返回值
                    if (Readn(cfd, &tail, 1) > 0 && tail == '#') {
                        MSG ack_msg;
                        memset(&ack_msg, 0, sizeof(MSG));
                        ack_msg.msgType  = HEARTBEAT_ACK;
                        ack_msg.targetfd = cfd;
                        sendqueue.push_msg(ack_msg);
                        printf("sent heartbeat_ack for fd %d\n", cfd);
                    } else {
                        printf("[ROOM ERR] fd=%d HEARTBEAT format error or read failed. Closing.\n", cfd);
                        fdclose(cfd, fd);
                    }
                    if (--nsel <= 0) break;
                    continue; // 心跳包处理完毕
                } else if (msgtype == KICK_OUT) {
                    // 1. 读取被踢成员的IP（网络序）并尾部 '#'
                    uint32_t kick_ip;
                    char tail;
                    if (Readn(cfd, &kick_ip, 4) != 4 || Readn(cfd, &tail, 1) != 1 || tail != '#') {
                        printf("[ROOM ERR] KICK_OUT format error\n");
                        continue;
                    }

                    // 2. 验证操作者是否为房主
                    if (cfd != user_pool->owner) {
                        MSG resp; memset(&resp, 0, sizeof(MSG));
                        resp.msgType = KICK_OUT_RESPONSE; resp.targetfd = cfd;
                        uint32_t status = 0; // 0=失败
                        resp.ptr = (char*)malloc(sizeof(status));
                        memcpy(resp.ptr, &status, sizeof(status));
                        resp.len = sizeof(status);
                        sendqueue.push_msg(resp);
                        continue;
                    }

                    // 3. 查找目标IP对应的fd
                    int kick_fd = -1;
                    Pthread_mutex_lock(&user_pool->lock);
                    for (const auto& pair : user_pool->fdToIp) {
                        if (pair.second == kick_ip) { kick_fd = pair.first; break; }
                    }
                    Pthread_mutex_unlock(&user_pool->lock);

                    // 4. 广播通知
                    if (kick_fd != -1) {
                        for (int i = 0; i <= maxfd; i++) {
                            Pthread_mutex_lock(&user_pool->lock);
                            bool alive = (user_pool->status[i] == ON);
                            Pthread_mutex_unlock(&user_pool->lock);
                            if (!alive) continue;
                            MSG notify; memset(&notify, 0, sizeof(MSG));
                            notify.msgType = KICK_OUT_NOTIFY; notify.targetfd = i;
                            notify.ptr = (char*)malloc(sizeof(kick_ip));
                            memcpy(notify.ptr, &kick_ip, sizeof(kick_ip));
                            notify.len = sizeof(kick_ip);
                            sendqueue.push_msg(notify);
                        }
                        // 可选：直接关闭被踢者连接
                        // fdclose(kick_fd, fd);
                    }

                    // 5. 返回响应
                    {
                        MSG resp; memset(&resp, 0, sizeof(MSG));
                        resp.msgType = KICK_OUT_RESPONSE; resp.targetfd = cfd;
                        uint32_t status = (kick_fd == -1) ? 0 : 1;
                        resp.ptr = (char*)malloc(sizeof(status));
                        memcpy(resp.ptr, &status, sizeof(status));
                        resp.len = sizeof(status);
                        sendqueue.push_msg(resp);
                    }
                    continue;
                }

                // 解析来源 ip（网络序）
                MSG msg;
                memset(&msg, 0, sizeof(MSG));
                msg.targetfd = cfd;
                memcpy(&msg.ip, head + 3, 4);
                msg.len = body_len;

                if (msgtype == IMG_SEND || msgtype == AUDIO_SEND || msgtype == TEXT_SEND) {
                    msg.msgType = (msgtype == IMG_SEND) ? IMG_RECV :
                                  (msgtype == AUDIO_SEND ? AUDIO_RECV : TEXT_RECV);
                    msg.ptr = (char *)malloc(msg.len);
                    // 统一用我们记录的 fd->ip（网络序）
                    msg.ip = user_pool->fdToIp[cfd];

                    char tail;
                    // 【重要修正 3】: 检查读取消息体和结尾符是否成功
                    if (Readn(cfd, msg.ptr, msg.len) == (ssize_t)msg.len && Readn(cfd, &tail, 1) > 0 && tail == '#') {
                        sendqueue.push_msg(msg);
                    } else {
                        printf("[ROOM ERR] fd=%d failed to read message body/tail. Closing.\n", cfd);
                        free(msg.ptr); // 【重要修正 4】: 错误时必须释放内存
                        fdclose(cfd, fd);
                    }
                } else if (msgtype == CLOSE_CAMERA) {
                    char tail;
                    Readn(cfd, &tail, 1);
                    if (tail == '#' && msg.len == 0) {
                        msg.msgType = CLOSE_CAMERA;
                        sendqueue.push_msg(msg);
                    } else {
                        err_msg("camera data error");
                    }
                } 




                // else if(msgtype == CLOSE_CAMERA)
                //         {
                //             char tail;
                //             Readn(i, &tail, 1);
                //             if(tail == '#' && msg.len == 0)
                //             {
                //                 msg.msgType = CLOSE_CAMERA;
                //                 sendqueue.push_msg(msg);
                //             }
                //             else
                //             {
                //                 err_msg("camera data error ");
                //             }
                //         }
                        else if(msgtype == HEARTBEAT)
                        {
                            // 处理心跳包
                            char tail;
                            Readn(cfd, &tail, 1);
                            if(tail == '#' && msg.len == 0)
                            {
                                // 构造并发送心跳确认包
                                MSG heartBeatAck;
                                memset(&heartBeatAck, 0, sizeof(MSG));
                                heartBeatAck.msgType = HEARTBEAT_ACK;
                                heartBeatAck.len = 0;
                                heartBeatAck.ptr = NULL;
                                heartBeatAck.targetfd = cfd;
                                
                                // 直接发送心跳确认，不经过sendqueue
                                char *buf = (char *) malloc(100);
                                memset(buf, 0, 100);
                                int bytestowrite = 0;
                                buf[bytestowrite++] = '$';
                                printf("sent heartbeat_ack");
                                uint16_t type = heartBeatAck.msgType;
                                type = htons(type);
                                memcpy(buf + bytestowrite, &type, sizeof(uint16_t));
                                bytestowrite += 2;
                                
                                // 填充IP字段为0.0.0.0
                                uint32_t server_ip = 0;
                                server_ip = htonl(server_ip);
                                memcpy(buf + bytestowrite, &server_ip, sizeof(uint32_t));
                                bytestowrite += 4;
                                
                                uint32_t size = heartBeatAck.len;
                                size = htonl(size);
                                memcpy(buf + bytestowrite, &size, sizeof(uint32_t));
                                bytestowrite += 4;
                                
                                buf[bytestowrite++] = '#';
                                
                                if(writen(cfd, buf, bytestowrite) < bytestowrite)
                                {
                                    printf("Heartbeat ACK write fail\n");
                                }
                                else
                                {
                                    printf("Room process: Sent HEARTBEAT_ACK to client fd %d\n", cfd);
                                }
                                
                                free(buf);
                            }
                            else
                            {
                                err_msg("Heartbeat data format error");
                            }
                        }
                        else if(msgtype == AUTH)
                        {
                            // 处理认证消息
                            char tail;
                            if (Readn(cfd, &tail, 1) > 0 && tail == '#' && body_len == 0) {
                                MSG ack_msg; memset(&ack_msg, 0, sizeof(MSG));
                                ack_msg.msgType = HEARTBEAT_ACK; // 复用心跳ACK
                                ack_msg.targetfd = cfd;
                                sendqueue.push_msg(ack_msg);
                            } else {
                                printf("[ROOM ERR] fd=%d AUTH format error. Closing.\n", cfd);
                                fdclose(cfd, fd);
                            }
                        }
                else {
                    printf("[ROOM WARN] fd=%d sent unsupported msg type %d. Draining %u bytes.\n", cfd, msgtype, body_len);
                    // 安全地丢弃未知消息的数据体
                    char drain_buf[1024];
                    uint32_t remaining = body_len;
                    bool drain_ok = true;
                    while (remaining > 0) {
                        size_t to_read = (remaining > sizeof(drain_buf)) ? sizeof(drain_buf) : remaining;
                        if (Readn(cfd, drain_buf, to_read) <= 0) {
                            drain_ok = false;
                            break;
                        }
                        remaining -= to_read;
                    }
                    // 丢弃最后的'#'
                    char tail;
                    if (drain_ok) Readn(cfd, &tail, 1);
                    
                    if (!drain_ok) {
                        printf("[ROOM ERR] fd=%d connection closed while draining. Closing.\n", cfd);
                        fdclose(cfd, fd);
                    }
                }

                if (--nsel <= 0) break;
            } 
        
    }
}

// 关闭某个客户端 fd
static void fdclose(int fd, int pipefd) {
    if (user_pool->owner == fd) { // 房主关闭 → 房间关闭
        user_pool->clear_room();
        printf("clear room\n");
        // 通知父进程房间空闲
        char cmd = 'E';
        if (writen(pipefd, &cmd, 1) < 1) {
            err_msg("writen error");
        }
    } else {
        uint32_t ip;

        Pthread_mutex_lock(&user_pool->lock);
        ip = user_pool->fdToIp[fd];
        user_pool->fdToIp.erase(fd);
        user_pool->fdToLastSeen.erase(fd);

        FD_CLR(fd, &user_pool->fdset);
        user_pool->num--;
        user_pool->status[fd] = CLOSE;
        if (fd == maxfd) maxfd--;
        Pthread_mutex_unlock(&user_pool->lock);

        // 通知父进程：有成员退出
        char cmd = 'Q';
        if (writen(pipefd, &cmd, 1) < 1) {
            err_msg("write error");
        }

        // 广播 PARTNER_EXIT
        MSG msg;
        memset(&msg, 0, sizeof(MSG));
        msg.msgType = PARTNER_EXIT;
        msg.targetfd = -1;
        msg.ip = ip; // 网络序
        Close(fd);
        sendqueue.push_msg(msg);
    }
}

// 接收父进程传来的客户端 fd（通过 UNIX 域套接字 SCM_RIGHTS）
static void* accept_fd(void *arg) {
    uint32_t getpeerip(int);
    Pthread_detach(pthread_self());
    int fd = *(int *)arg, tfd = -1;
    free(arg);

    for (;;) {
        int n, c;
        if ((n = read_fd(fd, &c, 1, &tfd)) <= 0) {
            err_quit("read_fd error");
        }
        if (tfd < 0) {
            printf("c = %c\n", c);
            err_quit("no descriptor from read_fd");
        }

        if (c == 'C') { // create: 设置房主
            Pthread_mutex_lock(&user_pool->lock);

            FD_SET(tfd, &user_pool->fdset);
            user_pool->owner = tfd;
            user_pool->fdToIp[tfd] = getpeerip(tfd); // 网络序
            user_pool->num++;
            user_pool->status[tfd] = ON;
            user_pool->fdToLastSeen[tfd] = time(NULL); // 初始化心跳时间
            maxfd = MAX(maxfd, tfd);
            roomstatus = ON;

            Pthread_mutex_unlock(&user_pool->lock);

            // 回应房间号（使用网络序）
            MSG msg;
            memset(&msg, 0, sizeof(MSG));
            msg.msgType = CREATE_MEETING_RESPONSE;
            msg.targetfd = tfd;
            int roomNo = htonl(getpid());
            msg.ptr = (char *)malloc(sizeof(int));
            memcpy(msg.ptr, &roomNo, sizeof(int));
            msg.len = sizeof(int);
            sendqueue.push_msg(msg);
        } else if (c == 'J') { // join: 普通成员
            Pthread_mutex_lock(&user_pool->lock);
            if (roomstatus == CLOSE) {
                close(tfd);
                Pthread_mutex_unlock(&user_pool->lock);
                continue;
            }

            FD_SET(tfd, &user_pool->fdset);
            user_pool->num++;
            user_pool->status[tfd] = ON;
            user_pool->fdToLastSeen[tfd] = time(NULL);
            maxfd = MAX(maxfd, tfd);
            user_pool->fdToIp[tfd] = getpeerip(tfd);
            Pthread_mutex_unlock(&user_pool->lock);

            // 广播新成员加入（PARTNER_JOIN）
            MSG msg;
            memset(&msg, 0, sizeof(MSG));
            msg.msgType = PARTNER_JOIN;
            msg.ptr = NULL;
            msg.len = 0;
            msg.targetfd = tfd;
            msg.ip = user_pool->fdToIp[tfd];
            sendqueue.push_msg(msg);

            // 给新成员发已有成员列表（PARTNER_JOIN2）
            MSG msg1;
            memset(&msg1, 0, sizeof(MSG));
            msg1.msgType = PARTNER_JOIN2;
            msg1.targetfd = tfd;

            // 组装当前在线成员（除自己）
            int size = 0;
            std::vector<uint32_t> ips;
            Pthread_mutex_lock(&user_pool->lock);
            for (int i = 0; i <= maxfd; i++) {
                if (user_pool->status[i] == ON && i != tfd) {
                    ips.push_back(user_pool->fdToIp[i]);
                }
            }
            Pthread_mutex_unlock(&user_pool->lock);

            if (!ips.empty()) {
                size = (int)ips.size() * (int)sizeof(uint32_t);
                msg1.ptr = (char *)malloc(size);
                memcpy(msg1.ptr, ips.data(), size);
                msg1.len = size;
            } else {
                msg1.ptr = NULL;
                msg1.len = 0;
            }
            sendqueue.push_msg(msg1);

            printf("join meeting: %d\n", msg.ip);
        }
    }

    return NULL;
}

static void *send_func(void *arg) {
    Pthread_detach(pthread_self());
    char * sendbuf = (char *)malloc(4 * MB);

    for (;;) {
        memset(sendbuf, 0, 4 * MB);
        MSG msg = sendqueue.pop_msg();
        int len = 0;

        sendbuf[len++] = '$';
        short type = htons((short)msg.msgType);
        memcpy(sendbuf + len, &type, sizeof(short));
        len += 2;

        // ip 字段：部分类型不需要
        if (msg.msgType == CREATE_MEETING_RESPONSE || msg.msgType == PARTNER_JOIN2) {
            len += 4; // 占位
        } else if (msg.msgType == TEXT_RECV || msg.msgType == PARTNER_EXIT ||
                   msg.msgType == PARTNER_JOIN || msg.msgType == IMG_RECV ||
                   msg.msgType == AUDIO_RECV || msg.msgType == CLOSE_CAMERA) {
            memcpy(sendbuf + len, &msg.ip, sizeof(uint32_t));
            len += 4;
        } else if (msg.msgType == HEARTBEAT_ACK) {
            len += 4; // 心跳 ACK 不带 ip，保持协议位宽一致
        }

        int msglen = htonl(msg.len);
        memcpy(sendbuf + len, &msglen, sizeof(int));
        len += 4;

        if (msg.len > 0 && msg.ptr != NULL) {
            memcpy(sendbuf + len, msg.ptr, msg.len);
            len += msg.len;
        }

        sendbuf[len++] = '#';

        Pthread_mutex_lock(&user_pool->lock);

        if (msg.msgType == CREATE_MEETING_RESPONSE) {
            if (writen(msg.targetfd, sendbuf, len) < 0) {
                err_msg("writen error");
            }
        } else if (msg.msgType == HEARTBEAT_ACK) {
            if (user_pool->status[msg.targetfd] == ON) {
                if (writen(msg.targetfd, sendbuf, len) < 0) {
                    err_msg("writen HEARTBEAT_ACK error");
                }
            }
        } else if (msg.msgType == PARTNER_EXIT || msg.msgType == IMG_RECV ||
                   msg.msgType == AUDIO_RECV || msg.msgType == TEXT_RECV ||
                   msg.msgType == CLOSE_CAMERA) {
            for (int i = 0; i <= maxfd; i++) {
                if (user_pool->status[i] == ON && msg.targetfd != i) {
                    if (writen(i, sendbuf, len) < 0) {
                        err_msg("writen error");
                    }
                }
            }
        } else if (msg.msgType == PARTNER_JOIN) {
            for (int i = 0; i <= maxfd; i++) {
                if (user_pool->status[i] == ON && i != msg.targetfd) {
                    if (writen(i, sendbuf, len) < 0) {
                        err_msg("writen error");
                    }
                }
            }
        } else if (msg.msgType == PARTNER_JOIN2) {
            for (int i = 0; i <= maxfd; i++) {
                if (user_pool->status[i] == ON && i == msg.targetfd) {
                    if (writen(i, sendbuf, len) < 0) {
                        err_msg("writen error");
                    }
                }
            }
        } else if (msg.msgType == KICK_OUT_RESPONSE) {
            // 单播给发起者
            if (user_pool->status[msg.targetfd] == ON) {
                if (writen(msg.targetfd, sendbuf, len) < 0) {
                    err_msg("writen error for KICK_OUT_RESPONSE");
                }
            }
        } else if (msg.msgType == KICK_OUT_NOTIFY) {
            // 这里我们将 targetfd 视为要下发的具体客户端
            if (user_pool->status[msg.targetfd] == ON) {
                if (writen(msg.targetfd, sendbuf, len) < 0) {
                    err_msg("writen error for KICK_OUT_NOTIFY");
                }
            }
        }

        Pthread_mutex_unlock(&user_pool->lock);

        // 释放消息体
        if (msg.ptr) {
            free(msg.ptr);
            msg.ptr = NULL;
        }
    }

    free(sendbuf);
    return NULL;
}
