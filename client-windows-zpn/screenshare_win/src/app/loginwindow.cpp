#include "loginwindow.h"
#include "ui_loginwindow.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LoginWindow)
{
    ui->setupUi(this);
    setupUiLogic();
    restoreRememberedUser();
    centerOnScreen();
}

LoginWindow::~LoginWindow()
{
    delete ui;
}

void LoginWindow::setupUiLogic()
{
    setWindowTitle("屏幕共享 Demo - 登录");
    setFixedSize(500, 640);

    ui->editLoginUsername->setClearButtonEnabled(true);
    ui->editLoginPassword->setClearButtonEnabled(true);
    ui->editRegisterUsername->setClearButtonEnabled(true);
    ui->editRegisterNickname->setClearButtonEnabled(true);
    ui->editRegisterPassword->setClearButtonEnabled(true);
    ui->editRegisterConfirm->setClearButtonEnabled(true);

    connect(ui->btnLogin, &QPushButton::clicked, this, &LoginWindow::handleLogin);
    connect(ui->editLoginUsername, &QLineEdit::returnPressed, this, &LoginWindow::handleLogin);
    connect(ui->editLoginPassword, &QLineEdit::returnPressed, this, &LoginWindow::handleLogin);

    connect(ui->btnRegister, &QPushButton::clicked, this, &LoginWindow::handleRegister);
    connect(ui->editRegisterConfirm, &QLineEdit::returnPressed, this, &LoginWindow::handleRegister);
}

void LoginWindow::restoreRememberedUser()
{
    QSettings settings;
    const QString remembered = settings.value("login/username").toString();
    if (!remembered.isEmpty()) {
        ui->editLoginUsername->setText(remembered);
        ui->checkRememberUser->setChecked(true);
        ui->editLoginPassword->setFocus();
    } else {
        ui->editLoginUsername->setFocus();
    }
}

void LoginWindow::centerOnScreen()
{
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        move(available.center() - rect().center());
    }
}

void LoginWindow::handleLogin()
{
    const QString username = ui->editLoginUsername->text().trimmed();
    const QString password = ui->editLoginPassword->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "提示", "账号或密码不能为空。没有账号的话，请先点击“注册”。");
        return;
    }

    QJsonObject root;
    if (!loadUsers(root)) {
        return;
    }

    const QJsonObject users = root.value("users").toObject();
    if (users.isEmpty()) {
        QMessageBox::information(this, "提示", "当前还没有本地账号，请先注册一个账号。");
        ui->tabWidget->setCurrentIndex(1);
        ui->editRegisterUsername->setText(username);
        ui->editRegisterPassword->setFocus();
        return;
    }

    if (!users.contains(username)) {
        QMessageBox::warning(this, "登录失败", "账号不存在，请检查账号或先注册。");
        return;
    }

    const QJsonObject user = users.value(username).toObject();
    const QString salt = user.value("salt").toString();
    const QString expectedHash = user.value("passwordHash").toString();
    if (passwordHash(password, salt) != expectedHash) {
        QMessageBox::warning(this, "登录失败", "密码错误。");
        return;
    }

    QSettings settings;
    if (ui->checkRememberUser->isChecked()) {
        settings.setValue("login/username", username);
    } else {
        settings.remove("login/username");
    }

    openMainWindow(username);
}

void LoginWindow::handleRegister()
{
    const QString username = ui->editRegisterUsername->text().trimmed();
    const QString nickname = ui->editRegisterNickname->text().trimmed();
    const QString password = ui->editRegisterPassword->text();
    const QString confirm = ui->editRegisterConfirm->text();

    static const QRegularExpression usernameRule("^[A-Za-z0-9_]{3,20}$");
    if (!usernameRule.match(username).hasMatch()) {
        QMessageBox::warning(this, "注册失败", "账号需要 3-20 位，只能包含字母、数字和下划线。");
        return;
    }

    if (password.length() < 6) {
        QMessageBox::warning(this, "注册失败", "密码至少需要 6 位。");
        return;
    }

    if (password != confirm) {
        QMessageBox::warning(this, "注册失败", "两次输入的密码不一致。");
        return;
    }

    QJsonObject root;
    if (!loadUsers(root)) {
        return;
    }

    QJsonObject users = root.value("users").toObject();
    if (users.contains(username)) {
        QMessageBox::warning(this, "注册失败", "该账号已经存在，请换一个账号。");
        return;
    }

    const QString salt = createSalt();
    QJsonObject user;
    user.insert("nickname", nickname.isEmpty() ? username : nickname);
    user.insert("salt", salt);
    user.insert("passwordHash", passwordHash(password, salt));
    user.insert("createdAt", QDateTime::currentDateTime().toString(Qt::ISODate));

    users.insert(username, user);
    root.insert("users", users);

    if (!saveUsers(root)) {
        return;
    }

    QMessageBox::information(this, "注册成功", "账号创建成功，现在可以登录。");
    ui->tabWidget->setCurrentIndex(0);
    ui->editLoginUsername->setText(username);
    ui->editLoginPassword->clear();
    ui->editLoginPassword->setFocus();

    ui->editRegisterPassword->clear();
    ui->editRegisterConfirm->clear();
}

void LoginWindow::openMainWindow(const QString &username)
{
    auto *mainWin = new MainWindow;
    mainWin->setAttribute(Qt::WA_DeleteOnClose, true);
    mainWin->setWindowTitle(QString("屏幕共享 Demo - %1").arg(username));
    mainWin->show();
    close();
}

QString LoginWindow::usersFilePath() const
{
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dirPath.isEmpty()) {
        dirPath = QApplication::applicationDirPath() + "/data";
    }

    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return dir.filePath("users.json");
}

bool LoginWindow::loadUsers(QJsonObject &root) const
{
    const QString path = usersFilePath();
    QFile file(path);
    if (!file.exists()) {
        root = QJsonObject{};
        root.insert("users", QJsonObject{});
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(nullptr, "错误", "无法读取用户文件：\n" + path);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::critical(nullptr, "错误", "用户文件格式损坏，请检查：\n" + path);
        return false;
    }

    root = doc.object();
    if (!root.contains("users") || !root.value("users").isObject()) {
        root.insert("users", QJsonObject{});
    }
    return true;
}

bool LoginWindow::saveUsers(const QJsonObject &root) const
{
    const QString path = usersFilePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(nullptr, "错误", "无法保存用户文件：\n" + path);
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

QString LoginWindow::createSalt() const
{
    QByteArray bytes;
    bytes.resize(16);
    for (int i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return QString::fromLatin1(bytes.toBase64());
}

QString LoginWindow::passwordHash(const QString &password, const QString &salt) const
{
    const QByteArray data = salt.toUtf8() + ':' + password.toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}
