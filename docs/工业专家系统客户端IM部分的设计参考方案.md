目标：用 Qt 6.5+/6.7 LTS 实现一款跨平台（Windows/macOS/Linux，后续可扩展 Android）的音视频会议客户端，支持实时音视频、文本聊天、成员管理、日志与监控，满足教学与中小型团队实际使用。

---

1. 项目概述

功能范围：

- 会议管理：创建/加入/退出会议、成员在线状态
- 实时通信：音频采集/回放，视频采集/显示，文本聊天
- 控制信令：静音/取消静音、开关摄像头、踢人、设置主画面
- 用户体验：@ 成员、消息气泡、音量调节、设备选择
- 健壮性：断线重连、心跳保活、日志与崩溃信息
- 安全性（可选）：TLS 加密、鉴权令牌、消息签名

非功能要求：

- 延时：音频端到端 ≤ 200ms，视频 ≤ 400ms（内网场景）
- 并发：单房间 8–16 人（UI 默认 3×3 / 4×4 视频墙）
- 资源：1080p/30fps 的单路主画面 + 6–8 路 360p/15fps 的副画面

技术栈：Qt 6（Widgets + Multimedia + Network + Concurrent + Test），C++17，CMake；编解码建议 Opus（音频）+ H.264（视频，FFmpeg/硬编），MVP 可先用 PCM + JPEG。

---

2. 总体架构

    UI 层（Widgets）
     ├─ MainWindow（视频墙/聊天/控制面板）
     ├─ DevicePanel（设备选择、音量、分辨率）
     └─ ChatView（消息列表 + 输入框）

    业务层（MeetingCore）
     ├─ SessionManager（进房/退房、成员状态、指令分发）
     ├─ MediaController（摄像头/麦克风/扬声器、采集参数）
     ├─ TextChatService（文本消息）
     └─ StatsMonitor（码率/丢包/时延）

    通信层（Transport）
     ├─ SignalingClient（TCP/TLS：登录、心跳、控制信令）
     ├─ AVTransport (可插拔)：
     │   ├─ TCPFrameChannel（MVP）
     │   └─ RTP/UDP + JitterBuffer（优化）
     └─ ProtoCodec（封包/解包、序列化）

    媒体层（Media）
     ├─ AudioCapture  （QAudioSource）
     ├─ AudioPlayback （QAudioSink）
     ├─ VideoCapture  （QCamera + QMediaCaptureSession + QVideoSink）
     ├─ VideoRenderer （QLabel/QOpenGLWidget/QQuickWidget）
     └─ Codec(可选) ：Opus/H.264 适配器

    基础设施（Infra）
     ├─ SafeQueue<T>（QMutex + QWaitCondition）
     ├─ Logger（QLoggingCategory + 文件滚动）
     ├─ Config（JSON/YAML）
     └─ Crash/Trace（信号捕获 + 栈回溯，可选）

线程模型（建议）：

- UI 线程：MainWindow/渲染控件
- 网络线程：SignalingClient + TCPFrameChannel（QObject + moveToThread）
- 媒体线程：AudioCapture、AudioPlayback、VideoCapture（各 1 个）
- 编解码线程：Opus/H.264（可用 QThreadPool）
- 分发线程：RecvDispatcher（从队列取包，分发到业务/UI）

---

3. 通信协议设计

3.1 封包格式（字节序：大端）

    '$' 1B | Ver 1B | MsgType 2B | Seq 4B | Timestamp 8B | Sender 4B | Room 4B | PayloadLen 4B | CRC32 4B | Payload | '#'

- Ver：协议版本，初版 0x01
- MsgType：见下（control / text / audio / video / ack / hb）
- Seq：序列号，重传与乱序检测
- Timestamp：UTC 毫秒，用于时延统计与抖动缓冲
- Sender / Room：用户 ID / 会议 ID
- Payload：具体消息体（JSON/二进制）

3.2 消息类型（节选）

