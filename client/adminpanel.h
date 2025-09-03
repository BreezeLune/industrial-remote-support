#ifndef ADMINPANEL_H
#define ADMINPANEL_H

#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

class MyTcpSocket;

struct AdminUserInfo {
    int userId;
    QString username;
    QString email;
    QString company;
    QString requestedRole;
    QString status;
    int reviewedBy;
};

namespace Ui { class AdminPanel; }

class AdminPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AdminPanel(QWidget *parent = nullptr);
    ~AdminPanel();

    void setTcpSocket(MyTcpSocket* socket);
    void setCurrentUserId(int userId);
    void showAdminPanel();
    void hideAdminPanel();

private slots:
    void refreshButtonClicked();
    void approveButtonClicked();
    void rejectButtonClicked();
    void statusFilterChanged(const QString& status);
    void userTableSelectionChanged();
    void autoRefresh();

    void handleServerResponse(int msgType, const QByteArray& data);

private:
    Ui::AdminPanel *ui;
    MyTcpSocket *m_tcpSocket;
    int m_currentUserId;
    QTimer *m_refreshTimer;
    QVector<AdminUserInfo> m_allUsers;
    QVector<AdminUserInfo> m_filteredUsers;

    void setupTable();
    void loadPendingUsers();
    void updateTableData(const QVector<AdminUserInfo>& users);
    void sendGetPendingUsersRequest();
    void sendApproveUserRequest(int userId);
    void sendRejectUserRequest(int userId);
    void showMessage(const QString& title, const QString& message);
    void sendData(int msgType, const QByteArray& data);
    bool isConnected() const;
    void loadQssStyle();
    void setBuiltinStyle();
    // 根据表格当前选中行读取用户ID，再从列表中查找对应用户
    bool getSelectedUser(AdminUserInfo &outUser) const;
    int indexOfUserById(int userId, const QVector<AdminUserInfo> &list) const;
    static QString mapStatusToCn(const QString& raw);

signals:
    void approveUserResult(bool success, const QString& message);
    void rejectUserResult(bool success, const QString& message);
    void pendingUsersReceived(const QVector<AdminUserInfo>& users);
};

#endif // ADMINPANEL_H


