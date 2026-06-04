#include "networktransport.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QAbstractSocket>
#include <QDataStream>
#include <QDebug>
#include <QIODevice>
#include <QtGlobal>
#include <QThread>
#include <QMetaObject>

namespace {
constexpr quint32 kMaxEnvelopeSize = 96 * 1024 * 1024;
constexpr quint32 kMaxPacketSize = 96 * 1024 * 1024;
}

TcpPacketTransport::TcpPacketTransport(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &TcpPacketTransport::onNewConnection);
}

TcpPacketTransport::~TcpPacketTransport()
{
    close();
}

void TcpPacketTransport::setLocalUserName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (!trimmed.isEmpty()) {
        m_localUserName = trimmed;
    }
}

bool TcpPacketTransport::listen(quint16 port, const QString &meetingCode)
{
    close();
    m_serverMode = true;
    m_localUserId = QStringLiteral("host");
    m_localUserName = QStringLiteral("主机");
    m_meetingCode = meetingCode.trimmed();
    m_joinMeetingCode.clear();
    m_nextClientNumber = 2;

    if (!m_server->listen(QHostAddress::Any, port)) {
        emit transportError(QStringLiteral("创建会议失败：%1").arg(m_server->errorString()));
        return false;
    }

    emit localIdentityAssigned(m_localUserId, m_localUserName);
    emit participantJoined(m_localUserId, m_localUserName);
    emit connectedChanged(false, QStringLiteral("已创建会议，会议码：%1，端口 %2").arg(m_meetingCode.isEmpty() ? QStringLiteral("无") : m_meetingCode).arg(port));
    return true;
}

void TcpPacketTransport::connectToPeer(const QString &host, quint16 port, const QString &meetingCode)
{
    close();
    m_serverMode = false;
    m_localUserId = QStringLiteral("pending");
    m_localUserName = QStringLiteral("参会者");
    m_joinMeetingCode = meetingCode.trimmed();
    m_meetingCode.clear();

    QTcpSocket *socket = new QTcpSocket(this);
    m_clientSocket = socket;
    attachSocket(socket);
    connect(socket, &QTcpSocket::connected,
            this, &TcpPacketTransport::onClientConnected);

    socket->connectToHost(host, port);
    emit connectedChanged(false, QStringLiteral("正在加入会议，会议码：%1").arg(m_joinMeetingCode));
}

void TcpPacketTransport::close()
{
    if (m_server && m_server->isListening()) {
        m_server->close();
    }

    const QList<QTcpSocket*> sockets = m_readBuffers.keys();
    for (QTcpSocket *socket : sockets) {
        if (!socket) {
            continue;
        }
        socket->disconnect(this);
        socket->disconnectFromHost();
        socket->deleteLater();
    }

    m_readBuffers.clear();
    m_clients.clear();
    m_clientSocket = nullptr;
    m_serverMode = false;
    m_localUserId = QStringLiteral("local");
    m_localUserName = QStringLiteral("我");
    m_meetingCode.clear();
    m_joinMeetingCode.clear();
    m_nextClientNumber = 2;
    emit connectedChanged(false, QStringLiteral("会议连接已关闭"));
}

bool TcpPacketTransport::isConnected() const
{
    if (m_serverMode) {
        return !m_clients.isEmpty();
    }
    return m_clientSocket && m_clientSocket->state() == QAbstractSocket::ConnectedState;
}

QString TcpPacketTransport::peerDescription() const
{
    if (m_serverMode) {
        return QStringLiteral("主机模式，已连接 %1 人").arg(m_clients.size());
    }
    if (!m_clientSocket) {
        return QStringLiteral("未连接");
    }
    return QStringLiteral("%1:%2")
        .arg(m_clientSocket->peerAddress().toString())
        .arg(m_clientSocket->peerPort());
}

bool TcpPacketTransport::sendPacket(const QByteArray &packet, bool reliable)
{
    if (packet.isEmpty()) {
        return false;
    }

    // Sender/codec worker may call this from a worker thread. QTcpSocket belongs to this QObject thread.
    if (QThread::currentThread() != thread()) {
        const QByteArray packetCopy = packet;
        QMetaObject::invokeMethod(this, [this, packetCopy, reliable]() {
            sendPacket(packetCopy, reliable);
        }, Qt::QueuedConnection);
        return true;
    }

    if (m_serverMode) {
        if (m_clients.isEmpty()) {
            return false;
        }
        broadcastMedia(m_localUserId, m_localUserName, packet, nullptr, reliable);
        return true;
    }

    if (!m_clientSocket || m_clientSocket->state() != QAbstractSocket::ConnectedState) {
        return false;
    }
    return writeEnvelope(m_clientSocket,
                         EnvelopeType::Media,
                         m_localUserId,
                         m_localUserName,
                         packet,
                         reliable);
}

