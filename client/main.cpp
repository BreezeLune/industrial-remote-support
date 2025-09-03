#include <QApplication>
#include "widget.h"
#include "screen.h"
#include <QTextCodec>
#include"dialog.h"
#include"mytcpsocket.h"
#include <QFile>
// 在main函数或窗口构造函数中加载
void loadStyleSheet()
{
    QFile styleFile(":/style/styles.qss");
    if (styleFile.open(QFile::ReadOnly))
    {
        QString styleSheet = QLatin1String(styleFile.readAll());
        qApp->setStyleSheet(styleSheet);
        styleFile.close();
    }
}

int main(int argc, char* argv[])
{
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF8"));
    QApplication app(argc, argv);
    Screen::init();
    // 3. 加载样式表（确保登录框和主界面都能应用样式）
    loadStyleSheet();
    // 创建TCP socket实例（全局使用，保持长连接）
    MyTcpSocket *tcpSocket = new MyTcpSocket;

    // 创建登录窗口
    Dialog loginDialog;
    // 将socket传递给登录窗口使用
    loginDialog.setTcpSocket(tcpSocket);
    /*
    QString serverIp = "192.168.90.91";
    QString serverPort = "8080";
    tcpSocket->connectToServer(serverIp, serverPort, QIODevice::ReadWrite);
*/
    //Widget mainWidget;
    loginDialog.show();
    // 将socket传递给主窗口使用

    //连接登录成功信号：登录成功后显示主窗口并传递角色信息
    QObject::connect(&loginDialog, &Dialog::loginSuccess, [&](const QString &role) {
        qDebug() << "登录成功，角色：" << role;
        //mainWidget.setTcpSocket(tcpSocket);
    });
    /*
    // 连接登录窗口关闭信号：如果登录窗口关闭且未登录成功，则退出程序
    QObject::connect(&loginDialog, &Dialog::rejected, [&]() {
        qDebug() << "登录窗口关闭，程序退出";
        app.quit();
    });
*/
    return app.exec();
}
