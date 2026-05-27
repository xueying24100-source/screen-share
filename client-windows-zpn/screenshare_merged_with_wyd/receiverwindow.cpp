#include "receiverwindow.h"
#include "ui_receiverwindow.h"

#include <QMessageBox>
#include <QDateTime>

Receiverwindow::Receiverwindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Receiverwindow)
{
    ui->setupUi(this);

    ui->labelStatus->setText("状态：等待发送端 Offer");
    ui->labelScreen->setText("等待接收共享画面");

    connect(ui->btnSetOffer, &QPushButton::clicked, this, [=]() {
        QString offer = ui->editRemoteOffer->toPlainText();

        if (offer.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先粘贴发送端 Offer SDP");
            return;
        }

        ui->labelStatus->setText("状态：已收到 Offer，已生成 Answer");
        ui->labelScreen->setText("远端共享画面区域\n后续这里显示发送端屏幕");

        QString fakeAnswer =
            "===== FAKE ANSWER SDP =====\n"
            "type: answer\n"
            "receiver: screen-watch-client\n"
            "time: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + "\n"
                                                                             "这里后续替换成 WebRTC 生成的真实 Answer SDP\n";

        ui->editLocalAnswer->setPlainText(fakeAnswer);

        QMessageBox::information(this, "成功", "已生成本地 Answer SDP");
    });
}

Receiverwindow::~Receiverwindow()
{
    delete ui;
}