void TcpPacketTransport::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (!socket) {
            continue;
        }

        attachSocket(socket);

        ClientSession pending;
        pending.socket = socket;
        m_clients.insert(socket, pending);
        emit connectedChanged(true, QStringLiteral("检测到新连接，等待会议码校验"));
    }
}

void TcpPacketTransport::attachSocket(QTcpSocket *socket)
{
    if (!socket) {
        return;
    }
    socket->setParent(this);
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_readBuffers.insert(socket, QByteArray());

    connect(socket, &QTcpSocket::readyRead,
            this, &TcpPacketTransport::onSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected,
            this, &TcpPacketTransport::onSocketDisconnected);
    connect(socket, &QTcpSocket::errorOccurred,
            this, &TcpPacketTransport::onSocketError);
}

void TcpPacketTransport::onClientConnected()
{
    sendHello();
    emit connectedChanged(true, QStringLiteral("已连接主机，等待会议身份分配"));
}

void TcpPacketTransport::sendHello()
{
    if (!m_clientSocket) {
        return;
    }
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << m_localUserName << m_joinMeetingCode;
    writeEnvelope(m_clientSocket, EnvelopeType::Hello, m_localUserId, m_localUserName, payload, true);
}

QByteArray TcpPacketTransport::buildEnvelope(EnvelopeType type,
                                             const QString &senderId,
                                             const QString &userName,
                                             const QByteArray &payload) const
{
    QByteArray out;
    QDataStream stream(&out, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << static_cast<quint8>(type)
           << senderId
           << userName
           << static_cast<quint32>(payload.size());
    if (!payload.isEmpty()) {
        stream.writeRawData(payload.constData(), payload.size());
    }
    return out;
}

bool TcpPacketTransport::writeEnvelope(QTcpSocket *socket,
                                       EnvelopeType type,
                                       const QString &senderId,
                                       const QString &userName,
                                       const QByteArray &payload,
                                       bool reliable)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return false;
    }
    return writeFramed(socket, buildEnvelope(type, senderId, userName, payload), reliable);
}

bool TcpPacketTransport::writeFramed(QTcpSocket *socket, const QByteArray &envelope, bool reliable)
{
    if (!socket || envelope.isEmpty()) {
        return false;
    }

    // For video packets, drop when the TCP buffer is already large to preserve real-time behavior.
    if (!reliable && socket->bytesToWrite() > 4 * 1024 * 1024) {
        return false;
    }

    QByteArray framed;
    framed.reserve(static_cast<int>(sizeof(quint32)) + envelope.size());
    QDataStream stream(&framed, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << static_cast<quint32>(envelope.size());
    framed.append(envelope);

    const qint64 written = socket->write(framed);
    return written == framed.size();
}

void TcpPacketTransport::sendWelcomeTo(const ClientSession &session)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << session.userId << session.userName << participantPairs();
    writeEnvelope(session.socket, EnvelopeType::Welcome, m_localUserId, m_localUserName, payload, true);
}

void TcpPacketTransport::broadcastPeerJoined(const ClientSession &session, QTcpSocket *exceptSocket)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << session.userId << session.userName;

    for (const ClientSession &client : m_clients) {
        if (!client.socket || client.socket == exceptSocket || client.userId.isEmpty()) {
            continue;
        }
        writeEnvelope(client.socket, EnvelopeType::PeerJoined, session.userId, session.userName, payload, true);
    }
}

void TcpPacketTransport::broadcastPeerLeft(const QString &userId, const QString &userName, QTcpSocket *exceptSocket)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << userId << userName;

    for (const ClientSession &client : m_clients) {
        if (!client.socket || client.socket == exceptSocket || client.userId.isEmpty()) {
            continue;
        }
        writeEnvelope(client.socket, EnvelopeType::PeerLeft, userId, userName, payload, true);
    }
}

void TcpPacketTransport::broadcastMedia(const QString &senderId,
                                        const QString &userName,
                                        const QByteArray &innerPacket,
                                        QTcpSocket *exceptSocket,
                                        bool reliable)
{
    for (const ClientSession &client : m_clients) {
        if (!client.socket || client.socket == exceptSocket || client.userId.isEmpty()) {
            continue;
        }
        writeEnvelope(client.socket, EnvelopeType::Media, senderId, userName, innerPacket, reliable);
    }
}

