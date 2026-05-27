#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QJsonObject>
#include <QWidget>

namespace Ui {
class LoginWindow;
}

class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow() override;

private:
    void setupUiLogic();
    void restoreRememberedUser();
    void centerOnScreen();

    void handleLogin();
    void handleRegister();
    void openMainWindow(const QString &username);

    QString usersFilePath() const;
    bool loadUsers(QJsonObject &root) const;
    bool saveUsers(const QJsonObject &root) const;
    QString createSalt() const;
    QString passwordHash(const QString &password, const QString &salt) const;

    Ui::LoginWindow *ui = nullptr;
};

#endif // LOGINWINDOW_H
