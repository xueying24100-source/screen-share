/********************************************************************************
** Form generated from reading UI file 'loginwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOGINWINDOW_H
#define UI_LOGINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_LoginWindow
{
public:
    QVBoxLayout *verticalLayoutRoot;
    QVBoxLayout *verticalLayoutHero;
    QLabel *labelHeroIcon;
    QLabel *labelHeroTitle;
    QLabel *labelHeroSubTitle;
    QFrame *frameCard;
    QVBoxLayout *verticalLayoutCard;
    QTabWidget *tabWidget;
    QWidget *loginTab;
    QVBoxLayout *verticalLayoutLogin;
    QLabel *labelLoginUsername;
    QLineEdit *editLoginUsername;
    QLabel *labelLoginPassword;
    QLineEdit *editLoginPassword;
    QCheckBox *checkRememberUser;
    QSpacerItem *verticalSpacerLoginSmall;
    QPushButton *btnLogin;
    QSpacerItem *verticalSpacerLogin;
    QWidget *registerTab;
    QVBoxLayout *verticalLayoutRegister;
    QLabel *labelRegisterUsername;
    QLineEdit *editRegisterUsername;
    QLabel *labelRegisterNickname;
    QLineEdit *editRegisterNickname;
    QLabel *labelRegisterPassword;
    QLineEdit *editRegisterPassword;
    QLabel *labelRegisterConfirm;
    QLineEdit *editRegisterConfirm;
    QSpacerItem *verticalSpacerRegisterSmall;
    QPushButton *btnRegister;
    QSpacerItem *verticalSpacerRegister;
    QLabel *labelHint;
    QSpacerItem *verticalSpacerBottom;

    void setupUi(QWidget *LoginWindow)
    {
        if (LoginWindow->objectName().isEmpty())
            LoginWindow->setObjectName("LoginWindow");
        LoginWindow->resize(500, 640);
        LoginWindow->setMinimumSize(QSize(500, 640));
        LoginWindow->setMaximumSize(QSize(500, 640));
        LoginWindow->setStyleSheet(QString::fromUtf8("QWidget {\n"
"    background: #f5f7fb;\n"
"    font-family: \"Microsoft YaHei UI\", \"Microsoft YaHei\", \"Segoe UI\";\n"
"    color: #1f2329;\n"
"}\n"
"\n"
"QLabel#labelHeroIcon {\n"
"    background: #1677ff;\n"
"    color: white;\n"
"    border-radius: 30px;\n"
"    font-size: 27px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QLabel#labelHeroTitle {\n"
"    color: #101828;\n"
"    font-size: 25px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QLabel#labelHeroSubTitle {\n"
"    color: #667085;\n"
"    font-size: 13px;\n"
"}\n"
"\n"
"QFrame#frameCard {\n"
"    background: #ffffff;\n"
"    border: 1px solid #edf0f5;\n"
"    border-radius: 18px;\n"
"}\n"
"\n"
"QTabWidget {\n"
"    background: #ffffff;\n"
"}\n"
"\n"
"QTabWidget::pane {\n"
"    border: none;\n"
"    top: -1px;\n"
"    background: #ffffff;\n"
"}\n"
"\n"
"QTabBar::tab {\n"
"    min-width: 138px;\n"
"    min-height: 36px;\n"
"    margin: 0 5px;\n"
"    border-radius: 18px;\n"
"    color: #667085;\n"
"    background: #f2f4f7;\n"
"    font-size: 14px;\n"
"}\n"
""
                        "\n"
"QTabBar::tab:selected {\n"
"    background: #1677ff;\n"
"    color: white;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QLabel#labelLoginUsername,\n"
"QLabel#labelLoginPassword,\n"
"QLabel#labelRegisterUsername,\n"
"QLabel#labelRegisterNickname,\n"
"QLabel#labelRegisterPassword,\n"
"QLabel#labelRegisterConfirm {\n"
"    background: transparent;\n"
"    color: #344054;\n"
"    font-size: 13px;\n"
"    font-weight: 600;\n"
"}\n"
"\n"
"QLineEdit {\n"
"    border: 1px solid #d0d5dd;\n"
"    border-radius: 9px;\n"
"    padding: 0 12px;\n"
"    background: white;\n"
"    color: #101828;\n"
"    font-size: 14px;\n"
"    min-height: 40px;\n"
"}\n"
"\n"
"QLineEdit:focus {\n"
"    border: 1px solid #1677ff;\n"
"}\n"
"\n"
"QPushButton#btnLogin,\n"
"QPushButton#btnRegister {\n"
"    background: #1677ff;\n"
"    color: white;\n"
"    border: none;\n"
"    border-radius: 9px;\n"
"    min-height: 42px;\n"
"    font-size: 15px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QPushButton#btnLogin:hover,\n"
"QPushButton#btnRegister:h"
                        "over {\n"
"    background: #4096ff;\n"
"}\n"
"\n"
"QPushButton#btnLogin:pressed,\n"
"QPushButton#btnRegister:pressed {\n"
"    background: #0958d9;\n"
"}\n"
"\n"
"QCheckBox {\n"
"    background: transparent;\n"
"    color: #667085;\n"
"    font-size: 13px;\n"
"}\n"
"\n"
"QLabel#labelHint {\n"
"    background: transparent;\n"
"    color: #667085;\n"
"    font-size: 12px;\n"
"}"));
        verticalLayoutRoot = new QVBoxLayout(LoginWindow);
        verticalLayoutRoot->setSpacing(18);
        verticalLayoutRoot->setObjectName("verticalLayoutRoot");
        verticalLayoutRoot->setContentsMargins(36, 28, 36, 24);
        verticalLayoutHero = new QVBoxLayout();
        verticalLayoutHero->setSpacing(8);
        verticalLayoutHero->setObjectName("verticalLayoutHero");
        labelHeroIcon = new QLabel(LoginWindow);
        labelHeroIcon->setObjectName("labelHeroIcon");
        labelHeroIcon->setMinimumSize(QSize(60, 60));
        labelHeroIcon->setMaximumSize(QSize(60, 60));
        labelHeroIcon->setAlignment(Qt::AlignmentFlag::AlignCenter);

        verticalLayoutHero->addWidget(labelHeroIcon);

        labelHeroTitle = new QLabel(LoginWindow);
        labelHeroTitle->setObjectName("labelHeroTitle");
        labelHeroTitle->setAlignment(Qt::AlignmentFlag::AlignCenter);

        verticalLayoutHero->addWidget(labelHeroTitle);

        labelHeroSubTitle = new QLabel(LoginWindow);
        labelHeroSubTitle->setObjectName("labelHeroSubTitle");
        labelHeroSubTitle->setAlignment(Qt::AlignmentFlag::AlignCenter);
        labelHeroSubTitle->setWordWrap(true);

        verticalLayoutHero->addWidget(labelHeroSubTitle);


        verticalLayoutRoot->addLayout(verticalLayoutHero);

        frameCard = new QFrame(LoginWindow);
        frameCard->setObjectName("frameCard");
        frameCard->setMinimumSize(QSize(408, 392));
        frameCard->setMaximumSize(QSize(408, 16777215));
        frameCard->setFrameShape(QFrame::Shape::StyledPanel);
        frameCard->setFrameShadow(QFrame::Shadow::Raised);
        verticalLayoutCard = new QVBoxLayout(frameCard);
        verticalLayoutCard->setSpacing(14);
        verticalLayoutCard->setObjectName("verticalLayoutCard");
        verticalLayoutCard->setContentsMargins(26, 24, 26, 18);
        tabWidget = new QTabWidget(frameCard);
        tabWidget->setObjectName("tabWidget");
        loginTab = new QWidget();
        loginTab->setObjectName("loginTab");
        verticalLayoutLogin = new QVBoxLayout(loginTab);
        verticalLayoutLogin->setSpacing(9);
        verticalLayoutLogin->setObjectName("verticalLayoutLogin");
        verticalLayoutLogin->setContentsMargins(2, 18, 2, 0);
        labelLoginUsername = new QLabel(loginTab);
        labelLoginUsername->setObjectName("labelLoginUsername");

        verticalLayoutLogin->addWidget(labelLoginUsername);

        editLoginUsername = new QLineEdit(loginTab);
        editLoginUsername->setObjectName("editLoginUsername");
        editLoginUsername->setClearButtonEnabled(true);

        verticalLayoutLogin->addWidget(editLoginUsername);

        labelLoginPassword = new QLabel(loginTab);
        labelLoginPassword->setObjectName("labelLoginPassword");

        verticalLayoutLogin->addWidget(labelLoginPassword);

        editLoginPassword = new QLineEdit(loginTab);
        editLoginPassword->setObjectName("editLoginPassword");
        editLoginPassword->setEchoMode(QLineEdit::EchoMode::Password);
        editLoginPassword->setClearButtonEnabled(true);

        verticalLayoutLogin->addWidget(editLoginPassword);

        checkRememberUser = new QCheckBox(loginTab);
        checkRememberUser->setObjectName("checkRememberUser");

        verticalLayoutLogin->addWidget(checkRememberUser);

        verticalSpacerLoginSmall = new QSpacerItem(20, 4, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayoutLogin->addItem(verticalSpacerLoginSmall);

        btnLogin = new QPushButton(loginTab);
        btnLogin->setObjectName("btnLogin");

        verticalLayoutLogin->addWidget(btnLogin);

        verticalSpacerLogin = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayoutLogin->addItem(verticalSpacerLogin);

        tabWidget->addTab(loginTab, QString());
        registerTab = new QWidget();
        registerTab->setObjectName("registerTab");
        verticalLayoutRegister = new QVBoxLayout(registerTab);
        verticalLayoutRegister->setSpacing(7);
        verticalLayoutRegister->setObjectName("verticalLayoutRegister");
        verticalLayoutRegister->setContentsMargins(2, 14, 2, 0);
        labelRegisterUsername = new QLabel(registerTab);
        labelRegisterUsername->setObjectName("labelRegisterUsername");

        verticalLayoutRegister->addWidget(labelRegisterUsername);

        editRegisterUsername = new QLineEdit(registerTab);
        editRegisterUsername->setObjectName("editRegisterUsername");
        editRegisterUsername->setClearButtonEnabled(true);

        verticalLayoutRegister->addWidget(editRegisterUsername);

        labelRegisterNickname = new QLabel(registerTab);
        labelRegisterNickname->setObjectName("labelRegisterNickname");

        verticalLayoutRegister->addWidget(labelRegisterNickname);

        editRegisterNickname = new QLineEdit(registerTab);
        editRegisterNickname->setObjectName("editRegisterNickname");
        editRegisterNickname->setClearButtonEnabled(true);

        verticalLayoutRegister->addWidget(editRegisterNickname);

        labelRegisterPassword = new QLabel(registerTab);
        labelRegisterPassword->setObjectName("labelRegisterPassword");

        verticalLayoutRegister->addWidget(labelRegisterPassword);

        editRegisterPassword = new QLineEdit(registerTab);
        editRegisterPassword->setObjectName("editRegisterPassword");
        editRegisterPassword->setEchoMode(QLineEdit::EchoMode::Password);
        editRegisterPassword->setClearButtonEnabled(true);

        verticalLayoutRegister->addWidget(editRegisterPassword);

        labelRegisterConfirm = new QLabel(registerTab);
        labelRegisterConfirm->setObjectName("labelRegisterConfirm");

        verticalLayoutRegister->addWidget(labelRegisterConfirm);

        editRegisterConfirm = new QLineEdit(registerTab);
        editRegisterConfirm->setObjectName("editRegisterConfirm");
        editRegisterConfirm->setEchoMode(QLineEdit::EchoMode::Password);
        editRegisterConfirm->setClearButtonEnabled(true);

        verticalLayoutRegister->addWidget(editRegisterConfirm);

        verticalSpacerRegisterSmall = new QSpacerItem(20, 4, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayoutRegister->addItem(verticalSpacerRegisterSmall);

        btnRegister = new QPushButton(registerTab);
        btnRegister->setObjectName("btnRegister");

        verticalLayoutRegister->addWidget(btnRegister);

        verticalSpacerRegister = new QSpacerItem(20, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayoutRegister->addItem(verticalSpacerRegister);

        tabWidget->addTab(registerTab, QString());

        verticalLayoutCard->addWidget(tabWidget);

        labelHint = new QLabel(frameCard);
        labelHint->setObjectName("labelHint");
        labelHint->setAlignment(Qt::AlignmentFlag::AlignCenter);
        labelHint->setWordWrap(true);

        verticalLayoutCard->addWidget(labelHint);


        verticalLayoutRoot->addWidget(frameCard, 0, Qt::AlignmentFlag::AlignHCenter);

        verticalSpacerBottom = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayoutRoot->addItem(verticalSpacerBottom);


        retranslateUi(LoginWindow);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(LoginWindow);
    } // setupUi

    void retranslateUi(QWidget *LoginWindow)
    {
        LoginWindow->setWindowTitle(QCoreApplication::translate("LoginWindow", "\345\261\217\345\271\225\345\205\261\344\272\253 Demo - \347\231\273\345\275\225", nullptr));
        labelHeroIcon->setText(QCoreApplication::translate("LoginWindow", "\344\274\232", nullptr));
        labelHeroTitle->setText(QCoreApplication::translate("LoginWindow", "\345\261\217\345\271\225\345\205\261\344\272\253 Demo", nullptr));
        labelHeroSubTitle->setText(QCoreApplication::translate("LoginWindow", "\346\263\250\345\206\214\346\234\254\345\234\260\350\264\246\345\217\267\345\220\216\347\231\273\345\275\225\357\274\214\350\277\233\345\205\245\345\205\261\344\272\253\344\270\216\347\224\273\347\254\224\346\240\207\346\263\250\346\274\224\347\244\272\347\225\214\351\235\242", nullptr));
        labelLoginUsername->setText(QCoreApplication::translate("LoginWindow", "\350\264\246\345\217\267", nullptr));
        editLoginUsername->setPlaceholderText(QCoreApplication::translate("LoginWindow", "\350\257\267\350\276\223\345\205\245\350\264\246\345\217\267", nullptr));
        labelLoginPassword->setText(QCoreApplication::translate("LoginWindow", "\345\257\206\347\240\201", nullptr));
        editLoginPassword->setPlaceholderText(QCoreApplication::translate("LoginWindow", "\350\257\267\350\276\223\345\205\245\345\257\206\347\240\201", nullptr));
        checkRememberUser->setText(QCoreApplication::translate("LoginWindow", "\350\256\260\344\275\217\350\264\246\345\217\267", nullptr));
        btnLogin->setText(QCoreApplication::translate("LoginWindow", "\347\231\273\345\275\225", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(loginTab), QCoreApplication::translate("LoginWindow", "\347\231\273\345\275\225", nullptr));
        labelRegisterUsername->setText(QCoreApplication::translate("LoginWindow", "\350\264\246\345\217\267", nullptr));
        editRegisterUsername->setPlaceholderText(QCoreApplication::translate("LoginWindow", "3-20 \344\275\215\357\274\214\346\224\257\346\214\201\345\255\227\346\257\215 / \346\225\260\345\255\227 / \344\270\213\345\210\222\347\272\277", nullptr));
        labelRegisterNickname->setText(QCoreApplication::translate("LoginWindow", "\346\230\265\347\247\260", nullptr));
        editRegisterNickname->setPlaceholderText(QCoreApplication::translate("LoginWindow", "\351\200\211\345\241\253\357\274\214\347\224\250\344\272\216\346\230\276\347\244\272\345\220\215\347\247\260", nullptr));
        labelRegisterPassword->setText(QCoreApplication::translate("LoginWindow", "\345\257\206\347\240\201", nullptr));
        editRegisterPassword->setPlaceholderText(QCoreApplication::translate("LoginWindow", "\350\207\263\345\260\221 6 \344\275\215\345\257\206\347\240\201", nullptr));
        labelRegisterConfirm->setText(QCoreApplication::translate("LoginWindow", "\347\241\256\350\256\244\345\257\206\347\240\201", nullptr));
        editRegisterConfirm->setPlaceholderText(QCoreApplication::translate("LoginWindow", "\345\206\215\346\254\241\350\276\223\345\205\245\345\257\206\347\240\201", nullptr));
        btnRegister->setText(QCoreApplication::translate("LoginWindow", "\345\210\233\345\273\272\350\264\246\345\217\267", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(registerTab), QCoreApplication::translate("LoginWindow", "\346\263\250\345\206\214", nullptr));
        labelHint->setText(QCoreApplication::translate("LoginWindow", "\350\264\246\345\217\267\344\277\241\346\201\257\344\277\235\345\255\230\345\234\250\346\234\254\346\234\272 users.json\357\274\214\347\224\250\344\272\216\350\257\276\347\250\213 Demo\357\274\233\344\270\215\346\230\257\350\201\224\347\275\221\345\220\216\345\217\260\343\200\202", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LoginWindow: public Ui_LoginWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOGINWINDOW_H
