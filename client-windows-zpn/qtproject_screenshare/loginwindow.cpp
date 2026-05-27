#include "loginwindow.h"
#include "ui_loginwindow.h"
#include "mainwindow.h"

#include <QMessageBox>

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LoginWindow)
{
    ui->setupUi(this);

    // 最小化按钮
    connect(ui->btnMin, &QPushButton::clicked, this, &QWidget::showMinimized);

    // 关闭按钮
    connect(ui->btnClose, &QPushButton::clicked, this, &QWidget::close);

    // 登录按钮
    connect(ui->btnLogin, &QPushButton::clicked, this, [=]() {
        QString username = ui->editUsername->text();
        QString password = ui->editPassword->text();

        if (username.isEmpty() || password.isEmpty()) {
            QMessageBox::warning(this, "提示", "账号或密码不能为空");
            return;
        }

        // 这里先写死账号密码，后面可以改成数据库验证
        if (username == "admin" && password == "123456") {
            MainWindow *mainWin = new MainWindow;
            mainWin->show();
            this->close();
        } else {
            QMessageBox::warning(this, "登录失败", "账号或密码错误");
        }
    });
}

LoginWindow::~LoginWindow()
{
    delete ui;
}