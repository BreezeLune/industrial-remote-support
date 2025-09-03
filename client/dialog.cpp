#include "dialog.h"
#include "ui_dialog.h"
#include "widget.h" // 包含目标 Widget 的头文件
#include "netheader.h"
#include "mytcpsocket.h"
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog),
    m_targetWidget(nullptr),
    m_tcpSocket(nullptr)

// m_tcpSocket(new MyTcpSocket(this))
{
    ui->setupUi(this);
    this->resize(800, 600);

    //MyTcpSocket *m_tcpSocket = new MyTcpSocket;
    /*
    // 假设按钮名为togglePasswordBtn，密码框为passwordLineEdit
    connect(ui->togglePasswordBtn, &QPushButton::clicked, this, [=]() {
        // 切换显示模式：如果是密码模式则切换为正常模式，反之亦然
        if (ui->passwordLineEdit->echoMode() == QLineEdit::Password) {
            ui->passwordLineEdit->setEchoMode(QLineEdit::Normal); // 显示明文
            ui->togglePasswordBtn->setText("隐藏密码");
        } else {
            ui->passwordLineEdit->setEchoMode(QLineEdit::Password); // 隐藏为密码
            ui->togglePasswordBtn->setText("显示密码");
        }
    });
*/
    // 连接登录/注册结果信号
    // 绑定登录按钮点击事件
    connect(ui->login_button, &QPushButton::clicked, this, &Dialog::onLoginClicked);
    // 绑定注册按钮点击事件
    connect(ui->register_button, &QPushButton::clicked, this, &Dialog::onRegisterClicked);
    // 绑定去注册按钮的 "clicked" 信号到自定义槽函数 onRegisterClicked
    connect(ui->go_register, &QPushButton::clicked, this, &Dialog::onGoRegisterClicked);
    // 绑定返回登录按钮的 "clicked" 信号到自定义槽函数 onBackLoginClicked
    connect(ui->back_login, &QPushButton::clicked, this, &Dialog::onBackLoginClicked);
    // 绑定登录结果信号
    // 在Dialog中连接时指定AutoConnection
    connect(m_tcpSocket, &MyTcpSocket::loginSuccess,
            this, &Dialog::onLoginSuccess, Qt::AutoConnection);
    connect(m_tcpSocket, &MyTcpSocket::loginFailed, this, &Dialog::onLoginFailed);
    // 注册的信号连接
    connect(m_tcpSocket, &MyTcpSocket::registerSuccess, this, &Dialog::onRegisterSuccess);
    connect(m_tcpSocket, &MyTcpSocket::registerFailed, this, &Dialog::onRegisterFailed);
}

Dialog::~Dialog()
{
    // 在 Dialog 的析构函数或关闭事件中
    delete ui;
    if (m_targetWidget) {
        delete m_targetWidget;
    }
}

// 点击登录按钮
void Dialog::onLoginClicked()
{
    QString username = ui->username->text().trimmed();
    QString password = ui->password->text().trimmed();
    QString role = getSelectedRoleAuth();
    qDebug() << "Login! Role received:" << role; // 调试输出

    if (username.isEmpty() || password.isEmpty() || role.isEmpty()) {
        ui->label_status->setText("请先输入账号密码并选择角色");
        return;
    }

    // 连接服务器 (假设服务器IP和端口是固定的，也可以从界面输入)
    QString serverIp = "192.168.90.91"; // 实际使用时修改为你的服务器IP
    QString serverPort = "8080";    // 实际使用时修改为你的服务器端口
    //m_tcpSocket->sendLoginData(username, password,role);
    ui->label_status->setText("正在连接服务器...");

    // 连接服务器并发送登录数据
    if (m_tcpSocket->connectToServer(serverIp, serverPort, QIODevice::ReadWrite)) {
        ui->label_status->setText("正在登录...");
        // 发送登录数据
        m_tcpSocket->sendLoginData(username, password,role);
    } else {
        ui->label_status->setText("连接服务器失败: " + m_tcpSocket->errorString());
    }

}
// 获取选中的角色（注册页）
QString Dialog::getSelectedRoleAuth() {

    if (ui->factory->isChecked()) return "factory";
    else if (ui->expert->isChecked()) return "expert";
    else if(ui->admin->isChecked()) return "admin";
    return ""; // 未选择
}
// 获取选中的角色（登录页）
QString Dialog::getSelectedRoleRegi() {

    if (ui->factory2->isChecked()) return "factory";
    else if (ui->expert2->isChecked()) return "expert";
    return ""; // 未选择
}

// 登录失败处理
void Dialog::onLoginFailed(const QString &errorMsg)
{
    ui->label_status->setText(errorMsg);
    // 清空密码框
    ui->password->clear();
    if (m_tcpSocket) {
        // 将MyTcpSocket指针转换为基类QTcpSocket指针
        QTcpSocket* baseSocket = dynamic_cast<QTcpSocket*>(m_tcpSocket);

        // 断开连接
        m_tcpSocket->disconnectFromHost();

        // 通过基类指针调用state()和waitForDisconnected()
        if (baseSocket && baseSocket->state() != QAbstractSocket::UnconnectedState) {
            baseSocket->waitForDisconnected(); // 等待连接断开
        }
        /*
        // 销毁socket实例
        delete m_tcpSocket;
        m_tcpSocket = nullptr;
        */
    }


}

