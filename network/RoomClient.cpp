#include "RoomClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

RoomClient::RoomClient(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
{
}

RoomClient::~RoomClient()
{
    disconnectFromServer();
}

bool RoomClient::connectToServer(const QString &host, quint16 port)
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        return true;
    }

    m_host = host;
    m_port = port;

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &RoomClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &RoomClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &RoomClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &RoomClient::onSocketError);

    m_socket->connectToHost(host, port);
    return true;
}

void RoomClient::disconnectFromServer()
{
    if (m_socket) {
        if (!m_roomId.isEmpty()) {
            leaveRoom();
        }
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

bool RoomClient::isConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void RoomClient::joinRoom(const QString &roomId, const QString &nickname)
{
    m_roomId = roomId;
    m_nickname = nickname;

    QJsonObject msg;
    msg["type"] = "join";
    msg["roomId"] = roomId;
    msg["nickname"] = nickname;
    sendMessage(msg);
}

void RoomClient::leaveRoom()
{
    QJsonObject msg;
    msg["type"] = "leave";
    sendMessage(msg);

    m_roomId.clear();
    m_nickname.clear();
}

void RoomClient::requestShareStart()
{
    QJsonObject msg;
    msg["type"] = "share_start";
    sendMessage(msg);
}

void RoomClient::requestShareStop()
{
    QJsonObject msg;
    msg["type"] = "share_stop";
    sendMessage(msg);
}

void RoomClient::requestGrabShare()
{
    QJsonObject msg;
    msg["type"] = "grab_share";
    sendMessage(msg);
}

void RoomClient::respondGrab(bool granted)
{
    QJsonObject msg;
    msg["type"] = "grab_respond";
    msg["granted"] = granted;
    sendMessage(msg);
}

void RoomClient::onReadyRead()
{
    while (m_socket && m_socket->canReadLine()) {
        QByteArray line = m_socket->readLine().trimmed();
        if (line.isEmpty()) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) continue;
        if (!doc.isObject()) continue;

        handleMessage(doc.object());
    }
}

void RoomClient::onSocketConnected()
{
    emit connected();

    if (!m_roomId.isEmpty() && !m_nickname.isEmpty()) {
        joinRoom(m_roomId, m_nickname);
    }
}

void RoomClient::onSocketDisconnected()
{
    emit disconnected();
}

void RoomClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    emit errorOccurred(m_socket ? m_socket->errorString() : "未知错误");
}

void RoomClient::handleMessage(const QJsonObject &msg)
{
    QString type = msg["type"].toString();

    if (type == "member_list") {
        QString roomId = msg["roomId"].toString();
        QList<MemberEntry> members;
        QJsonArray arr = msg["members"].toArray();
        for (const QJsonValue &v : arr) {
            MemberEntry entry;
            entry.name = v.toObject()["name"].toString();
            entry.isSharing = v.toObject()["isSharing"].toBool();
            members << entry;
        }
        emit memberListReceived(roomId, members);
    } else if (type == "member_joined") {
        emit memberJoined(msg["roomId"].toString(), msg["name"].toString(),
                          msg["isSharing"].toBool());
    } else if (type == "member_left") {
        emit memberLeft(msg["roomId"].toString(), msg["name"].toString());
    } else if (type == "share_started") {
        emit shareStarted(msg["roomId"].toString(), msg["name"].toString());
    } else if (type == "share_stopped") {
        emit shareStopped(msg["roomId"].toString(), msg["name"].toString());
    } else if (type == "share_rejected") {
        emit shareRejected(msg["reason"].toString());
    } else if (type == "grab_request") {
        emit grabRequested(msg["from"].toString());
    } else if (type == "grab_result") {
        emit grabResult(msg["granted"].toBool(), msg["from"].toString());
    }
}

void RoomClient::sendMessage(const QJsonObject &msg)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;

    QJsonDocument doc(msg);
    m_socket->write(doc.toJson(QJsonDocument::Compact) + "\n");
    m_socket->flush();
}
