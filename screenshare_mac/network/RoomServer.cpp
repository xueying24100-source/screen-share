#include "RoomServer.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

RoomServer::RoomServer(QObject *parent)
    : QObject(parent)
    , m_server(nullptr)
{
}

RoomServer::~RoomServer()
{
    stop();
}

bool RoomServer::start(quint16 port)
{
    if (m_server) return true;

    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::Any, port)) {
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QTcpServer::newConnection,
            this, &RoomServer::onNewConnection);

    return true;
}

void RoomServer::stop()
{
    if (!m_server) return;

    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        it.key()->disconnectFromHost();
    }
    m_clients.clear();
    m_rooms.clear();
    m_pendingGrabs.clear();

    m_server->close();
    delete m_server;
    m_server = nullptr;
}

bool RoomServer::isRunning() const
{
    return m_server && m_server->isListening();
}

quint16 RoomServer::port() const
{
    return m_server ? m_server->serverPort() : 0;
}

void RoomServer::onNewConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &RoomServer::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &RoomServer::onDisconnected);

    m_clients[socket] = ClientInfo{socket, "", "", false};
}

void RoomServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    while (socket->canReadLine()) {
        QByteArray line = socket->readLine().trimmed();
        if (line.isEmpty()) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) continue;
        if (!doc.isObject()) continue;

        handleMessage(socket, doc.object());
    }
}

void RoomServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    handleLeave(socket);

    // 清理与该 socket 相关的 pending grab
    m_pendingGrabs.remove(socket);
    for (auto it = m_pendingGrabs.begin(); it != m_pendingGrabs.end(); ) {
        if (it.value() == socket) {
            it = m_pendingGrabs.erase(it);
        } else {
            ++it;
        }
    }

    m_clients.remove(socket);
    socket->deleteLater();
}

void RoomServer::handleMessage(QTcpSocket *socket, const QJsonObject &msg)
{
    QString type = msg["type"].toString();

    if (type == "join") {
        handleJoin(socket, msg);
    } else if (type == "leave") {
        handleLeave(socket);
    } else if (type == "share_start") {
        handleShareStart(socket);
    } else if (type == "share_stop") {
        handleShareStop(socket);
    } else if (type == "grab_share") {
        handleGrabShare(socket);
    } else if (type == "grab_respond") {
        handleGrabRespond(socket, msg);
    } else if (type == "video_frame") {
        handleVideoFrame(socket, msg);
    } else if (type == "annotation") {
        handleVideoFrame(socket, msg);
    }
}

void RoomServer::handleJoin(QTcpSocket *socket, const QJsonObject &msg)
{
    QString roomId = msg["roomId"].toString();
    QString nickname = msg["nickname"].toString();

    if (roomId.isEmpty() || nickname.isEmpty()) return;

    handleLeave(socket);

    m_clients[socket].nickname = nickname;
    m_clients[socket].roomId = roomId;
    m_rooms[roomId].insert(socket);

    QJsonObject listMsg;
    listMsg["type"] = "member_list";
    listMsg["roomId"] = roomId;
    listMsg["members"] = buildMemberList(roomId);
    sendToClient(socket, listMsg);

    QJsonObject joinMsg;
    joinMsg["type"] = "member_joined";
    joinMsg["roomId"] = roomId;
    joinMsg["name"] = nickname;
    joinMsg["isSharing"] = false;
    broadcastToRoom(roomId, joinMsg, socket);
}

void RoomServer::handleLeave(QTcpSocket *socket)
{
    if (!m_clients.contains(socket)) return;

    ClientInfo info = m_clients[socket];
    if (info.roomId.isEmpty()) return;

    QString roomId = info.roomId;
    QString nickname = info.nickname;

    // 如果正在共享，先广播停止共享
    if (info.isSharing) {
        QJsonObject stopMsg;
        stopMsg["type"] = "share_stopped";
        stopMsg["roomId"] = roomId;
        stopMsg["name"] = nickname;
        broadcastToRoom(roomId, stopMsg);
    }

    m_rooms[roomId].remove(socket);
    m_clients[socket].roomId = "";
    m_clients[socket].nickname = "";
    m_clients[socket].isSharing = false;

    if (m_rooms[roomId].isEmpty()) {
        m_rooms.remove(roomId);
    }

    QJsonObject leaveMsg;
    leaveMsg["type"] = "member_left";
    leaveMsg["roomId"] = roomId;
    leaveMsg["name"] = nickname;
    broadcastToRoom(roomId, leaveMsg);
}

