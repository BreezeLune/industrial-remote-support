#ifndef DIALOG_H
#define DIALOG_H
#include <QDialog>
#include "widget.h"

namespace Ui {
class Dialog;
}

class Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = nullptr);
    // 新增：接收TCP socket的方法
    void setTcpSocket(MyTcpSocket *socket);
    ~Dialog();

private slots:
    void onLoginClicked(); // 登录按钮的槽函数（点击后执行）
    void onRegisterClicked(); // 注册按钮的槽函数（点击后执行）
    void onLoginSuccess(const QString &role);//登录成功
    void onLoginFailed(const QString &errorMsg);//登录失败
    //void onRegisterResponse(bool success, const QString &message);//注册回应
    void onBackLoginClicked();//返回登录
    void onGoRegisterClicked();
    void onRegisterSuccess(const QString &msg);  // 新增注册成功槽
    void onRegisterFailed(const QString &errorMsg);  // 新增注册失败槽



signals:
    // 登录验证结果信号（成功时返回角色，失败时返回错误信息）
    void loginSuccess(const QString &role);  // 登录成功，携带角色信息
    void loginFailed(const QString &errorMsg); // 登录失败，携带错误原因

private:
    Ui::Dialog *ui;
    Widget *m_targetWidget;
    MyTcpSocket *m_tcpSocket;    // 用于存储目标 Widget 实例
    QString getSelectedRoleAuth();  // 获取选中的角色
    QString getSelectedRoleRegi();  // 获取选中的角色
};

#endif // DIALOG_H