- 控制：HELLO, AUTH, JOIN, LEAVE, KICK, MUTE, UNMUTE, CAMERA_ON, CAMERA_OFF
- 聊天：TEXT
- 媒体：AUDIO_FRAME, VIDEO_FRAME
- 保活：HEARTBEAT, HEARTBEAT_ACK
- 应答：OK, ERROR

3.3 传输信道

- MVP：统一 TCP（简化 NAT/防火墙穿透），设置 TcpNoDelay，自建「音频优先队列」避免饿死
- 增强：媒体走 UDP/RTP(+SRTP)，控制与文本走 TCP/TLS；音频帧使用 jitter buffer（60–120ms）

---

4. 关键模块设计

4.1 SignalingClient（TCP/TLS）

- 负责连接/鉴权/心跳/指令收发
- 断线重连：指数退避（1s, 2s, 4s, 上限 15s），重连成功自动重入房
- 接口：connect(url, token), join(roomId), sendCommand(cmd), sendText(msg)

4.2 AVTransport（媒体收发）

- sendAudioFrame(const QByteArray& bytes, qint64 ts)
- sendVideoFrame(const QByteArray& bytes, qint64 ts)
- MVP 实现：TCPFrameChannel
  - 独立发送线程：从 audioSendQ 优先取，再取 videoSendQ
  - 接收线程：按 MsgType 分发到 audioRecvQ / videoRecvQ / ctrlQ
- 优化实现：RTPChannel（预留接口）

4.3 AudioCapture / AudioPlayback（Qt6 Multimedia）

- 设备枚举：QMediaDevices::audioInputs() / audioOutputs()
- 采集：QAudioSource → QIODevice::read() 拉流；采样建议 16k/48kHz，单声道，16bit
- 播放：QAudioSink 写入；带 抖动缓冲（基于时间戳重排 + 欠/过载纠偏）
- 编码：
  - MVP：PCM → zlib（qCompress）+ 可选 Base64（跨语言调试方便）
  - 生产：Opus（libopus）

4.4 VideoCapture / VideoRenderer

- 采集：QCamera + QMediaCaptureSession + QVideoSink
  - connect(videoSink, &QVideoSink::videoFrameChanged, this, onFrame)
- 格式：NV12/ARGB；将 QVideoFrame 映射为 QImage（CPU 渲染）或上传到 QOpenGLTexture
- 编码：
  - MVP：QImage::save(QBuffer, "JPEG", quality)
  - 生产：H.264（FFmpeg / 平台硬编）
- 渲染控件：
  - 简易：QLabel + QPixmap（教学）
  - 高效：QOpenGLWidget 或 QQuickWidget（后者可做动效与布局）

4.5 SessionManager / TextChatService

- 管理本地媒体状态与远端成员集合：QHash<Uid, Participant>
- 信令到业务的映射（加入/离开/静音/主持人切换）
- 文本聊天：Model-View 分离（QAbstractListModel，便于分页与存档）

4.6 SafeQueue<T>

- 典型实现：QQueue<T> + QMutex + QWaitCondition
- 支持 push(const T&)、pop(T& out, int timeoutMs)、clear()、size()
- 音频帧单独队列 audioRecvQ，优先级最高

4.7 Logger / Config / Stats

- QLoggingCategory + 轮转文件（按天/按大小）
- 关键埋点：连接状态、重连次数、音视频帧率、码率、丢包、音量电平（VU meter）
- 配置：config.json（服务器地址、初始码率、分辨率、录制开关）

---

5. 典型数据流

1) 本地音频：QAudioSource → PCM 缓冲 → (Opus 编码) → audioSendQ → AVTransport → 服务器 → audioRecvQ → (解码) → QAudioSink

2) 本地视频：QCamera → QVideoSink（QVideoFrame）→ 转 QImage（或 GPU 纹理）→ (H.264/JPEG) → videoSendQ → 服务器 → UI 渲染

3) 文本消息：输入框 → TextChatService → SignalingClient → 服务器 → ChatView

---

6. 关键接口与代码骨架（部分）

