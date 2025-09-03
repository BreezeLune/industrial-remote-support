#include "adminpanel.h"
#include "ui_adminpanel.h"
#include "mytcpsocket.h"
#include "netheader.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHeaderView>
#include <QApplication>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QScreen>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDir>

extern QUEUE_DATA<MESG> queue_send;

// 补充客户端未定义的管理员相关消息常量
#ifndef GET_PENDING_USERS
#define GET_PENDING_USERS 60
#endif
#ifndef APPROVE_USER
#define APPROVE_USER 61
#endif
#ifndef REJECT_USER
#define REJECT_USER 62
#endif
#ifndef GET_PENDING_USERS_RESPONSE
#define GET_PENDING_USERS_RESPONSE 63
#endif
#ifndef APPROVE_USER_RESPONSE
#define APPROVE_USER_RESPONSE 64
#endif
#ifndef REJECT_USER_RESPONSE
#define REJECT_USER_RESPONSE 65
#endif

AdminPanel::AdminPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AdminPanel)
    , m_tcpSocket(nullptr)
    , m_currentUserId(-1)
    , m_refreshTimer(new QTimer(this))
{
    ui->setupUi(this);

    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    setWindowTitle("管理员控制面板");
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    setMinimumSize(600, 400);

    setupTable();

    connect(m_refreshTimer, &QTimer::timeout, this, &AdminPanel::autoRefresh);
    m_refreshTimer->start(30000);

    ui->approveButton->setEnabled(false);
    ui->rejectButton->setEnabled(false);

    connect(ui->refreshButton, &QPushButton::clicked, this, &AdminPanel::refreshButtonClicked);
    connect(ui->approveButton, &QPushButton::clicked, this, &AdminPanel::approveButtonClicked);
    connect(ui->rejectButton, &QPushButton::clicked, this, &AdminPanel::rejectButtonClicked);
    connect(ui->statusFilter, QOverload<const QString&>::of(&QComboBox::currentTextChanged), this, &AdminPanel::statusFilterChanged);
    connect(ui->userTable, &QTableWidget::itemSelectionChanged, this, &AdminPanel::userTableSelectionChanged);

    ui->statusLabel->setText("状态：未连接");
    ui->label_2->setProperty("title", "admin_title");
    loadQssStyle();
}

AdminPanel::~AdminPanel()
{
    delete ui;
}

void AdminPanel::setTcpSocket(MyTcpSocket* socket)
{
    m_tcpSocket = socket;
    if (m_tcpSocket) {
        ui->statusLabel->setText("状态：已连接");
        // 连接管理员消息
        connect(m_tcpSocket, &MyTcpSocket::adminMessage, this, &AdminPanel::handleServerResponse, Qt::UniqueConnection);
        qDebug() << "[AdminPanel] TcpSocket set & signal connected";
    }
}

void AdminPanel::setCurrentUserId(int userId)
{
    m_currentUserId = userId;
}

void AdminPanel::showAdminPanel()
{
    qDebug() << "[AdminPanel] showAdminPanel: request pending users";
    this->show();
    loadPendingUsers();
}

void AdminPanel::hideAdminPanel()
{
    this->hide();
}

void AdminPanel::setupTable()
{
    QStringList headers;
    headers << "用户ID" << "用户名" << "邮箱" << "公司" << "申请角色" << "状态";
    ui->userTable->setColumnCount(headers.size());
    ui->userTable->setHorizontalHeaderLabels(headers);
    ui->userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->userTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->userTable->setAlternatingRowColors(true);
    ui->userTable->setSortingEnabled(true);
    ui->userTable->horizontalHeader()->setStretchLastSection(true);
    ui->userTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->userTable->setColumnWidth(0, 80);
    ui->userTable->setColumnWidth(1, 120);
    ui->userTable->setColumnWidth(2, 180);
    ui->userTable->setColumnWidth(3, 120);
    ui->userTable->setColumnWidth(4, 100);
    ui->userTable->setColumnWidth(5, 100);
}

void AdminPanel::refreshButtonClicked()
{
    loadPendingUsers();
}