void RoomServer::handleShareStart(QTcpSocket *socket)
{
    if (!m_clients.contains(socket)) return;
    QString roomId = m_clients[socket].roomId;
    if (roomId.isEmpty()) return;

    // 检查房间内是否已有共享者
    QTcpSocket *existingSharer = findSharerInRoom(roomId);
    if (existingSharer) {
        QJsonObject rejectMsg;
        rejectMsg["type"] = "share_rejected";
        rejectMsg["reason"] = "已有其他人正在共享屏幕，请点击抢共享";
        sendToClient(socket, rejectMsg);
        return;
    }

    m_clients[socket].isSharing = true;

    QJsonObject msg;
    msg["type"] = "share_started";
    msg["roomId"] = roomId;
    msg["name"] = m_clients[socket].nickname;
    broadcastToRoom(roomId, msg);
}

void RoomServer::handleShareStop(QTcpSocket *socket)
{
    if (!m_clients.contains(socket)) return;
    if (!m_clients[socket].isSharing) return;

    QString roomId = m_clients[socket].roomId;
    QString nickname = m_clients[socket].nickname;
    m_clients[socket].isSharing = false;

    QJsonObject msg;
    msg["type"] = "share_stopped";
    msg["roomId"] = roomId;
    msg["name"] = nickname;
    broadcastToRoom(roomId, msg);
}

void RoomServer::handleGrabShare(QTcpSocket *socket)
{
    if (!m_clients.contains(socket)) return;
    QString roomId = m_clients[socket].roomId;
    if (roomId.isEmpty()) return;

    QTcpSocket *sharer = findSharerInRoom(roomId);

    if (!sharer) {
        // 无人共享，直接允许开始
        m_clients[socket].isSharing = true;
        QJsonObject msg;
        msg["type"] = "share_started";
        msg["roomId"] = roomId;
        msg["name"] = m_clients[socket].nickname;
        broadcastToRoom(roomId, msg);
        return;
    }

    // 记录 pending grab，向当前共享者发送请求
    m_pendingGrabs[sharer] = socket;

    QJsonObject msg;
    msg["type"] = "grab_request";
    msg["from"] = m_clients[socket].nickname;
    sendToClient(sharer, msg);
}

void RoomServer::handleGrabRespond(QTcpSocket *socket, const QJsonObject &msg)
{
    bool granted = msg["granted"].toBool();

    QTcpSocket *grabber = m_pendingGrabs.take(socket);
    if (!grabber || !m_clients.contains(grabber)) return;

    if (granted) {
        // 原共享者停止共享
        QString roomId = m_clients[socket].roomId;
        QString oldName = m_clients[socket].nickname;
        m_clients[socket].isSharing = false;

        QJsonObject stopMsg;
        stopMsg["type"] = "share_stopped";
        stopMsg["roomId"] = roomId;
        stopMsg["name"] = oldName;
        broadcastToRoom(roomId, stopMsg);

        // 新共享者开始共享
        QString newName = m_clients[grabber].nickname;
        m_clients[grabber].isSharing = true;

        QJsonObject startMsg;
        startMsg["type"] = "share_started";
        startMsg["roomId"] = roomId;
        startMsg["name"] = newName;
        broadcastToRoom(roomId, startMsg);
    }

    // 通知抢夺者结果
    QJsonObject resultMsg;
    resultMsg["type"] = "grab_result";
    resultMsg["granted"] = granted;
    resultMsg["from"] = m_clients[socket].nickname;
    sendToClient(grabber, resultMsg);
}

QTcpSocket* RoomServer::findSharerInRoom(const QString &roomId) const
{
    if (!m_rooms.contains(roomId)) return nullptr;

    for (QTcpSocket *s : m_rooms[roomId]) {
        if (m_clients.contains(s) && m_clients[s].isSharing) {
            return s;
        }
    }
    return nullptr;
}

void RoomServer::sendToClient(QTcpSocket *socket, const QJsonObject &msg)
{
    if (socket->state() != QAbstractSocket::ConnectedState) return;

    QJsonDocument doc(msg);
    socket->write(doc.toJson(QJsonDocument::Compact) + "\n");
    socket->flush();
}

void RoomServer::broadcastToRoom(const QString &roomId, const QJsonObject &msg,
                                  QTcpSocket *exclude)
{
    if (!m_rooms.contains(roomId)) return;

    for (QTcpSocket *socket : m_rooms[roomId]) {
        if (socket != exclude) {
            sendToClient(socket, msg);
        }
    }
}

void RoomServer::handleVideoFrame(QTcpSocket *socket, const QJsonObject &msg)
{
    if (!m_clients.contains(socket)) return;
    QString roomId = m_clients[socket].roomId;
    if (roomId.isEmpty()) return;

    broadcastToRoom(roomId, msg, socket);
}

QJsonArray RoomServer::buildMemberList(const QString &roomId) const
{
    QJsonArray arr;
    if (!m_rooms.contains(roomId)) return arr;

    for (QTcpSocket *socket : m_rooms[roomId]) {
        if (m_clients.contains(socket)) {
            QJsonObject m;
            m["name"] = m_clients[socket].nickname;
            m["isSharing"] = m_clients[socket].isSharing;
            arr.append(m);
        }
    }
    return arr;
}
