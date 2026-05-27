#include "RoomPage.h"
#include "widgets/ScreenView.h"
#include "widgets/MemberList.h"
#include "network/RoomClient.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMessageBox>

RoomPage::RoomPage(QWidget *parent)
    : QWidget(parent)
    , m_screenView(nullptr)
    , m_memberList(nullptr)
    , m_shareBtn(nullptr)
    , m_grabBtn(nullptr)
    , m_micBtn(nullptr)
    , m_leaveBtn(nullptr)
{
    setupUI();
}

void RoomPage::setRoomInfo(const RoomPageInfo &info)
{
    m_info = info;
    m_roomLabel->setText(QString("房间: %1").arg(info.roomId));

    QList<MemberInfo> members;
    members << MemberInfo{info.nickname, false, true};
    m_memberList->setMyName(info.nickname);
    m_memberList->setMembers(members);
    updateMemberCount();
}

void RoomPage::resetRoom()
{
    m_isSharing = false;
    m_micOn = false;
    m_currentSharer.clear();
    m_shareBtn->setText("共享屏幕");
    m_shareBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #2d5aa0;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #3670b8; }
        QPushButton:pressed { background-color: #244a82; }
    )");
    m_micBtn->setChecked(false);
    m_screenView->stopSimulatedView();
    m_memberList->clearMembers();
    m_memberCountLabel->setText("在线: 0 人");
    m_roomLabel->setText("房间: --");
}

void RoomPage::onMemberListReceived(const QString &roomId, const QList<MemberEntry> &members)
{
    Q_UNUSED(roomId);

    QList<MemberInfo> memberInfos;
    m_currentSharer.clear();
    for (const MemberEntry &entry : members) {
        bool isMe = (entry.name == m_info.nickname);
        memberInfos << MemberInfo{entry.name, entry.isSharing, isMe};
        if (entry.isSharing) {
            m_currentSharer = entry.name;
        }
    }
    m_memberList->setMyName(m_info.nickname);
    m_memberList->setMembers(memberInfos);
    updateMemberCount();
}

void RoomPage::onMemberJoined(const QString &roomId, const QString &name, bool isSharing)
{
    Q_UNUSED(roomId);
    m_memberList->addMember(MemberInfo{name, isSharing, false});
    if (isSharing) m_currentSharer = name;
    updateMemberCount();
}

void RoomPage::onMemberLeft(const QString &roomId, const QString &name)
{
    Q_UNUSED(roomId);
    m_memberList->removeMember(name);
    if (m_currentSharer == name) {
        m_currentSharer.clear();
    }
    updateMemberCount();
}

void RoomPage::onShareStarted(const QString &roomId, const QString &name)
{
    Q_UNUSED(roomId);
    m_currentSharer = name;
    m_memberList->updateMemberSharing(name, true);

    if (name == m_info.nickname) {
        // 自己是共享者
        m_isSharing = true;
        setSharingUI(true);
        m_grabBtn->setDisabled(true);
    } else {
        // 自己是观看者，显示模拟画面
        m_screenView->startSimulatedView(name);
        m_grabBtn->setEnabled(true);
    }
}

void RoomPage::onShareStopped(const QString &roomId, const QString &name)
{
    Q_UNUSED(roomId);
    m_memberList->updateMemberSharing(name, false);
    m_currentSharer.clear();

    if (name == m_info.nickname) {
        // 自己停止共享
        m_isSharing = false;
        setSharingUI(false);
        m_grabBtn->setDisabled(false);
    } else {
        // 观看者：停止模拟画面
        m_screenView->stopSimulatedView();
    }
}

void RoomPage::onShareRejected(const QString &reason)
{
    QMessageBox::information(this, "无法共享", reason);
}