void AdminPanel::approveButtonClicked()
{
    AdminUserInfo user;
    if (getSelectedUser(user)) {
        if (user.status == "已通过" || user.status == "已拒绝") {
            QMessageBox::warning(this, "警告", QString("用户 '%1' 已经审核过了，状态为：%2").arg(user.username, user.status));
            return;
        }
        if (QMessageBox::Yes == QMessageBox::question(this, "确认审核", QString("确定要审核通过用户 '%1' 吗？").arg(user.username), QMessageBox::Yes | QMessageBox::No)) {
            sendApproveUserRequest(user.userId);
        }
    }
}

void AdminPanel::rejectButtonClicked()
{
    AdminUserInfo user;
    if (getSelectedUser(user)) {
        if (user.status == "已通过" || user.status == "已拒绝") {
            QMessageBox::warning(this, "警告", QString("用户 '%1' 已经审核过了，状态为：%2").arg(user.username, user.status));
            return;
        }
        if (QMessageBox::Yes == QMessageBox::question(this, "确认拒绝", QString("确定要拒绝用户 '%1' 的申请吗？").arg(user.username), QMessageBox::Yes | QMessageBox::No)) {
            sendRejectUserRequest(user.userId);
        }
    }
}

void AdminPanel::statusFilterChanged(const QString& status)
{
    m_filteredUsers.clear();
    if (status == "全部" || status.isEmpty()) {
        m_filteredUsers = m_allUsers;
    } else {
        for (const AdminUserInfo& user : m_allUsers) {
            if (user.status == status) m_filteredUsers.append(user);
        }
    }
    updateTableData(m_filteredUsers);
}

void AdminPanel::userTableSelectionChanged()
{
    bool hasSelection = ui->userTable->currentRow() >= 0;
    ui->approveButton->setEnabled(hasSelection);
    ui->rejectButton->setEnabled(hasSelection);
}

void AdminPanel::handleServerResponse(int msgType, const QByteArray& data)
{
    qDebug() << "[AdminPanel] handleServerResponse: type=" << msgType << " bytes=" << data.size();
    switch (msgType) {
        case GET_PENDING_USERS_RESPONSE: {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isArray()) {
                QJsonArray arr = doc.array();
                m_allUsers.clear();
                for (const QJsonValue &v : arr) {
                    if (!v.isObject()) continue;
                    QJsonObject o = v.toObject();
                    AdminUserInfo u;
                    u.userId = o.value("userId").toInt();
                    u.username = o.value("username").toString();
                    u.email = o.value("email").toString();
                    u.company = o.value("company").toString();
                    u.requestedRole = o.value("requestedRole").toString();
                    u.status = mapStatusToCn(o.value("status").toString());
                    u.reviewedBy = o.value("reviewedBy").toInt();
                    m_allUsers.append(u);
                }
                qDebug() << "[AdminPanel] users loaded=" << m_allUsers.size();
                statusFilterChanged(ui->statusFilter->currentText());
                ui->statusLabel->setText(QString("状态：已加载 %1 个用户").arg(m_allUsers.size()));
                emit pendingUsersReceived(m_allUsers);
            }
            break; }
        case APPROVE_USER_RESPONSE:
        case REJECT_USER_RESPONSE: {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject r = doc.object();
                bool ok = r.value("ok").toBool();
                QString err = r.value("error").toString();
                if (ok) {
                    showMessage("成功", msgType == APPROVE_USER_RESPONSE ? "用户审核通过成功！" : "用户审核拒绝成功！");
                    if (msgType == APPROVE_USER_RESPONSE) emit approveUserResult(true, "ok");
                    else emit rejectUserResult(true, "ok");
                    qDebug() << "[AdminPanel] approve/reject ok, reload list";
                    loadPendingUsers();
                } else {
                    QString msg = QString("审核失败: %1").arg(err);
                    showMessage("错误", msg);
                    if (msgType == APPROVE_USER_RESPONSE) emit approveUserResult(false, msg);
                    else emit rejectUserResult(false, msg);
                }
            }
            break; }
    }
}

void AdminPanel::autoRefresh()
{
    if (m_tcpSocket && isConnected()) loadPendingUsers();
}