void Dialog::onRegisterClicked()
{
    QString username = ui->username2->text().trimmed();
    QString password = ui->password2->text().trimmed();
    QString email = ui->address->text().trimmed();
    QString confirm = ui->confirm->text().trimmed();
    if(confirm != password){
        ui->label_status_2->setText("确认密码错误，请重新输入");
        ui->password2->clear();
        ui->confirm->clear();
    }
    QString role = getSelectedRoleRegi();

    // 输入验证
    if (username.isEmpty() || password.isEmpty() || email.isEmpty() || role.isEmpty()) {
        ui->label_status_2->setText("请完善注册信息并选择角色");
        return;
    }
    // m_tcpSocket->sendRegisterData(username, password, email, role);

    // 连接服务器并发送注册数据
    QString serverIp = "192.168.90.91";
    QString serverPort = "8080";
    ui->label_status_2->setText("正在连接服务器...");
    if (m_tcpSocket->connectToServer(serverIp, serverPort, QIODevice::ReadWrite)) {
        ui->label_status_2->setText("正在注册...");
        m_tcpSocket->sendRegisterData(username, password, email, role);
    } else {
        ui->label_status_2->setText("连接服务器失败: " + m_tcpSocket->errorString());
    }

}
/*
// 处理注册响应
void Dialog::onRegisterResponse(bool success, const QString &message)
{
    if(success) {
        ui->label_status->setText("注册成功");
        QMessageBox::information(this, "注册成功", "账号已创建，请登录");
        // 切换到登录界面
        ui->stackedWidget->setCurrentIndex(0);
    } else {
        ui->label_status->setText("注册失败: " + message);
        ui->password2->clear();
    }
}
*/
// 更新登录成功处理 - 根据角色启用功能
void Dialog::onLoginSuccess(const QString &role)
{
    qDebug() << "Login success! Role received:" << role; // 调试输出

    ui->label_status->setText("登录成功，正在进入系统...");
    // 1. 断开TCP连接并销毁socket实例
    if (m_tcpSocket) {
        // 将MyTcpSocket指针转换为基类QTcpSocket指针
        QTcpSocket* baseSocket = dynamic_cast<QTcpSocket*>(m_tcpSocket);

        // 断开连接
        m_tcpSocket->disconnectFromHost();

        // 通过基类指针调用state()和waitForDisconnected()
        if (baseSocket && baseSocket->state() != QAbstractSocket::UnconnectedState) {
            baseSocket->waitForDisconnected(); // 等待连接断开
        }

        // 销毁socket实例
        delete m_tcpSocket;
        m_tcpSocket = nullptr;
    }

    //传递role参数
    if(role =="admin"){
        Widget *mainWidget = new Widget();
        mainWidget->setUserRole(role); // 传递角色信息
        mainWidget->show();
        this->close();
    }

    Widget *mainWidget = new Widget();
    // mainWidget->setTcpSocket(m_tcpSocket);
    mainWidget->setUserRole(role); // 传递角色信息
    mainWidget->show();
    this->close();
}

void Dialog::setTcpSocket(MyTcpSocket *socket)
{
    // 断开旧socket的连接
    if (m_tcpSocket) {
        disconnect(m_tcpSocket, &MyTcpSocket::loginSuccess, this, &Dialog::onLoginSuccess);
        disconnect(m_tcpSocket, &MyTcpSocket::loginFailed, this, &Dialog::onLoginFailed);
        disconnect(m_tcpSocket, &MyTcpSocket::registerSuccess, this, &Dialog::onRegisterSuccess);
        disconnect(m_tcpSocket, &MyTcpSocket::registerFailed, this, &Dialog::onRegisterFailed);
    }

    m_tcpSocket = socket; // 使用外部socket

    // 重新连接信号
    connect(m_tcpSocket, &MyTcpSocket::loginSuccess,
            this, &Dialog::onLoginSuccess, Qt::AutoConnection);
    connect(m_tcpSocket, &MyTcpSocket::loginFailed,
            this, &Dialog::onLoginFailed);
    connect(m_tcpSocket, &MyTcpSocket::registerSuccess,
            this, &Dialog::onRegisterSuccess);
    connect(m_tcpSocket, &MyTcpSocket::registerFailed,
            this, &Dialog::onRegisterFailed);
}

void Dialog::onBackLoginClicked()
{
    // 切换到登录界面
    ui->stackedWidget->setCurrentIndex(0);
}

void Dialog::onGoRegisterClicked()
{
    // 切换到登录界面
    ui->stackedWidget->setCurrentIndex(1);
}

// 注册成功处理
void Dialog::onRegisterSuccess(const QString &msg) {
    ui->label_status_2->setText(msg);
    ui->label_status_2->setText("注册成功");
    if(ui->factory2->isChecked()){
        ui->factory2->setChecked(false);
        ui->expert2->setChecked(false);
    }
    if(ui->expert2->isChecked()){
        ui->factory2->setChecked(false);
        ui->expert2->setChecked(false);
    }
    QMessageBox::information(this, "已提交注册信息", "待管理员审核");
    ui->stackedWidget->setCurrentIndex(0); // 切回登录页
    if (m_tcpSocket) {
        // 将MyTcpSocket指针转换为基类QTcpSocket指针
        QTcpSocket* baseSocket = dynamic_cast<QTcpSocket*>(m_tcpSocket);

        // 断开连接
        m_tcpSocket->disconnectFromHost();

        // 通过基类指针调用state()和waitForDisconnected()
        if (baseSocket && baseSocket->state() != QAbstractSocket::UnconnectedState) {
            baseSocket->waitForDisconnected(); // 等待连接断开
        }
        /*
        // 销毁socket实例
        delete m_tcpSocket;
        m_tcpSocket = nullptr;
        */
    }


}
// 注册失败处理
void Dialog::onRegisterFailed(const QString &errorMsg) {
    ui->label_status_2->setText("注册失败: " + errorMsg);
    ui->password2->clear();
}