#ifndef LOGINPAGE_H
#define LOGINPAGE_H

#include <QWidget>

class QLineEdit;
class QLabel;

class LoginPage : public QWidget
{
    Q_OBJECT

public:
    explicit LoginPage(QWidget *parent = nullptr);

    void setServerStatus(const QString &text, bool success);

signals:
    void joinRoomRequested(const QString &nickname, const QString &roomId,
                           const QString &serverHost, quint16 serverPort);

private:
    void setupUI();
    void onJoinClicked();

    QLineEdit *m_nicknameEdit;
    QLineEdit *m_roomIdEdit;
    QLineEdit *m_serverEdit;
    QLineEdit *m_portEdit;
    QLabel *m_serverStatus;
};

#endif // LOGINPAGE_H
