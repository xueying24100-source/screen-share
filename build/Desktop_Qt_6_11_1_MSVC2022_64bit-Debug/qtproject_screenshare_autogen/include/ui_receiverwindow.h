/********************************************************************************
** Form generated from reading UI file 'receiverwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RECEIVERWINDOW_H
#define UI_RECEIVERWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Receiverwindow
{
public:
    QWidget *sender;
    QHBoxLayout *horizontalLayout_2;
    QWidget *LeftPanel;
    QWidget *leftPanel;
    QTextEdit *editLocalAnswer;
    QTextEdit *editRemoteOffer;
    QLabel *labelStatus;
    QWidget *rightPanel;
    QLabel *labelOfferTitle;
    QLabel *labelAnswerTitle;
    QPushButton *btnSetOffer;
    QWidget *RightPanel;
    QLabel *labelScreenTitle;
    QLabel *labelScreen;

    void setupUi(QWidget *Receiverwindow)
    {
        if (Receiverwindow->objectName().isEmpty())
            Receiverwindow->setObjectName("Receiverwindow");
        Receiverwindow->resize(971, 459);
        Receiverwindow->setStyleSheet(QString::fromUtf8("QWidget {\n"
"    background-color: white;\n"
"    font-family: \"Microsoft YaHei\";\n"
"    color: #1f2329;\n"
"}\n"
"\n"
"QLabel#labelStatus {\n"
"    font-size: 15px;\n"
"    color: #333333;\n"
"    font-weight: bold;\n"
"}\n"
"\n"
"QLabel#labelScreenTitle {\n"
"    font-size: 20px;\n"
"    font-weight: bold;\n"
"}\n"
"\n"
"QTextEdit {\n"
"    background-color: white;\n"
"    border: 1px solid #dcdfe6;\n"
"    border-radius: 6px;\n"
"    padding: 6px;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"QPushButton#btnSetOffer {\n"
"    background-color: #1677ff;\n"
"    color: white;\n"
"    border: none;\n"
"    border-radius: 8px;\n"
"    font-size: 15px;\n"
"    font-weight: bold;\n"
"    min-height: 38px;\n"
"}\n"
"\n"
"QPushButton#btnSetOffer:hover {\n"
"    background-color: #4096ff;\n"
"}\n"
"\n"
"QLabel#labelScreen {\n"
"    background-color: #f2f3f5;\n"
"    border: 2px solid #dcdfe6;\n"
"    border-radius: 12px;\n"
"    color: #8a94a6;\n"
"    font-size: 18px;\n"
"}"));
        sender = new QWidget(Receiverwindow);
        sender->setObjectName("sender");
        sender->setGeometry(QRect(0, 0, 962, 451));
        horizontalLayout_2 = new QHBoxLayout(sender);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        LeftPanel = new QWidget(sender);
        LeftPanel->setObjectName("LeftPanel");
        leftPanel = new QWidget(LeftPanel);
        leftPanel->setObjectName("leftPanel");
        leftPanel->setGeometry(QRect(10, 0, 341, 451));
        editLocalAnswer = new QTextEdit(leftPanel);
        editLocalAnswer->setObjectName("editLocalAnswer");
        editLocalAnswer->setGeometry(QRect(30, 250, 241, 81));
        editLocalAnswer->setMinimumSize(QSize(120, 0));
        editRemoteOffer = new QTextEdit(leftPanel);
        editRemoteOffer->setObjectName("editRemoteOffer");
        editRemoteOffer->setGeometry(QRect(30, 120, 241, 81));
        editRemoteOffer->setMinimumSize(QSize(120, 0));
        labelStatus = new QLabel(leftPanel);
        labelStatus->setObjectName("labelStatus");
        labelStatus->setGeometry(QRect(90, 20, 141, 41));
        rightPanel = new QWidget(leftPanel);
        rightPanel->setObjectName("rightPanel");
        rightPanel->setGeometry(QRect(390, -1, 461, 431));
        labelOfferTitle = new QLabel(leftPanel);
        labelOfferTitle->setObjectName("labelOfferTitle");
        labelOfferTitle->setGeometry(QRect(100, 80, 131, 21));
        labelAnswerTitle = new QLabel(leftPanel);
        labelAnswerTitle->setObjectName("labelAnswerTitle");
        labelAnswerTitle->setGeometry(QRect(100, 210, 131, 31));
        btnSetOffer = new QPushButton(leftPanel);
        btnSetOffer->setObjectName("btnSetOffer");
        btnSetOffer->setGeometry(QRect(80, 350, 141, 51));
        btnSetOffer->setStyleSheet(QString::fromUtf8("QWidget#LeftPanel {\n"
"    background-color: #f7f8fa;\n"
"    border-right: 1px solid #e5e6eb;\n"
"}\n"
"\n"
"QWidget#RightPanel {\n"
"    background-color: white;\n"
"}"));

        horizontalLayout_2->addWidget(LeftPanel);

        RightPanel = new QWidget(sender);
        RightPanel->setObjectName("RightPanel");
        labelScreenTitle = new QLabel(RightPanel);
        labelScreenTitle->setObjectName("labelScreenTitle");
        labelScreenTitle->setGeometry(QRect(160, 20, 131, 21));
        labelScreen = new QLabel(RightPanel);
        labelScreen->setObjectName("labelScreen");
        labelScreen->setGeometry(QRect(9, 50, 441, 360));
        labelScreen->setAlignment(Qt::AlignmentFlag::AlignCenter);

        horizontalLayout_2->addWidget(RightPanel);


        retranslateUi(Receiverwindow);

        QMetaObject::connectSlotsByName(Receiverwindow);
    } // setupUi

    void retranslateUi(QWidget *Receiverwindow)
    {
        Receiverwindow->setWindowTitle(QCoreApplication::translate("Receiverwindow", "Form", nullptr));
        labelStatus->setText(QCoreApplication::translate("Receiverwindow", "\347\212\266\346\200\201\357\274\232\347\255\211\345\276\205\345\217\221\351\200\201\347\253\257offer", nullptr));
        labelOfferTitle->setText(QCoreApplication::translate("Receiverwindow", "\345\217\221\351\200\201\347\253\257Offer SDP", nullptr));
        labelAnswerTitle->setText(QCoreApplication::translate("Receiverwindow", "\346\234\254\345\234\260 Answer SDP", nullptr));
        btnSetOffer->setText(QCoreApplication::translate("Receiverwindow", "\350\256\276\347\275\256\345\217\221\351\200\201\347\253\257Offer", nullptr));
        labelScreenTitle->setText(QCoreApplication::translate("Receiverwindow", "\350\277\234\347\253\257\345\205\261\344\272\253\347\224\273\351\235\242", nullptr));
        labelScreen->setText(QCoreApplication::translate("Receiverwindow", "\347\255\211\345\276\205\346\216\245\346\224\266\345\205\261\344\272\253", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Receiverwindow: public Ui_Receiverwindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RECEIVERWINDOW_H
