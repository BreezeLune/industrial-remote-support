#include "unpthread.h"
#include <stdlib.h>
#include "unp.h"
#include "netheader.h"
#include "msg.h"
#include "auth.h"
#include <ctype.h>
#include <sys/ioctl.h>
#include <mysql/mysql.h>

// 定义一个最大允许的数据包体大小，例如64KB。防止客户端发送恶意超大包。
#define MAX_PACKET_BODY_SIZE 65536

pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER; // accept lock
extern socklen_t addrlen;
extern int listenfd;
extern int nprocesses;
extern Room *room;
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

void* thread_main(void *arg)
{
    void dowithuser(int connfd);
    int i = *(int *)arg;
    free(arg); //free
    int connfd;
    Pthread_detach(pthread_self()); //detach child thread

//    printf("thread %d starting\n", i);

    SA *cliaddr;
    socklen_t clilen;
    cliaddr = (SA *)Calloc(1, addrlen);
    char buf[MAXSOCKADDR];
    for(;;)
    {
        clilen = addrlen;
        //lock accept
        Pthread_mutex_lock(&mlock);
        connfd = Accept(listenfd, cliaddr, &clilen);
        //unlock accept
        Pthread_mutex_unlock(&mlock);

        printf("connection from %s\n", Sock_ntop(buf, MAXSOCKADDR, cliaddr, clilen));

        dowithuser(connfd); // process user


    }
    return NULL;
}


/*
 *
 *read data from client
 *
 */

