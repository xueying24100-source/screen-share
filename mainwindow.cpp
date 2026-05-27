#include "mainwindow.h"
#include "pages/LoginPage.h"
#include "pages/RoomPage.h"
#include "network/RoomServer.h"
#include "network/RoomClient.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_stackWidget(nullptr)
    , m_loginPage(nullptr)
    , m_roomPage(nullptr)
    , m_server(nullptr)
    , m_client(nullptr)
{
    setupUI();
}

MainWindow::~MainWindow()
{
    if (m_client) {
        m_client->disconnectFromServer();
        delete m_client;
    }
    if (m_server) {
        m_server->stop();
        delete m_server;
    }
}

void MainWindow::setupUI()
{
    setWindowTitle("屏幕共享");
    resize(1024, 680);
    setMinimumSize(800, 540);

    setStyleSheet("QMainWindow { background-color: #1e1e1e; }");

    m_stackWidget = new QStackedWidget(this);
    setCentralWidget(m_stackWidget);

    m_loginPage = new LoginPage(m_stackWidget);
    m_roomPage = new RoomPage(m_stackWidget);

    m_stackWidget->addWidget(m_loginPage);
    m_stackWidget->addWidget(m_roomPage);
    m_stackWidget->setCurrentWidget(m_loginPage);

    connect(m_loginPage, &LoginPage::joinRoomRequested,
            this, qOverload<const QString&, const QString&, const QString&, quint16>(&MainWindow::onJoinRoom));
    connect(m_roomPage, &RoomPage::leaveRoomRequested,
            this, &MainWindow::onLeaveRoom);
}

void MainWindow::onJoinRoom(const QString &nickname, const QString &roomId,
                             const QString &serverHost, quint16 serverPort)
{
    m_nickname = nickname;
    m_roomId = roomId;

    // 自动尝试在本机启动服务器：成功说明是第一个客户端，失败说明已有服务器
    if (!m_server) {
        m_server = new RoomServer(this);
    }
    if (!m_server->isRunning()) {
        m_server->start(serverPort);
    }

    // 创建客户端并连接
    if (m_client) {
        m_client->disconnectFromServer();
        delete m_client;
    }

    m_client = new RoomClient(this);

    // 成员同步
    connect(m_client, &RoomClient::memberListReceived,
            m_roomPage, &RoomPage::onMemberListReceived);
    connect(m_client, &RoomClient::memberJoined,
            m_roomPage, &RoomPage::onMemberJoined);
    connect(m_client, &RoomClient::memberLeft,
            m_roomPage, &RoomPage::onMemberLeft);

    // 共享状态
    connect(m_client, &RoomClient::shareStarted,
            m_roomPage, &RoomPage::onShareStarted);
    connect(m_client, &RoomClient::shareStopped,
            m_roomPage, &RoomPage::onShareStopped);
    connect(m_client, &RoomClient::shareRejected,
            m_roomPage, &RoomPage::onShareRejected);
    connect(m_client, &RoomClient::grabRequested,
            m_roomPage, &RoomPage::onGrabRequested);
    connect(m_client, &RoomClient::grabResult,
            m_roomPage, &RoomPage::onGrabResult);

    // RoomPage 请求 → RoomClient
    connect(m_roomPage, &RoomPage::shareScreenRequested,
            this, [this]() { m_client->requestShareStart(); });
    connect(m_roomPage, &RoomPage::stopShareRequested,
            this, [this]() { m_client->requestShareStop(); });
    connect(m_roomPage, &RoomPage::grabShareRequested,
            this, [this]() { m_client->requestGrabShare(); });
    connect(m_roomPage, &RoomPage::grabShareResponded,
            this, [this](bool granted) { m_client->respondGrab(granted); });

    // 连接错误
    connect(m_client, &RoomClient::errorOccurred, this,
            [this](const QString &err) {
                m_loginPage->setServerStatus("连接失败: " + err, false);
            });

    // 设置房间信息并切换页面
    RoomPageInfo info;
    info.nickname = nickname;
    info.roomId = roomId;
    m_roomPage->setRoomInfo(info);

    m_stackWidget->setCurrentWidget(m_roomPage);

    // 连接服务器，成功后自动加入房间
    m_client->connectToServer(serverHost, serverPort);
    connect(m_client, &RoomClient::connected, this, [this]() {
        m_client->joinRoom(m_roomId, m_nickname);
    }, Qt::QueuedConnection);
}

void MainWindow::onLeaveRoom()
{
    if (m_client) {
        m_client->leaveRoom();
        m_client->disconnectFromServer();
        delete m_client;
        m_client = nullptr;
    }

    m_roomPage->resetRoom();
    m_stackWidget->setCurrentWidget(m_loginPage);
    m_nickname.clear();
    m_roomId.clear();
}