void TcpPacketTransport::onSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !m_readBuffers.contains(socket)) {
        return;
    }

    QByteArray &buffer = m_readBuffers[socket];
    buffer.append(socket->readAll());

    while (true) {
        if (buffer.size() < static_cast<int>(sizeof(quint32))) {
            return;
        }

        quint32 envelopeSize = 0;
        QByteArray headerBytes = buffer.left(static_cast<int>(sizeof(quint32)));
        QDataStream sizeStream(&headerBytes, QIODevice::ReadOnly);
        sizeStream.setVersion(QDataStream::Qt_6_0);
        sizeStream >> envelopeSize;

        if (envelopeSize == 0 || envelopeSize > kMaxEnvelopeSize || envelopeSize > kMaxPacketSize) {
            emit transportError(QStringLiteral("收到异常会议数据包，已断开该连接"));
            removeSocket(socket, true);
            return;
        }

        const int framedSize = static_cast<int>(sizeof(quint32) + envelopeSize);
        if (buffer.size() < framedSize) {
            return;
        }

        QByteArray envelope = buffer.mid(sizeof(quint32), envelopeSize);
        buffer.remove(0, framedSize);
        handleEnvelope(socket, envelope);
    }
}

void TcpPacketTransport::handleEnvelope(QTcpSocket *socket, const QByteArray &envelope)
{
    QDataStream stream(envelope);
    stream.setVersion(QDataStream::Qt_6_0);

    quint8 typeRaw = 0;
    QString senderId;
    QString userName;
    quint32 payloadSize = 0;
    stream >> typeRaw >> senderId >> userName >> payloadSize;

    if (stream.status() != QDataStream::Ok || payloadSize > kMaxPacketSize) {
        emit transportError(QStringLiteral("无法解析会议数据包"));
        return;
    }

    QByteArray payload;
    payload.resize(static_cast<int>(payloadSize));
    const int readBytes = payload.isEmpty() ? 0 : stream.readRawData(payload.data(), payload.size());
    if (readBytes != payload.size()) {
        emit transportError(QStringLiteral("收到不完整会议数据包"));
        return;
    }

    const EnvelopeType type = static_cast<EnvelopeType>(typeRaw);
    if (m_serverMode) {
        handleServerEnvelope(socket, type, senderId, userName, payload);
    } else {
        handleClientEnvelope(socket, type, senderId, userName, payload);
    }
}

void TcpPacketTransport::handleServerEnvelope(QTcpSocket *socket,
                                              EnvelopeType type,
                                              const QString &senderId,
                                              const QString &userName,
                                              const QByteArray &payload)
{
    Q_UNUSED(senderId);
    Q_UNUSED(userName);

    if (type == EnvelopeType::Hello) {
        QString requestedName;
        QString requestedCode;
        QDataStream stream(payload);
        stream.setVersion(QDataStream::Qt_6_0);
        stream >> requestedName >> requestedCode;

        if (!m_meetingCode.isEmpty() && requestedCode.trimmed() != m_meetingCode) {
            QByteArray rejectPayload;
            QDataStream out(&rejectPayload, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_0);
            out << QStringLiteral("会议码错误，无法加入会议。请确认主机端显示的会议码后重试。");
            writeEnvelope(socket, EnvelopeType::Reject, m_localUserId, m_localUserName, rejectPayload, true);
            socket->disconnectFromHost();
            return;
        }

        if (m_clients.contains(socket)) {
            ClientSession session = m_clients.value(socket);
            if (session.userId.isEmpty()) {
                session.userId = makeNextClientId();
                ++m_nextClientNumber;
            }
            session.userName = requestedName.trimmed().isEmpty()
                                   ? QStringLiteral("用户%1").arg(session.userId.section('-', -1))
                                   : requestedName.trimmed();
            session.socket = socket;
            m_clients[socket] = session;

            sendWelcomeTo(session);
            broadcastPeerJoined(session, socket);
            emit participantJoined(session.userId, session.userName);
            emit connectedChanged(true, QStringLiteral("%1 已通过会议码加入，当前远端人数：%2")
                                  .arg(session.userName)
                                  .arg(m_clients.size()));
        }
        return;
    }

    if (type != EnvelopeType::Media) {
        return;
    }

    const ClientSession session = m_clients.value(socket);
    if (session.userId.isEmpty()) {
        return;
    }

    // Host UI receives this participant's media; other clients receive it via server relay.
    emit packetReceivedFromPeer(session.userId, payload);
    emit packetReceived(payload);
    broadcastMedia(session.userId, session.userName, payload, socket, false);
}

