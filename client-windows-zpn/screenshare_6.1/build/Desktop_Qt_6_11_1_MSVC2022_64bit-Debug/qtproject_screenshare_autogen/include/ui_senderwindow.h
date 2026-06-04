/********************************************************************************
** Form generated from reading UI file 'senderwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SENDERWINDOW_H
#define UI_SENDERWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SenderWindow
{
public:
    QHBoxLayout *horizontalLayout;
    QWidget *sender;
    QHBoxLayout *horizontalLayout_2;
    QWidget *LeftPanel;
    QWidget *leftPanel;
    QTextEdit *editRemoteAnswer;
    QTextEdit *editLocalOffer;
    QPushButton *btnStopShare;
    QPushButton *btnStartShare;
    QLabel *labelStatus;
    QPushButton *btnSetAnswer;
    QWidget *rightPanel;
    QLabel *labelOfferTitle;
    QLabel *labelAnswerTitle;
    QWidget *RightPanel;
    QLabel *labelPreviewTitle;
    QLabel *labelPreview;

    void setupUi(QWidget *SenderWindow)
    {
        if (SenderWindow->objectName().isEmpty())
            SenderWindow->setObjectName("SenderWindow");
        SenderWindow->resize(984, 473);
        SenderWindow->setStyleSheet(QString::fromUtf8("QWidget {\n"
"    background-color: white;\n"
"    font-family: \"Microsoft YaHei\";\n"
"    color: #1f2329;\n"
"}\n"
"\n"
"QWidget#leftPanel {\n"
"    background-color: #f7f8fa;\n"
"    border-right: 1px solid #e5e6eb;\n"
"}\n"
"\n"
"QLabel#labelStatus {\n"
"    font-size: 15px;\n"
"    color: #333333;\n"
"    font-weight: bold;\n"
"}\n"
"\n"
"QPushButton {\n"
"    border: none;\n"
"    border-radius: 8px;\n"
"    font-size: 15px;\n"
"    font-weight: bold;\n"
"    min-height: 38px;\n"
"}\n"
"\n"
"QPushButton#btnStartShare {\n"
"    background-color: #1677ff;\n"
"    color: white;\n"
"}\n"
"\n"
"QPushButton#btnStopShare {\n"
"    background-color: #ff4d4f;\n"
"    color: white;\n"
"}\n"
"\n"
"QPushButton#btnSetAnswer {\n"
"    background-color: #52c41a;\n"
"    color: white;\n"
"}\n"
"\n"
"QTextEdit {\n"
"    background-color: white;\n"
"    border: 1px solid #dcdfe6;\n"
"    border-radius: 6px;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"QLabel#labelPreviewTitle {\n"
"    font-size: 20px;\n"
"    font-weight: bo"
                        "ld;\n"
"}\n"
"\n"
"QLabel#labelPreview {\n"
"    background-color: #f2f3f5;\n"
"    border: 2px solid #dcdfe6;\n"
"    border-radius: 12px;\n"
"    color: #8a94a6;\n"
"    font-size: 18px;\n"
"}"));
        horizontalLayout = new QHBoxLayout(SenderWindow);
        horizontalLayout->setObjectName("horizontalLayout");
        sender = new QWidget(SenderWindow);
        sender->setObjectName("sender");
        horizontalLayout_2 = new QHBoxLayout(sender);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        LeftPanel = new QWidget(sender);
        LeftPanel->setObjectName("LeftPanel");
        leftPanel = new QWidget(LeftPanel);
        leftPanel->setObjectName("leftPanel");
        leftPanel->setGeometry(QRect(10, 0, 341, 451));
        editRemoteAnswer = new QTextEdit(leftPanel);
        editRemoteAnswer->setObjectName("editRemoteAnswer");
        editRemoteAnswer->setGeometry(QRect(200, 220, 120, 31));
        editRemoteAnswer->setMinimumSize(QSize(120, 0));
        editLocalOffer = new QTextEdit(leftPanel);
        editLocalOffer->setObjectName("editLocalOffer");
        editLocalOffer->setGeometry(QRect(200, 120, 120, 31));
        editLocalOffer->setMinimumSize(QSize(120, 0));
        btnStopShare = new QPushButton(leftPanel);
        btnStopShare->setObjectName("btnStopShare");
        btnStopShare->setGeometry(QRect(210, 270, 93, 38));
        btnStartShare = new QPushButton(leftPanel);
        btnStartShare->setObjectName("btnStartShare");
        btnStartShare->setGeometry(QRect(30, 270, 93, 38));
        labelStatus = new QLabel(leftPanel);
        labelStatus->setObjectName("labelStatus");
        labelStatus->setGeometry(QRect(110, 20, 141, 41));
        btnSetAnswer = new QPushButton(leftPanel);
        btnSetAnswer->setObjectName("btnSetAnswer");
        btnSetAnswer->setGeometry(QRect(100, 340, 141, 51));
        rightPanel = new QWidget(leftPanel);
        rightPanel->setObjectName("rightPanel");
        rightPanel->setGeometry(QRect(390, -1, 461, 431));
        labelOfferTitle = new QLabel(leftPanel);
        labelOfferTitle->setObjectName("labelOfferTitle");
        labelOfferTitle->setGeometry(QRect(30, 120, 91, 21));
        labelAnswerTitle = new QLabel(leftPanel);
        labelAnswerTitle->setObjectName("labelAnswerTitle");
        labelAnswerTitle->setGeometry(QRect(20, 220, 131, 31));

        horizontalLayout_2->addWidget(LeftPanel);

        RightPanel = new QWidget(sender);
        RightPanel->setObjectName("RightPanel");
        labelPreviewTitle = new QLabel(RightPanel);
        labelPreviewTitle->setObjectName("labelPreviewTitle");
        labelPreviewTitle->setGeometry(QRect(160, 20, 131, 21));
        labelPreview = new QLabel(RightPanel);
        labelPreview->setObjectName("labelPreview");
        labelPreview->setGeometry(QRect(0, 50, 450, 360));
        labelPreview->setAlignment(Qt::AlignmentFlag::AlignCenter);

        horizontalLayout_2->addWidget(RightPanel);


        horizontalLayout->addWidget(sender);


        retranslateUi(SenderWindow);

        QMetaObject::connectSlotsByName(SenderWindow);
    } // setupUi

    void retranslateUi(QWidget *SenderWindow)
    {
        SenderWindow->setWindowTitle(QCoreApplication::translate("SenderWindow", "Form", nullptr));
        btnStopShare->setText(QCoreApplication::translate("SenderWindow", "\345\201\234\346\255\242\345\205\261\344\272\253", nullptr));
        btnStartShare->setText(QCoreApplication::translate("SenderWindow", "\345\274\200\345\247\213\345\205\261\344\272\253", nullptr));
        labelStatus->setText(QCoreApplication::translate("SenderWindow", "\347\212\266\346\200\201\357\274\232\346\234\252\345\274\200\345\247\213\345\205\261\344\272\253", nullptr));
        btnSetAnswer->setText(QCoreApplication::translate("SenderWindow", "\350\256\276\347\275\256\346\216\245\346\224\266\347\253\257Answer", nullptr));
        labelOfferTitle->setText(QCoreApplication::translate("SenderWindow", "\346\234\254\345\234\260Offer SDP", nullptr));
        labelAnswerTitle->setText(QCoreApplication::translate("SenderWindow", "\346\216\245\346\224\266\347\253\257Answer SDP", nullptr));
        labelPreviewTitle->setText(QCoreApplication::translate("SenderWindow", "\346\234\254\345\234\260\345\261\217\345\271\225\351\242\204\350\247\210", nullptr));
        labelPreview->setText(QCoreApplication::translate("SenderWindow", "\347\255\211\345\276\205\345\274\200\345\247\213\345\205\261\344\272\253", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SenderWindow: public Ui_SenderWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SENDERWINDOW_H
