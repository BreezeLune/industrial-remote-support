## 修改的文件：`client/mytcpsocket.cpp`

### 修改位置和内容：

**1. 在 `recvFromSocket()` 函数中，大约在第200-250行左右**

**修改前：**

```cpp
quint32 data_size;
qFromBigEndian<quint32>(recvbuf + 7, 4, &data_size);
```

**修改后：**

```cpp
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
```

**2. 在 `CREATE_MEETING_RESPONSE` 处理块中**

**修改前：**

```cpp
qint32 roomNo;
qFromBigEndian<qint32>(recvbuf + MSG_HEADER, 4, &roomNo);
// ... 创建MESG* msg时
msg->data = (uchar*)malloc(data_size);
memcpy(msg->data, recvbuf + MSG_HEADER, data_size);
msg->len = data_size;
```

**修改后：**

```cpp
qint32 roomNo;
// MSG_HEADER = 11，但CREATE_MEETING_RESPONSE的数据从位置11开始
qFromBigEndian<qint32>(recvbuf + MSG_HEADER, 4, &roomNo);
// ... 创建MESG* msg时
msg->data = (uchar*)malloc(4); // 使用固定的4字节长度存储roomNo
memcpy(msg->data, &roomNo, 4);
msg->len = 4; // 设置长度为4字节
```

### 修改的核心问题：

这个bug的根本原因是**协议不匹配**：

- **服务端**：在 `server/room.cpp` 的 `send_func` 中，对于 `CREATE_MEETING_RESPONSE` 消息，跳过了4字节的IP字段
- **客户端**：原本的解析逻辑假设所有消息都有IP字段，导致读取 `MSG_SIZE` 时位置错误