void TcpPacketTransport::handleClientEnvelope(QTcpSocket *socket,
                                              EnvelopeType type,
                                              const QString &senderId,
                                              const QString &userName,
                                              const QByteArray &payload)
{
    Q_UNUSED(socket);

    if (type == EnvelopeType::Welcome) {
        QString assignedId;
        QString assignedName;
        QStringList pairs;
        QDataStream stream(payload);
        stream.setVersion(QDataStream::Qt_6_0);
        stream >> assignedId >> assignedName >> pairs;

        if (!assignedId.isEmpty()) {
            m_localUserId = assignedId;
            m_localUserName = assignedName.isEmpty() ? assignedId : assignedName;
            emit localIdentityAssigned(m_localUserId, m_localUserName);
        }

        for (int i = 0; i + 1 < pairs.size(); i += 2) {
            emit participantJoined(pairs.at(i), pairs.at(i + 1));
        }
        emit connectedChanged(true, QStringLiteral("已加入会议，身份：%1").arg(m_localUserName));
        return;
    }

    if (type == EnvelopeType::Reject) {
        QString reason;
        QDataStream stream(payload);
        stream.setVersion(QDataStream::Qt_6_0);
        stream >> reason;
        emit transportError(reason.isEmpty() ? QStringLiteral("加入会议失败：会议码错误") : reason);
        if (m_clientSocket) {
            m_clientSocket->disconnectFromHost();
        }
        return;
    }

    if (type == EnvelopeType::PeerJoined) {
        QString id;
        QString name;
        QDataStream stream(payload);
        stream.setVersion(QDataStream::Qt_6_0);
        stream >> id >> name;
        if (!id.isEmpty()) {
            emit participantJoined(id, name.isEmpty() ? id : name);
        }
        return;
    }

    if (type == EnvelopeType::PeerLeft) {
        QString id;
        QString name;
        QDataStream stream(payload);
        stream.setVersion(QDataStream::Qt_6_0);
        stream >> id >> name;
        Q_UNUSED(name);
        if (!id.isEmpty()) {
            emit participantLeft(id);
        }
        return;
    }

    if (type == EnvelopeType::Media) {
        emit packetReceivedFromPeer(senderId, payload);
        emit packetReceived(payload);
        return;
    }
}

void TcpPacketTransport::onSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }
    removeSocket(socket, true);
}

void TcpPacketTransport::onSocketError()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }
    emit transportError(QStringLiteral("网络错误：%1").arg(socket->errorString()));
}

void TcpPacketTransport::removeSocket(QTcpSocket *socket, bool notifyPeerLeft)
{
    if (!socket) {
        return;
    }

    QString leavingId;
    QString leavingName;
    if (m_clients.contains(socket)) {
        const ClientSession session = m_clients.take(socket);
        leavingId = session.userId;
        leavingName = session.userName;
    }

    if (m_clientSocket == socket) {
        m_clientSocket = nullptr;
    }

    m_readBuffers.remove(socket);
    socket->disconnect(this);
    socket->disconnectFromHost();
    socket->deleteLater();

    if (notifyPeerLeft && !leavingId.isEmpty()) {
        broadcastPeerLeft(leavingId, leavingName, socket);
        emit participantLeft(leavingId);
        emit connectedChanged(!m_clients.isEmpty(), QStringLiteral("%1 已离开，当前远端人数：%2")
                              .arg(leavingName)
                              .arg(m_clients.size()));
    } else if (!m_serverMode) {
        emit connectedChanged(false, QStringLiteral("已断开主机连接"));
    }
}

QString TcpPacketTransport::makeNextClientId() const
{
    return QStringLiteral("user-%1").arg(m_nextClientNumber);
}

QString TcpPacketTransport::userNameForId(const QString &userId) const
{
    if (userId == m_localUserId) {
        return m_localUserName;
    }
    for (const ClientSession &client : m_clients) {
        if (client.userId == userId) {
            return client.userName;
        }
    }
    return userId;
}

QStringList TcpPacketTransport::participantPairs() const
{
    QStringList pairs;
    pairs << m_localUserId << m_localUserName;
    for (const ClientSession &client : m_clients) {
        if (client.userId.isEmpty()) {
            continue;
        }
        pairs << client.userId << client.userName;
    }
    return pairs;
}
