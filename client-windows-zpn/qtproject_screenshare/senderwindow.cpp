#include "senderwindow.h"
#include "ui_senderwindow.h"

#include <QMessageBox>
#include <QDateTime>

SenderWindow::SenderWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SenderWindow)
{
    ui->setupUi(this);

    ui->labelStatus->setText("状态：未开始共享");
    ui->labelPreview->setText("等待开始共享");

    connect(ui->btnStartShare, &QPushButton::clicked, this, [=]() {
        ui->labelStatus->setText("状态：正在共享屏幕，已生成 Offer");
        ui->labelPreview->setText("本地屏幕预览区域\n后续这里显示待共享画面");

        QString fakeOffer =
            "===== FAKE OFFER SDP =====\n"
            "type: offer\n"
            "sender: screen-share-client\n"
            "time: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + "\n"
                                                                             "这里后续替换成 WebRTC 生成的真实 Offer SDP\n";

        ui->editLocalOffer->setPlainText(fakeOffer);
    });

    connect(ui->btnStopShare, &QPushButton::clicked, this, [=]() {
        ui->labelStatus->setText("状态：已停止共享");
        ui->labelPreview->setText("等待开始共享");
    });

    connect(ui->btnSetAnswer, &QPushButton::clicked, this, [=]() {
        QString answer = ui->editRemoteAnswer->toPlainText();

        if (answer.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先粘贴接收端 Answer SDP");
            return;
        }

        ui->labelStatus->setText("状态：已设置接收端 Answer，等待连接建立");

        QMessageBox::information(this, "成功", "接收端 Answer 已设置");
    });
}

SenderWindow::~SenderWindow()
{
    delete ui;
}