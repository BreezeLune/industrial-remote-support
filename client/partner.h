#ifndef PARTNER_H
#define PARTNER_H

#include <QLabel>
#include <QMenu>
#include <QContextMenuEvent> // 添加必要的头文件

class Partner : public QLabel
{
    Q_OBJECT
private:
    quint32 ip;

    void mousePressEvent(QMouseEvent *ev) override;
    int w;
    bool isSelf; // 标识是否是自己
public:
    Partner(QWidget * parent = nullptr, quint32 = 0);
    void setpic(QImage img);
    void contextMenuEvent(QContextMenuEvent* event) override;
signals:
    void sendip(quint32); //发送ip
    void kickRequested(quint32 ip); // 新增踢人信号
};

#endif // PARTNER_H