void AdminPanel::loadPendingUsers()
{
    if (m_tcpSocket && isConnected()) {
        qDebug() << "[AdminPanel] loadPendingUsers: send request";
        sendGetPendingUsersRequest();
        ui->statusLabel->setText("状态：正在加载...");
    } else {
        ui->statusLabel->setText("状态：未连接");
        showMessage("错误", "网络连接已断开，请检查连接状态");
    }
}

void AdminPanel::updateTableData(const QVector<AdminUserInfo>& users)
{
    qDebug() << "[AdminPanel] updateTableData rows=" << users.size();
    bool sorting = ui->userTable->isSortingEnabled();
    ui->userTable->setSortingEnabled(false);
    ui->userTable->clearContents();
    ui->userTable->setRowCount(users.size());
    for (int i = 0; i < users.size(); ++i) {
        const AdminUserInfo& u = users[i];
        QTableWidgetItem* userIdItem = new QTableWidgetItem(QString::number(u.userId));
        QTableWidgetItem* usernameItem = new QTableWidgetItem(u.username);
        QTableWidgetItem* emailItem = new QTableWidgetItem(u.email);
        QTableWidgetItem* companyItem = new QTableWidgetItem(u.company);
        QTableWidgetItem* roleItem = new QTableWidgetItem(u.requestedRole);
        QTableWidgetItem* statusItem = new QTableWidgetItem(u.status);

        if (u.status == "待审核") { statusItem->setBackground(QColor("#f39c12")); statusItem->setForeground(QColor("#ffffff")); }
        else if (u.status == "已通过") { statusItem->setBackground(QColor("#27ae60")); statusItem->setForeground(QColor("#ffffff")); }
        else if (u.status == "已拒绝") { statusItem->setBackground(QColor("#e74c3c")); statusItem->setForeground(QColor("#ffffff")); }

        ui->userTable->setItem(i, 0, userIdItem);
        ui->userTable->setItem(i, 1, usernameItem);
        ui->userTable->setItem(i, 2, emailItem);
        ui->userTable->setItem(i, 3, companyItem);
        ui->userTable->setItem(i, 4, roleItem);
        ui->userTable->setItem(i, 5, statusItem);
    }
    ui->userTable->setSortingEnabled(sorting);
}

int AdminPanel::indexOfUserById(int userId, const QVector<AdminUserInfo> &list) const
{
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].userId == userId) return i;
    }
    return -1;
}

bool AdminPanel::getSelectedUser(AdminUserInfo &outUser) const
{
    int row = ui->userTable->currentRow();
    if (row < 0) return false;
    QTableWidgetItem *idItem = ui->userTable->item(row, 0);
    if (!idItem) return false;
    bool ok = false;
    int userId = idItem->text().toInt(&ok);
    if (!ok) return false;
    // 在当前过滤后的列表中查找
    for (const AdminUserInfo &u : m_filteredUsers) {
        if (u.userId == userId) { outUser = u; return true; }
    }
    // 兜底：在全量列表中查找
    for (const AdminUserInfo &u : m_allUsers) {
        if (u.userId == userId) { outUser = u; return true; }
    }
    return false;
}

void AdminPanel::sendGetPendingUsersRequest()
{
    if (!m_tcpSocket) return;
    QJsonObject req; req["adminId"] = m_currentUserId;
    QByteArray data = QJsonDocument(req).toJson(QJsonDocument::Compact);
    qDebug() << "[AdminPanel] -> GET_PENDING_USERS payload=" << data;
    sendData(GET_PENDING_USERS, data);
}

void AdminPanel::sendApproveUserRequest(int userId)
{
    if (!m_tcpSocket) return;
    QJsonObject req; req["userId"] = userId; // 精简：不再携带 adminId
    QByteArray data = QJsonDocument(req).toJson(QJsonDocument::Compact);
    qDebug() << "[AdminPanel] -> APPROVE_USER userId=" << userId << " payload=" << data;
    sendData(APPROVE_USER, data);
}

void AdminPanel::sendRejectUserRequest(int userId)
{
    if (!m_tcpSocket) return;
    QJsonObject req; req["userId"] = userId; // 精简：不再携带 adminId
    QByteArray data = QJsonDocument(req).toJson(QJsonDocument::Compact);
    qDebug() << "[AdminPanel] -> REJECT_USER userId=" << userId << " payload=" << data;
    sendData(REJECT_USER, data);
}