6.1 CMake 项目骨架

    cmake_minimum_required(VERSION 3.20)
    project(CloudMeeting LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    
    find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia Concurrent)
    
    qt_add_executable(CloudMeeting
        src/main.cpp
        src/ui/mainwindow.cpp src/ui/mainwindow.h src/ui/mainwindow.ui
        src/core/sessionmanager.cpp src/core/sessionmanager.h
        src/core/mediacontroller.cpp src/core/mediacontroller.h
        src/net/signalingclient.cpp src/net/signalingclient.h
        src/net/tcpframechannel.cpp src/net/tcpframechannel.h
        src/media/audiocapture.cpp src/media/audiocapture.h
        src/media/audioplayback.cpp src/media/audioplayback.h
        src/media/videocapture.cpp src/media/videocapture.h
        src/media/videorenderer.cpp src/media/videorenderer.h
        src/common/safequeue.h src/common/logger.cpp src/common/logger.h
        src/common/protobuf.cpp src/common/protobuf.h
    )
    
    target_link_libraries(CloudMeeting PRIVATE
        Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::Concurrent)

6.2 音频采集（Qt6 API）

    // audiocapture.h
    class AudioCapture : public QObject {
        Q_OBJECT
    public:
        struct Frame { QByteArray pcm; qint64 tsMs; };
        explicit AudioCapture(QObject* p=nullptr);
        bool start(const QAudioDevice& dev, const QAudioFormat& fmt);
        void stop();
    signals:
        void frameReady(const Frame& f);
    private slots:
        void onReadyRead();
    private:
        QScopedPointer<QAudioSource> src_;
        QIODevice* io_ = nullptr;
        QAudioFormat fmt_;
    };
    
    // audiocapture.cpp
    bool AudioCapture::start(const QAudioDevice& dev, const QAudioFormat& fmt){
        fmt_ = fmt; src_.reset(new QAudioSource(dev, fmt_));
        io_ = src_->start();
        connect(io_, &QIODevice::readyRead, this, &AudioCapture::onReadyRead);
        return io_ != nullptr;
    }
    void AudioCapture::onReadyRead(){
        QByteArray buf = io_->readAll();
        emit frameReady({buf, QDateTime::currentMSecsSinceEpoch()});
    }

6.3 音频播放（抖动缓冲思想）

    class AudioPlayback : public QObject {
        Q_OBJECT
    public:
        explicit AudioPlayback(QObject* p=nullptr);
        bool start(const QAudioDevice& dev, const QAudioFormat& fmt);
        void stop();
    public slots:
        void playFrame(const QByteArray& pcm, qint64 tsMs);
    private:
        QScopedPointer<QAudioSink> sink_;
        QIODevice* io_ = nullptr;
        // 简化：可替换为基于时间戳的环形缓冲
    };

6.4 视频采集（QCamera + QVideoSink）

    class VideoCapture : public QObject {
        Q_OBJECT
    public:
        explicit VideoCapture(QObject* p=nullptr);
        bool start(const QCameraDevice& dev, const QSize& res, int fps);
        void stop();
    signals:
        void frameReady(const QVideoFrame& vf, qint64 tsMs);
    private:
        QCamera camera_;
        QMediaCaptureSession session_;
        QVideoSink sink_;
    };
    
    VideoCapture::VideoCapture(){
        session_.setCamera(&camera_);
        session_.setVideoOutput(&sink_);
        QObject::connect(&sink_, &QVideoSink::videoFrameChanged, this,[this](const QVideoFrame& vf){
            emit frameReady(vf, QDateTime::currentMSecsSinceEpoch());
        });
    }

6.5 网络收发（MVP：统一 TCP）

    class TCPFrameChannel : public QObject {
        Q_OBJECT
    public:
        explicit TCPFrameChannel(QObject* p=nullptr);
        void connectTo(const QHostAddress& host, quint16 port);
        void sendPacket(const QByteArray& pkt);
    signals:
        void packetReceived(QByteArray pkt);
    private slots:
        void onReadyRead();
    private:
        QTcpSocket sock_;
        QByteArray rbuf_;
    };

---

7. UI 设计（Widgets）

