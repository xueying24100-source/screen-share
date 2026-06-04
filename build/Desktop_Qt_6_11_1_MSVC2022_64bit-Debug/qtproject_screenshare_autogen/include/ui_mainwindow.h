/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout_2;
    QWidget *widget;
    QWidget *topUserArea;
    QHBoxLayout *horizontalLayout;
    QLabel *labelSmall1;
    QLabel *labelSmall2;
    QLabel *labelSmall3;
    QLabel *labelSmall4;
    QLabel *labelSmall5;
    QLabel *labelMainScreen;
    QWidget *bottomBar;
    QLabel *labelStatus;
    QPushButton *btnShare;
    QPushButton *btnEnd;
    QPushButton *btnAnnotate;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1004, 707);
        MainWindow->setMinimumSize(QSize(720, 480));
        MainWindow->setStyleSheet(QString::fromUtf8("QWidget {\n"
"    background-color: white;\n"
"    font-family: \"Microsoft YaHei\";\n"
"    color: #1f2329;\n"
"}\n"
"\n"
"QLabel#labelMainScreen {\n"
"    background-color: #f2f3f5;\n"
"    border: 2px solid #dcdfe6;\n"
"    border-radius: 14px;\n"
"    color: #8a94a6;\n"
"    font-size: 24px;\n"
"    font-weight: bold;\n"
"}\n"
"\n"
"QLabel#labelSmall1,\n"
"QLabel#labelSmall2,\n"
"QLabel#labelSmall3,\n"
"QLabel#labelSmall4,\n"
"QLabel#labelSmall5 {\n"
"    background-color: #ffffff;\n"
"    border: 1px solid #dcdfe6;\n"
"    border-radius: 12px;\n"
"    font-size: 18px;\n"
"    font-weight: bold;\n"
"    color: #334155;\n"
"}\n"
"\n"
"QLabel#labelStatus {\n"
"    font-size: 16px;\n"
"    font-weight: bold;\n"
"    color: #1f2329;\n"
"}\n"
"\n"
"QPushButton {\n"
"    border: none;\n"
"    border-radius: 10px;\n"
"    font-size: 18px;\n"
"    font-weight: bold;\n"
"    min-width: 120px;\n"
"    min-height: 42px;\n"
"}\n"
"\n"
"QPushButton#btnShare {\n"
"    background-color: #1677ff;\n"
"    color: white;\n"
""
                        "}\n"
"\n"
"QPushButton#btnShare:hover {\n"
"    background-color: #4096ff;\n"
"}\n"
"\n"
"QPushButton#btnEnd {\n"
"    background-color: #ff4d4f;\n"
"    color: white;\n"
"}\n"
"\n"
"QPushButton#btnEnd:hover {\n"
"    background-color: #ff7875;\n"
"}\n"
"\n"
"QPushButton#btnAnnotate {\n"
"    background-color: #22c55e;\n"
"    color: white;\n"
"}\n"
"\n"
"QPushButton#btnAnnotate:hover {\n"
"    background-color: #4ade80;\n"
"}\n"
"\n"
"QPushButton#btnAnnotate:disabled {\n"
"    background-color: #cbd5e1;\n"
"    color: #64748b;\n"
"}"));
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        horizontalLayout_2 = new QHBoxLayout(centralwidget);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        widget = new QWidget(centralwidget);
        widget->setObjectName("widget");
        topUserArea = new QWidget(widget);
        topUserArea->setObjectName("topUserArea");
        topUserArea->setGeometry(QRect(10, 10, 971, 120));
        topUserArea->setMinimumSize(QSize(0, 120));
        topUserArea->setMaximumSize(QSize(16777215, 150));
        horizontalLayout = new QHBoxLayout(topUserArea);
        horizontalLayout->setObjectName("horizontalLayout");
        labelSmall1 = new QLabel(topUserArea);
        labelSmall1->setObjectName("labelSmall1");
        labelSmall1->setAlignment(Qt::AlignmentFlag::AlignCenter);

        horizontalLayout->addWidget(labelSmall1);

        labelSmall2 = new QLabel(topUserArea);
        labelSmall2->setObjectName("labelSmall2");
        labelSmall2->setAlignment(Qt::AlignmentFlag::AlignCenter);

        horizontalLayout->addWidget(labelSmall2);

        labelSmall3 = new QLabel(topUserArea);
        labelSmall3->setObjectName("labelSmall3");
        labelSmall3->setAlignment(Qt::AlignmentFlag::AlignCenter);

        horizontalLayout->addWidget(labelSmall3);

        labelSmall4 = new QLabel(topUserArea);
        labelSmall4->setObjectName("labelSmall4");
        labelSmall4->setAlignment(Qt::AlignmentFlag::AlignCenter);

        horizontalLayout->addWidget(labelSmall4);

        labelSmall5 = new QLabel(topUserArea);
        labelSmall5->setObjectName("labelSmall5");
        labelSmall5->setAlignment(Qt::AlignmentFlag::AlignCenter);

        horizontalLayout->addWidget(labelSmall5);

        labelMainScreen = new QLabel(widget);
        labelMainScreen->setObjectName("labelMainScreen");
        labelMainScreen->setGeometry(QRect(10, 130, 961, 401));
        labelMainScreen->setMinimumSize(QSize(800, 30));
        labelMainScreen->setStyleSheet(QString::fromUtf8(""));
        labelMainScreen->setAlignment(Qt::AlignmentFlag::AlignCenter);
        bottomBar = new QWidget(widget);
        bottomBar->setObjectName("bottomBar");
        bottomBar->setGeometry(QRect(0, 550, 971, 80));
        bottomBar->setMinimumSize(QSize(0, 70));
        bottomBar->setMaximumSize(QSize(16777215, 80));
        labelStatus = new QLabel(bottomBar);
        labelStatus->setObjectName("labelStatus");
        labelStatus->setGeometry(QRect(100, 30, 191, 31));
        btnShare = new QPushButton(bottomBar);
        btnShare->setObjectName("btnShare");
        btnShare->setGeometry(QRect(390, 30, 120, 42));
        btnShare->setMinimumSize(QSize(120, 42));
        btnEnd = new QPushButton(bottomBar);
        btnEnd->setObjectName("btnEnd");
        btnEnd->setGeometry(QRect(590, 30, 120, 42));
        btnEnd->setMinimumSize(QSize(120, 42));
        btnAnnotate = new QPushButton(bottomBar);
        btnAnnotate->setObjectName("btnAnnotate");
        btnAnnotate->setGeometry(QRect(740, 30, 120, 42));
        btnAnnotate->setMinimumSize(QSize(120, 42));
        btnAnnotate->setEnabled(false);

        horizontalLayout_2->addWidget(widget);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 1004, 22));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "\345\261\200\345\237\237\347\275\221\345\261\217\345\271\225\345\205\261\344\272\253demo", nullptr));
        labelSmall1->setText(QCoreApplication::translate("MainWindow", "\347\224\250\346\210\2671", nullptr));
        labelSmall2->setText(QCoreApplication::translate("MainWindow", "\347\224\250\346\210\2672", nullptr));
        labelSmall3->setText(QCoreApplication::translate("MainWindow", "\347\224\250\346\210\2673", nullptr));
        labelSmall4->setText(QCoreApplication::translate("MainWindow", "\347\224\250\346\210\2674", nullptr));
        labelSmall5->setText(QCoreApplication::translate("MainWindow", "\347\224\250\346\210\2675", nullptr));
        labelMainScreen->setText(QCoreApplication::translate("MainWindow", "\347\255\211\345\276\205\345\205\261\344\272\253", nullptr));
        labelStatus->setText(QCoreApplication::translate("MainWindow", "\347\212\266\346\200\201\357\274\232\346\234\252\345\205\261\344\272\253", nullptr));
        btnShare->setText(QCoreApplication::translate("MainWindow", "\345\274\200\345\247\213\345\205\261\344\272\253", nullptr));
        btnEnd->setText(QCoreApplication::translate("MainWindow", "\347\273\223\346\235\237\345\205\261\344\272\253", nullptr));
        btnAnnotate->setText(QCoreApplication::translate("MainWindow", "\347\224\273\347\254\224", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