void AdminPanel::sendData(int msgType, const QByteArray& data)
{
    if (!m_tcpSocket) return;
    MESG* msg = (MESG*)malloc(sizeof(MESG));
    if (!msg) return;
    memset(msg, 0, sizeof(MESG));
    msg->msg_type = (MSG_TYPE)msgType;
    msg->len = data.size();
    if (data.size() > 0) {
        msg->data = (uchar*)malloc(data.size());
        if (!msg->data) { free(msg); return; }
        memcpy(msg->data, data.data(), data.size());
    } else {
        msg->data = nullptr;
    }
    queue_send.push_msg(msg);
}

void AdminPanel::showMessage(const QString& title, const QString& message)
{
    QMessageBox::information(this, title, message);
}

bool AdminPanel::isConnected() const
{
    return m_tcpSocket != nullptr;
}

void AdminPanel::loadQssStyle()
{
    QStringList possiblePaths = {
        "styles.qss",
        "../styles.qss",
        "../../styles.qss"
    };
    bool qssLoaded = false; QString baseQss;
    for (const QString &p : possiblePaths) {
        QFile f(p); if (f.open(QFile::ReadOnly | QFile::Text)) { QTextStream s(&f); baseQss = s.readAll(); f.close(); qssLoaded = true; break; }
    }
    if (!qssLoaded) { setBuiltinStyle(); return; }
    QString adminPanelQss = baseQss + R"(
        QLabel[title="admin_title"] { font-size: 18pt; font-weight: bold; color: #3daee9; background-color: transparent; border: none; margin: 10px; }
        QTableWidget { background-color: #353535; border: 1px solid #404040; border-radius: 4px; gridline-color: #404040; selection-background-color: #3daee9; selection-color: white; alternate-background-color: #2a2a2a; }
        QTableWidget::item { padding: 6px; border: none; background-color: transparent; color: #e0e0e0; }
        QHeaderView::section { background-color: #404040; color: #e0e0e0; padding: 8px; border: 1px solid #505050; font-weight: bold; font-size: 9pt; }
        QLabel#statusLabel { background-color: #252525; border: 1px solid #404040; border-radius: 4px; padding: 6px; color: #e0e0e0; font-weight: bold; }
    )";
    this->setStyleSheet(adminPanelQss);
}

void AdminPanel::setBuiltinStyle()
{
    QString qss = R"(
        * { font-family: "Microsoft YaHei", "Segoe UI", sans-serif; font-size: 8pt; color: #e0e0e0; }
        QWidget { background-color: #2b2b2b; color: #e0e0e0; selection-background-color: #3daee9; selection-color: white; }
        QGroupBox { font-weight: bold; border: 2px solid #404040; border-radius: 8px; margin-top: 1ex; padding-top: 10px; background-color: #353535; }
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 8px; background-color: #404040; border-radius: 4px; color: #3daee9; }
        QPushButton { background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #505050, stop:1 #383838); border: 1px solid #404040; border-radius:6px; padding:6px 12px; min-width:70px; font-weight:bold; }
        QPushButton:hover { background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #606060, stop:1 #484848); border: 1px solid #3daee9; }
        QPushButton:pressed { background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #383838, stop:1 #505050); }
        QPushButton:disabled { background-color: #323232; color: #707070; border: 1px solid #404040; }
        QComboBox { background-color: #404040; border: 1px solid #505050; border-radius: 4px; padding: 6px; selection-background-color: #3daee9; selection-color: white; }
        QHeaderView::section { background-color: #404040; color: #e0e0e0; padding: 6px; border: 1px solid #505050; font-weight: bold; }
        QLabel { background-color: transparent; border: none; color: #e0e0e0; }
        QLabel#statusLabel { background-color: #252525; border: 1px solid #404040; border-radius: 4px; padding: 4px; }
    )";
    this->setStyleSheet(qss);
}

QString AdminPanel::mapStatusToCn(const QString& raw)
{
    if (raw.compare("pending", Qt::CaseInsensitive) == 0) return "待审核";
    if (raw.compare("approved", Qt::CaseInsensitive) == 0) return "已通过";
    if (raw.compare("rejected", Qt::CaseInsensitive) == 0) return "已拒绝";
    return raw;
}