- MainWindow：中央视频墙（QGridLayout 动态布局），右侧聊天面板（消息 QListView + 输入框），顶部工具栏（设备、分辨率、码率、录制按钮）
- ParticipantTile：单个成员视频卡片（头像/昵称/网络指示/静音图标）
- ChatView：消息模型 + 富文本渲染（气泡、时间戳、@ 高亮）
- DevicePanel：用 QMediaDevices 列举音视频设备，可热插拔监听

---

8. 安全与可靠性

- 传输安全：Signaling 使用 TLS（QSslSocket）；媒体通道可选 SRTP
- 鉴权：登录后发放短期 JWT；每次 JOIN 携带 token
- 心跳与保活：客户端每 5s 发 HEARTBEAT，服务端 10s 超时剔除
- 断线重连：指数退避 + 状态恢复（媒体自动重启）
- 流控：根据网络质量动态调节帧率/分辨率/码率（先简单开关：360p/720p/1080p 档）

---

9. 性能优化路线

10. 先简后繁：MVP 用 TCP + JPEG + PCM；确认端到端链路与 UI
11. 音频先行：接入 Opus 编解码，降低带宽（16kbps–32kbps）
12. 视频编码：接入 H.264（FFmpeg 或平台硬编，如 Windows Media Foundation）
13. UDP/RTP：媒体改走 UDP，加入抖动缓冲与 NACK/FEC
14. 渲染优化：GPU 路径（YUV→RGB 着色器），避免 CPU 拷贝

---

10. 目录结构建议

   CloudMeeting/
     CMakeLists.txt
     src/
       main.cpp
       ui/ (MainWindow、控件、资源)
       core/ (SessionManager, MediaController, TextChatService, StatsMonitor)
       media/ (AudioCapture, AudioPlayback, VideoCapture, VideoRenderer, codec adapters)
       net/ (SignalingClient, TCPFrameChannel, RTPChannel*)
       common/ (safequeue.h, logger.*, config.*, protobuf.*)
     resources/
       icons/, qml/(*可选*), styles.qss
     tests/ (Qt Test)
     docs/  (协议、时序图、开发手册)
     scripts/ (packaging、ci)

---

11. 测试与交付

- 单元测试：Codec 适配器、SafeQueue、ProtoCodec、消息模型
- 集成测试：回环（本机采集→本机播放），两端互通（局域网）
- 压力测试：模拟 8–16 人会议（本地多进程 + 虚拟摄像头/音频）
- CI/CD：GitHub Actions 构建 + windeployqt/macdeployqt 打包
- 监控：实时显示本地与远端的 fps/bitrate/jitter/丢包

---

12. 迁移与兼容（Qt5 → Qt6 要点）

- 多媒体 API：用 QAudioSource/Sink、QMediaDevices、QVideoSink 取代 Qt5 的旧接口
- 构建系统：全面使用 CMake；qmake 仅用于学习对比
- 正则/JSON：统一 QRegularExpression 与 QJson*

---

13. 里程碑（建议）

- M1（1–2 周）：TCP 信令 + 文本聊天 + 设备枚举 + 本地采集与渲染
- M2（2–3 周）：音频通话可用（PCM → Opus），断线重连/心跳
- M3（2–3 周）：视频互通（JPEG → H.264），视频墙与主画面
- M4（1–2 周）：RTP/UDP 通道、统计面板、打包上线

---

14. 风险与对策

- 跨平台摄像头兼容：增加设备黑/白名单，允许用户切换分辨率
- 网络抖动：实装 jitter buffer 与动态码率策略
- 专利/许可：H.264 需注意授权；教学环境可用开源 FFmpeg，自研/商用需评估

---

15. 可扩展功能

- 屏幕共享（Desktop Duplication API / X11 / PipeWire）
- 录制与回放（混音/混画，MP4 封装）
- 虚拟背景/降噪（GPU/NN 加速）
- 会议控制台（主持人权限、举手、投票）

---

结论：本方案从协议、模块、线程、接口到落地代码骨架均基于 Qt6 最新多媒体栈，先以 TCP + 简单编解码实现 MVP，随后逐步升级到 Opus/H.264 + UDP/RTP 的生产路线。