void dowithuser(int connfd)
{
    void writetofd(int fd, MSG msg);

    char head[15]  = {0};
    //read head
    while(1)
    {
        ssize_t ret = Readn(connfd, head, 11);
        if(ret <= 0)
        {
           printf("[INFO] fd=%d closed (ret=%zd)\n", connfd, ret);
            close(connfd);
            return;
        }
        else if(ret < 11)
        {
            printf("data len too short\n");
        }
        else if(head[0] != '$')
        {
            printf("[ERR] fd=%d invalid start byte: 0x%02X ('%c')\n",
                   connfd, (unsigned char)head[0],
                   isprint((unsigned char)head[0]) ? head[0] : '.');
        }
        else
        {
            //solve datatype
            uint16_t msgtype;
            memcpy(&msgtype, head + 1, 2);
            uint16_t type_host = ntohs(msgtype);
            msgtype = (MSG_TYPE)type_host;
  

            //solve ip
            uint32_t ip;
            memcpy(&ip, head + 3, 4);
            ip = ntohl(ip);

            //solve datasize
            uint32_t raw_len;
            memcpy(&raw_len, head + 7, 4);
            uint32_t datasize = ntohl(raw_len);
            unsigned char *ip_bytes = (unsigned char *)&ip;
            printf("[PARSED] fd=%d msgtype=%u ip=%u.%u.%u.%u datasize=%u\n",
               connfd, type_host,
               ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3],
               datasize);
            // *** 【重要修改 1】: 增加数据包大小合法性检查 ***
            if (datasize > MAX_PACKET_BODY_SIZE) {
            printf("[ERR] fd=%d packet body size %u exceeds limit %d. Closing connection.\n",
                   connfd, datasize, MAX_PACKET_BODY_SIZE);
            close(connfd);
            return;
            }
    //        printf("msg type %d\n", msgtype);

            if(msgtype == CREATE_MEETING){
      char tail;
            ssize_t tr = Readn(connfd, &tail, 1);
            printf("[DBG] fd=%d tail=0x%02X '%c'\n",
                   connfd, (unsigned char)tail,
                   isprint((unsigned char)tail) ? tail : '.');
            if (datasize == 0 && tail == '#') {
                printf("[FLOW] fd=%d CREATE_MEETING OK\n", connfd);
                char *c = (char *)&ip;
                    printf("create meeting  ip: %d.%d.%d.%d\n", (u_char)c[3], (u_char)c[2], (u_char)c[1], (u_char)c[0]);
                    if(room->navail <=0) // no room
                    {
                        MSG msg;
                        memset(&msg, 0, sizeof(msg));
                        msg.msgType = CREATE_MEETING_RESPONSE;
                        int roomNo = 0;
                        msg.ptr = (char *) malloc(sizeof(int));
                        memcpy(msg.ptr, &roomNo, sizeof(int));
                        msg.len = sizeof(roomNo);
                        writetofd(connfd, msg);
                    }
                    else
                    {
                        int i;
                        //find room empty
                        Pthread_mutex_lock(&room->lock);

                        for(i = 0; i < nprocesses; i++)
                        {
                            if(room->pptr[i].child_status == 0) break;
                        }
                        if(i == nprocesses) //no room empty
                        {
                            MSG msg;
                            memset(&msg, 0, sizeof(msg));
                            msg.msgType = CREATE_MEETING_RESPONSE;
                            int roomNo = 0;
                            msg.ptr = (char *) malloc(sizeof(int));
                            memcpy(msg.ptr, &roomNo, sizeof(int));
                            msg.len = sizeof(roomNo);
                            writetofd(connfd, msg);
                        }
                        else
                        {
                            char cmd = 'C';
                            if(write_fd(room->pptr[i].child_pipefd, &cmd, 1, connfd) < 0)
                            {
                                printf("write fd error");
                                Pthread_mutex_unlock(&room->lock); // 错误也要解锁
                            }
                            else
                            {
                                close(connfd);
                                printf("room %d empty\n", room->pptr[i].child_pid);
                                room->pptr[i].child_status = 1; // occupy
                                room->navail--;
                                room->pptr[i].total++;
                                Pthread_mutex_unlock(&room->lock);
                                return;
                            }
                        }
                        Pthread_mutex_unlock(&room->lock);

                    }
            } else {
                printf("[ERR] fd=%d CREATE_MEETING bad tail or datasize\n", connfd);
                // 因为格式错误，关闭连接以防万一
                //    close(connfd);
            }
            }
            else if(msgtype == JOIN_MEETING)
            {
                printf("[FLOW] fd=%d JOIN_MEETING\n", connfd);
            //read msgsize
                uint32_t msgsize, roomno;
                memcpy(&msgsize, head + 7, 4);
                msgsize = ntohl(msgsize);
                //read data + #
                int r =  Readn(connfd, head, msgsize + 1 );
                if(r < msgsize + 1)
                {
                    printf("data too short\n");
                }
                else
                {
                    if(head[msgsize] == '#')
                    {
                        memcpy(&roomno, head, msgsize);
                        roomno = ntohl(roomno);
    //                    printf("room : %d\n", roomno);
                        //find room no
                        bool ok = false;
                        int i;
                        for(i = 0; i < nprocesses; i++)
                        {
                            if(room->pptr[i].child_pid == roomno && room->pptr[i].child_status == 1)
                            {
                                ok = true; //find room
                                break;
                            }
                        }

                        MSG msg;
                        memset(&msg, 0, sizeof(msg));
                        msg.msgType = JOIN_MEETING_RESPONSE;
                        msg.len = sizeof(uint32_t);
                        if(ok)
                        {
                            if(room->pptr[i].total >= 1024)
                            {
                                msg.ptr = (char *)malloc(msg.len);
                                uint32_t full = -1;
                                memcpy(msg.ptr, &full, sizeof(uint32_t));
                                writetofd(connfd, msg);
                            }
                            else
                            {
                                Pthread_mutex_lock(&room->lock);

                                char cmd = 'J';
    //                            printf("i  =  %d\n", i);
                                if(write_fd(room->pptr[i].child_pipefd, &cmd, 1, connfd) < 0)
                                {
                                    err_msg("write fd:");
                                }
                                else
                                {

                                    msg.ptr = (char *)malloc(msg.len);
                                    memcpy(msg.ptr, &roomno, sizeof(uint32_t));
                                    writetofd(connfd, msg);
                                    room->pptr[i].total++;// add 1
                                    Pthread_mutex_unlock(&room->lock);
                                    close(connfd);
                                    return;
                                }
                                Pthread_mutex_unlock(&room->lock);
                            }
                        }
                        else
                        {
                            msg.ptr = (char *)malloc(msg.len);
                            uint32_t fail = 0;
                            memcpy(msg.ptr, &fail, sizeof(uint32_t));
                            writetofd(connfd, msg);
                        }
                    }
                    else
                    {
                        printf("format error\n");
                    }
                }
            }
               /*————————————————————登录注册相关代码，勿动——————————————————————*/
else if (msgtype == AUTH_B) {  // 处理登录请求
    printf("Auth request from IP: %u.%u.%u.%u\n", 
           (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
           (ip >> 8) & 0xFF, ip & 0xFF);
    
    // 添加安全校验 - 使用合理的最大值
    if (datasize <= 0 || datasize > 1024) { // 使用1024作为合理最大值
        printf("Invalid auth datasize: %u\n", datasize);
        continue;
    }
    
    char *data = (char*)malloc(datasize + 1);
    if (!data) {
        perror("malloc failed");
        continue;
    }
    
    // 先读数据再读结束符
    ssize_t n = Readn(connfd, data, datasize);
    if (n != datasize) {
        free(data);
        printf("Auth data read error: %ld/%u\n", (long)n, datasize);
        continue;
    }
    data[datasize] = '\0';
    
    // 读取结束符
    char tail;
    if (Readn(connfd, &tail, 1) != 1 || tail != '#') {
        free(data);
        printf("AUTH missing end tag!\n");
        continue;
    }
    
    // 解析和处理数据
    char account[64], password[64], role[16];
    int parsed = sscanf(data, "%63[^|]|%63[^|]|%15s", account, password, role);
    if (parsed != 3) {
        printf("AUTH data format error: parsed %d fields\n", parsed);
        free(data);
        continue;
    }
    
    MSG resp;
    memset(&resp, 0, sizeof(resp)); // 正确初始化MSG结构体
    
    AuthResult auth = authenticateUser(account, password, role);
    if (auth.ok) {
        if(auth.role == "factory"){
                resp.msgType = AUTH_OK_FACTORY;
        }else if(auth.role == "expert"){
            resp.msgType = AUTH_OK_EXPERT;
        }else if(auth.role == "admin"){
            resp.msgType = AUTH_OK_ADMIN;
        }
        // 返回 角色|用户ID，便于客户端记录管理员ID
        char buf[64] = {0};
        snprintf(buf, sizeof(buf), "%s|%d", auth.role.c_str(), auth.userId);
        resp.ptr = strdup(buf);
        resp.len = strlen(resp.ptr);
        printf("Login success, role: %s\n", auth.role.c_str()); // 添加调试输出
    } else {
        resp.msgType = AUTH_FAILED;
        if (auth.error == "USER_REJECTED") {
            resp.ptr = strdup("该账号已被管理员拒绝，无法登录");
        } else if (auth.error == "USER_NOT_FOUND") {
            resp.ptr = strdup("用户不存在");
        } else if (auth.error == "BAD_PASSWORD") {
            resp.ptr = strdup("密码错误");
        } else if (auth.error == "ROLE_MISMATCH") {
            resp.ptr = strdup("角色不匹配");
        } else {
            resp.ptr = strdup("账号/密码/角色不匹配");
        }
        resp.len = strlen(resp.ptr);
    }
    writetofd(connfd, resp);
    free(data);
}
/*————————————————————登录注册相关代码，勿动——————————————————————*/
//注册
    else if (msgtype == AUTH_REGISTER) {  // 处理注册请求
    printf("Register request from IP: %u.%u.%u.%u\n", 
           (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
           (ip >> 8) & 0xFF, ip & 0xFF);
                // 正确读取完整数据包
    char *data = (char*)malloc(datasize + 1);
    ssize_t n = Readn(connfd, data, datasize);  // 读取数据部分
    if (n != datasize) {
        free(data);
        printf("read data error\n");
        continue;
    }
    data[datasize] = '\0';
    
    // 读取结束符
    char tail;
    if (Readn(connfd, &tail, 1) != 1 || tail != '#') {
        free(data);
        printf("data format error: missing end tag\n");
        continue;
    }

    // 解析数据："username|password|email|role"
    char username[64], password[64], email[64], role[16];
    sscanf(data, "%63[^|]|%63[^|]|%63[^|]|%15s", username, password, email, role);
    
    // ... 后续处理 ...

            //MYSQL *conn = db_init();
            MSG resp;
            memset(&resp, 0, sizeof(resp));
            resp.msgType = AUTH_REGISTER_FAILED;
            AuthResult regi = registerUser(username, password, email, "", role);
            printf("Register request: username=%s, email=%s, role=%s\n", username, email, role);
            if(regi.ok == true){
                printf("注册成功: %s\n", username);
                resp.msgType =AUTH_REGISTER_OK;
                resp.ptr = strdup("注册成功,待管理员审核");
            }
            else{
                printf("注册失败");
                resp.ptr = strdup("注册失败");
                resp.len = strlen(resp.ptr);
            }
            resp.len = strlen(resp.ptr);
/*resp.len = strlen(resp.ptr);
            if (conn) {
                if (db_register_user(conn, username, password, email, role)) {
                    resp.msgType = AUTH_REGISTER_OK;
                    resp.ptr = strdup("注册成功,待管理员审核");
                } else {
                    resp.ptr = strdup(mysql_error(conn)); // 返回数据库错误
                }
                resp.len = strlen(resp.ptr);
                mysql_close(conn);
            } else {
                resp.ptr = strdup("数据库连接失败");
                resp.len = strlen(resp.ptr);
            }
*/
            writetofd(connfd, resp);
            free(data);
            printf("success");
    }
    /*————————————————————登录注册相关代码，勿动——————————————————————*/
         else if(msgtype == HEARTBEAT)
            {
                char tail;
                Readn(connfd, &tail, 1);
                // 验证心跳包数据格式
                if(datasize == 0 && tail == '#')
                {
                    // 心跳包处理：构造并发送心跳确认包
                    MSG msg;
                    memset(&msg, 0, sizeof(msg));
                    msg.msgType = HEARTBEAT_ACK;
                    msg.len = 0;
                    msg.ptr = NULL;
                    writetofd(connfd, msg);
                    printf("Received HEARTBEAT from client, sent HEARTBEAT_ACK\n");
                }
                else
                {
                    printf("HEARTBEAT data format error\n");
                }
            }
            else if(msgtype == AUTH)
            {
                char tail;
                Readn(connfd, &tail, 1);
                // 验证认证消息数据格式
                if(datasize == 0 && tail == '#')
                {
                    // 接受认证消息（不做实际认证处理，只是确认收到）
                    printf("Received AUTH message from client, accepted\n");
                    // 可以选择发送确认消息
                    MSG msg;
                    memset(&msg, 0, sizeof(msg));
                    msg.msgType = HEARTBEAT_ACK; // 使用心跳确认作为简单的响应
                    msg.len = 0;
                    msg.ptr = NULL;
                    writetofd(connfd, msg);
                }
                else
                {
                    printf("AUTH data format error\n");
                }
            }
            else if (msgtype == GET_PENDING_USERS || msgtype == APPROVE_USER || msgtype == REJECT_USER)
            {
                // 读取数据体与尾标志（使用std::string，避免Qt依赖）
                std::string payload;
                if (datasize > 0) {
                    payload.resize(datasize);
                    Readn(connfd, &payload[0], datasize);
                }
                char tail;
                Readn(connfd, &tail, 1);

                MSG resp; memset(&resp, 0, sizeof(resp));
                if (msgtype == GET_PENDING_USERS) {
                    // 构造JSON数组响应（仅展示非管理员，且建议只取待审核）
                    std::vector<UserInfo> users = getAllUsers(); // 如需仅待审核可改为 getPendingUser()
                    std::string json = "[";
                    bool first = true;
                    for (size_t i = 0; i < users.size(); ++i) {
                        const UserInfo &u = users[i];
                        // 跳过管理员账号
                        if (u.role == "admin") continue;
                        char buf[1024];
                        snprintf(buf, sizeof(buf),
                                 "{\"userId\":%d,\"username\":\"%s\",\"email\":\"%s\",\"company\":\"%s\",\"requestedRole\":\"%s\",\"status\":\"%s\",\"reviewedBy\":0}",
                                 u.id,
                                 u.username.c_str(), u.email.c_str(), u.company.c_str(), u.role.c_str(), u.status.c_str());
                        if (!first) json += ",";
                        json += buf;
                        first = false;
                    }
                    json += "]";
                    resp.msgType = GET_PENDING_USERS_RESPONSE;
                    resp.ptr = strdup(json.c_str());
                    resp.len = (int)strlen(resp.ptr);
                } else if (msgtype == APPROVE_USER || msgtype == REJECT_USER) {
                    // 精简：仅解析 userId，不再解析 adminId/不再记录 reviewed_by
                    int uid = -1;
                    if (datasize > 0) {
                        const std::string &s = payload;
                        size_t p1 = s.find("userId");
                        if (p1 != std::string::npos) {
                            size_t c = s.find_first_of("0123456789", p1);
                            if (c != std::string::npos) uid = atoi(s.c_str() + c);
                        }
                    }
                    bool ok = false;
                    if (uid > 0) {
                        if (msgtype == APPROVE_USER) ok = approveUser(uid, -1);
                        else ok = rejectUser(uid, -1);
                    }
                    std::string json = ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"UPDATE_FAILED\"}";
                    resp.msgType = (msgtype == APPROVE_USER) ? APPROVE_USER_RESPONSE : REJECT_USER_RESPONSE;
                    resp.ptr = strdup(json.c_str());
                    resp.len = (int)strlen(resp.ptr);
                }
                writetofd(connfd, resp);
            }
            else
            {
                // // 对于其他不支持的消息类型，只打印信息，不报错
                // printf("Received unsupported message type: %d, ignoring\n", msgtype);
                // // 读取剩余数据以保持连接正常
                // char tail[1024];
                // if(datasize > 0)
                // {
                //     Readn(connfd, tail, datasize);
                // }
                // Readn(connfd, tail, 1); // 读取结尾的#

                // *** 【重要修改 2】: 安全地排空（丢弃）未知消息的数据体 ***
                printf("Received unsupported message type: %u, ignoring and draining %u bytes.\n", msgtype, datasize);

                // 使用一个临时的小缓冲区来循环读取并丢弃数据
                char drain_buf[1024];
                uint32_t remaining = datasize;
                while (remaining > 0) {
                    size_t to_read = (remaining > sizeof(drain_buf)) ? sizeof(drain_buf) : remaining;
                    ssize_t drained_bytes = Readn(connfd, drain_buf, to_read);
                    if (drained_bytes <= 0) {
                        printf("[ERR] fd=%d connection closed while draining data. Aborting.\n", connfd);
                        close(connfd);
                        return; // 立即退出函数
                    }
                    remaining -= drained_bytes;
                }

                // 最后，读取并丢弃结尾的 '#'
                char tail;
                Readn(connfd, &tail, 1);
                break;
            }
    }
}
}

void writetofd(int fd, MSG msg)
{
    // 计算所需缓冲区大小：头(11) + 数据 + 尾(1)
    size_t total_size = 11 + msg.len + 1;
    char *buf = (char *) malloc(total_size);
    if (!buf) {
        perror("malloc failed");
        return;
    }
    memset(buf, 0, total_size);
    
    int bytestowrite = 0;
    buf[bytestowrite++] = '$';

    uint16_t type = msg.msgType;
    type = htons(type);
    memcpy(buf + bytestowrite, &type, sizeof(uint16_t));

    bytestowrite += 2;//skip type

      // 填充IP字段，而不是跳过
    // 对于服务器响应，我们可以使用0.0.0.0作为IP
    uint32_t server_ip = 0; // 0.0.0.0
    server_ip = htonl(server_ip);
    memcpy(buf + bytestowrite, &server_ip, sizeof(uint32_t));
    bytestowrite += 4;

    uint32_t size = msg.len;
    size = htonl(size);
    memcpy(buf + bytestowrite, &size, sizeof(uint32_t));
    bytestowrite += 4;

    memcpy(buf + bytestowrite, msg.ptr, msg.len);
    bytestowrite += msg.len;

    buf[bytestowrite++] = '#';

    if(writen(fd, buf, bytestowrite) < bytestowrite)
    {
        printf("write fail\n");
    }

    if(msg.ptr)
    {
        free(msg.ptr);
        msg.ptr = NULL;
    }
    free(buf);
}