void RoomPage::onGrabRequested(const QString &fromName)
{
    int ret = QMessageBox::question(this, "抢共享请求",
        QString("%1 请求获取共享权限，是否同意？").arg(fromName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    emit grabShareResponded(ret == QMessageBox::Yes);
}

void RoomPage::onGrabResult(bool granted, const QString &fromName)
{
    if (granted) {
        // 后续会收到 share_started 广播自动切换状态
    } else {
        QMessageBox::information(this, "抢共享",
            QString("%1 拒绝了您的抢共享请求").arg(fromName));
    }
}

void RoomPage::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // === 顶部信息栏 ===
    QWidget *topBar = new QWidget(this);
    topBar->setFixedHeight(40);
    topBar->setStyleSheet("background-color: #252525; border-bottom: 1px solid #3a3a3a;");

    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(16, 0, 16, 0);

    m_roomLabel = new QLabel("房间: --", topBar);
    m_roomLabel->setStyleSheet("color: #cccccc; font-size: 13px; background: transparent;");

    m_memberCountLabel = new QLabel("在线: 0 人", topBar);
    m_memberCountLabel->setStyleSheet("color: #888888; font-size: 12px; background: transparent;");

    topLayout->addWidget(m_roomLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_memberCountLabel);

    mainLayout->addWidget(topBar);

    // === 中间区域 ===
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setStyleSheet("QSplitter::handle { background: #3a3a3a; width: 1px; }");

    m_screenView = new ScreenView(splitter);
    m_screenView->setPlaceholderText("等待屏幕共享...");

    QWidget *rightPanel = new QWidget(splitter);
    rightPanel->setStyleSheet("background-color: #2b2b2b;");

    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    QLabel *memberTitle = new QLabel("成员列表", rightPanel);
    memberTitle->setAlignment(Qt::AlignCenter);
    memberTitle->setFixedHeight(36);
    memberTitle->setStyleSheet("color: #aaaaaa; font-size: 12px; border-bottom: 1px solid #3a3a3a; background: transparent;");

    m_memberList = new MemberList(rightPanel);

    rightLayout->addWidget(memberTitle);
    rightLayout->addWidget(m_memberList);

    splitter->addWidget(m_screenView);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    mainLayout->addWidget(splitter, 1);

    // === 底部工具栏 ===
    QWidget *toolbar = new QWidget(this);
    toolbar->setFixedHeight(72);
    toolbar->setStyleSheet("background-color: #252525; border-top: 1px solid #3a3a3a;");

    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setAlignment(Qt::AlignCenter);
    toolbarLayout->setSpacing(12);

    // 共享屏幕按钮
    m_shareBtn = new QPushButton("共享屏幕", toolbar);
    m_shareBtn->setFixedSize(100, 42);
    m_shareBtn->setCursor(Qt::PointingHandCursor);
    m_shareBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #2d5aa0;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #3670b8; }
        QPushButton:pressed { background-color: #244a82; }
    )");
    connect(m_shareBtn, &QPushButton::clicked, this, &RoomPage::onShareClicked);

    // 抢共享按钮
    m_grabBtn = new QPushButton("抢共享", toolbar);
    m_grabBtn->setFixedSize(80, 42);
    m_grabBtn->setCursor(Qt::PointingHandCursor);
    m_grabBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #cccccc;
            border: none;
            border-radius: 6px;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #4a4a4a; }
        QPushButton:pressed { background-color: #555555; }
    )");
    connect(m_grabBtn, &QPushButton::clicked, this, &RoomPage::onGrabClicked);

    // 麦克风按钮
    m_micBtn = new QPushButton("🎤 麦克风", toolbar);
    m_micBtn->setFixedSize(90, 42);
    m_micBtn->setCursor(Qt::PointingHandCursor);
    m_micBtn->setCheckable(true);
    m_micBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #cccccc;
            border: none;
            border-radius: 6px;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #4a4a4a; }
        QPushButton:pressed { background-color: #555555; }
        QPushButton:checked {
            background-color: #2d5aa0;
            color: white;
        }
    )");
    connect(m_micBtn, &QPushButton::clicked, this, &RoomPage::onMicClicked);

    // 退出房间按钮
    m_leaveBtn = new QPushButton("退出房间", toolbar);
    m_leaveBtn->setFixedSize(90, 42);
    m_leaveBtn->setCursor(Qt::PointingHandCursor);
    m_leaveBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #c0392b;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #e74c3c; }
        QPushButton:pressed { background-color: #a93226; }
    )");
    connect(m_leaveBtn, &QPushButton::clicked, this, &RoomPage::leaveRoomRequested);

    toolbarLayout->addWidget(m_shareBtn);
    toolbarLayout->addWidget(m_grabBtn);
    toolbarLayout->addWidget(m_micBtn);
    toolbarLayout->addWidget(m_leaveBtn);

    mainLayout->addWidget(toolbar);
}

void RoomPage::onShareClicked()
{
    if (!m_isSharing) {
        emit shareScreenRequested();
    } else {
        emit stopShareRequested();
    }
}

void RoomPage::onGrabClicked()
{
    emit grabShareRequested();
}

void RoomPage::onMicClicked()
{
    m_micOn = m_micBtn->isChecked();
    emit micToggleRequested(m_micOn);
}

void RoomPage::updateMemberCount()
{
    int count = m_memberList->count();
    m_memberCountLabel->setText(QString("在线: %1 人").arg(count));
}

void RoomPage::setSharingUI(bool sharing)
{
    if (sharing) {
        m_shareBtn->setText("停止共享");
        m_shareBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #c0392b;
                color: white;
                border: none;
                border-radius: 6px;
                font-size: 13px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #e74c3c; }
            QPushButton:pressed { background-color: #a93226; }
        )");
        m_screenView->setPlaceholderText("正在共享屏幕...");
    } else {
        m_shareBtn->setText("共享屏幕");
        m_shareBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #2d5aa0;
                color: white;
                border: none;
                border-radius: 6px;
                font-size: 13px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #3670b8; }
            QPushButton:pressed { background-color: #244a82; }
        )");
        m_screenView->setPlaceholderText("等待屏幕共享...");
    }
}
