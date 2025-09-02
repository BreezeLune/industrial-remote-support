#ifndef NETHEADER_H
#define NETHEADER_H

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
    AUTH_OK_ADMIN,     // 认证成功
    AUTH_FAILED,    // 认证失败
    AUTH_REGISTER,  // 注册请求
    AUTH_REGISTER_OK,    // 注册成功
    AUTH_REGISTER_FAILED,// 注册失败
    CREATE_MEETING_RESPONSE = 20,
    PARTNER_EXIT = 21,
    PARTNER_JOIN = 22,
    JOIN_MEETING_RESPONSE = 23,
    PARTNER_JOIN2 = 24,
     //心跳类型
    HEARTBEAT = 99,       // 客户端-服务端
    HEARTBEAT_ACK = 98,    // 服务端-客户端
    AUTH = 97,
    RemoteHostClosedError = 40,
    OtherNetError = 41

};
// 角色枚举
enum RoleType {
    ROLE_UNKNOWN,
    ROLE_FACTORY,
    ROLE_EXPERT
};
enum Image_Format {
    Format_Invalid,
    Format_Mono,
    Format_MonoLSB,
    Format_Indexed8,
    Format_RGB32,
    Format_ARGB32,
    Format_ARGB32_Premultiplied,
    Format_RGB16,
    Format_ARGB8565_Premultiplied,
    Format_RGB666,
    Format_ARGB6666_Premultiplied,
    Format_RGB555,
    Format_ARGB8555_Premultiplied,
    Format_RGB888,
    Format_RGB444,
    Format_ARGB4444_Premultiplied,
    Format_RGBX8888,
    Format_RGBA8888,
    Format_RGBA8888_Premultiplied,
    Format_BGR30,
    Format_A2BGR30_Premultiplied,
    Format_RGB30,
    Format_A2RGB30_Premultiplied,
    Format_Alpha8,
    Format_Grayscale8,
    Format_RGBX64,
    Format_RGBA64,
    Format_RGBA64_Premultiplied,
    Format_Grayscale16,
    Format_BGR888,
    NImageFormats
};

#endif // NETHEADER_H
