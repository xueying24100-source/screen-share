#include "LoginPage.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

LoginPage::LoginPage(QWidget *parent)
    : QWidget(parent)
    , m_nicknameEdit(nullptr)
    , m_roomIdEdit(nullptr)
    , m_serverEdit(nullptr)
    , m_portEdit(nullptr)
    , m_serverStatus(nullptr)
{
    setupUI();
}

void LoginPage::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);

    QWidget *card = new QWidget(this);
    card->setFixedWidth(420);
    card->setObjectName("loginCard");
    card->setStyleSheet(R"(
        #loginCard {
            background-color: #2b2b2b;
            border-radius: 12px;
        }
    )");

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(40, 36, 40, 36);
    cardLayout->setSpacing(16);

    // 标题
    QLabel *titleLabel = new QLabel("屏幕共享", card);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #ffffff; background: transparent;");

    QLabel *subtitleLabel = new QLabel("局域网屏幕共享软件", card);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet("font-size: 13px; color: #888888; background: transparent;");

    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(subtitleLabel);
    cardLayout->addSpacing(8);

    // 昵称
    QLabel *nickLabel = new QLabel("昵称", card);
    nickLabel->setStyleSheet("font-size: 12px; color: #aaaaaa; background: transparent;");

    m_nicknameEdit = new QLineEdit(card);
    m_nicknameEdit->setPlaceholderText("请输入昵称");
    m_nicknameEdit->setFixedHeight(40);
    m_nicknameEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #3a3a3a;
            border: 1px solid #4a4a4a;
            border-radius: 6px;
            padding: 0 12px;
            color: #ffffff;
            font-size: 13px;
        }
        QLineEdit:focus { border-color: #2d5aa0; }
    )");

    cardLayout->addWidget(nickLabel);
    cardLayout->addWidget(m_nicknameEdit);

    // 房间号
    QLabel *roomLabel = new QLabel("房间号", card);
    roomLabel->setStyleSheet("font-size: 12px; color: #aaaaaa; background: transparent;");

    m_roomIdEdit = new QLineEdit(card);
    m_roomIdEdit->setPlaceholderText("请输入房间号");
    m_roomIdEdit->setFixedHeight(40);
    m_roomIdEdit->setStyleSheet(m_nicknameEdit->styleSheet());

    cardLayout->addWidget(roomLabel);
    cardLayout->addWidget(m_roomIdEdit);

    // 服务器地址
    QLabel *serverLabel = new QLabel("服务器地址", card);
    serverLabel->setStyleSheet("font-size: 12px; color: #aaaaaa; background: transparent;");

    QWidget *serverRow = new QWidget(card);
    serverRow->setStyleSheet("background: transparent;");
    QHBoxLayout *serverLayout = new QHBoxLayout(serverRow);
    serverLayout->setContentsMargins(0, 0, 0, 0);
    serverLayout->setSpacing(8);

    m_serverEdit = new QLineEdit(serverRow);
    m_serverEdit->setPlaceholderText("服务器IP");
    m_serverEdit->setText("127.0.0.1");
    m_serverEdit->setDisabled(true);
    m_serverEdit->setFixedHeight(40);
    m_serverEdit->setStyleSheet(m_nicknameEdit->styleSheet());

    m_portEdit = new QLineEdit(serverRow);
    m_portEdit->setPlaceholderText("端口");
    m_portEdit->setText("9527");
    m_portEdit->setFixedHeight(40);
    m_portEdit->setFixedWidth(80);
    m_portEdit->setStyleSheet(m_nicknameEdit->styleSheet());

    serverLayout->addWidget(m_serverEdit, 1);
    serverLayout->addWidget(m_portEdit);

    cardLayout->addWidget(serverLabel);
    cardLayout->addWidget(serverRow);

    // 状态提示
    m_serverStatus = new QLabel("", card);
    m_serverStatus->setAlignment(Qt::AlignCenter);
    m_serverStatus->setStyleSheet("font-size: 11px; color: #888888; background: transparent;");

    cardLayout->addWidget(m_serverStatus);
    cardLayout->addSpacing(6);

    // 加入按钮
    QPushButton *joinBtn = new QPushButton("加入房间", card);
    joinBtn->setFixedHeight(42);
    joinBtn->setCursor(Qt::PointingHandCursor);
    joinBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #2d5aa0;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #3670b8; }
        QPushButton:pressed { background-color: #244a82; }
    )");
    connect(joinBtn, &QPushButton::clicked, this, &LoginPage::onJoinClicked);

    cardLayout->addWidget(joinBtn);

    mainLayout->addWidget(card);

    connect(m_nicknameEdit, &QLineEdit::returnPressed, this, &LoginPage::onJoinClicked);
    connect(m_roomIdEdit, &QLineEdit::returnPressed, this, &LoginPage::onJoinClicked);
}

void LoginPage::onJoinClicked()
{
    QString nickname = m_nicknameEdit->text().trimmed();
    QString roomId = m_roomIdEdit->text().trimmed();
    QString host = m_serverEdit->text().trimmed();
    quint16 port = m_portEdit->text().toUShort();
    if (port == 0) port = 9527;

    if (nickname.isEmpty()) { m_nicknameEdit->setFocus(); setServerStatus("昵称不能为空", false); return; }
    if (roomId.isEmpty()) { m_roomIdEdit->setFocus(); setServerStatus("房间号不能为空", false); return; }
    if (host.isEmpty()) { m_serverEdit->setFocus(); setServerStatus("端口号不能为空", false); return; }

    emit joinRoomRequested(nickname, roomId, host, port);
}

void LoginPage::setServerStatus(const QString &text, bool success)
{
    m_serverStatus->setText(text);
    m_serverStatus->setStyleSheet(
        success
            ? "font-size: 11px; color: #4caf50; background: transparent;"
            : "font-size: 11px; color: #e74c3c; background: transparent;");
}
