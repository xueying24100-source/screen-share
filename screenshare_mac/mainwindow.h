#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>

class LoginPage;
class RoomPage;
class RoomServer;
class RoomClient;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupUI();
    void onJoinRoom(const QString &nickname, const QString &roomId,
                    const QString &serverHost, quint16 serverPort);
    void onLeaveRoom();

    QStackedWidget *m_stackWidget;
    LoginPage *m_loginPage;
    RoomPage *m_roomPage;

    RoomServer *m_server;
    RoomClient *m_client;

    QString m_nickname;
    QString m_roomId;
};

#endif // MAINWINDOW_H
