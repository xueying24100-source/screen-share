#pragma once

#include <QObject>
#include <QByteArray>
#include <QHostAddress>
#include <QHash>
#include <QString>
#include <QStringList>

#include "sender.h"

class QTcpServer;
class QTcpSocket;

// v7 multiplayer transport.
// One executable can act as either:
//   - host/server: accepts multiple QTcpSocket clients and broadcasts media packets
//   - client: connects to one host and sends local media to that host
// The Sender still produces the original inner media packet. This transport wraps it in
// a small outer meeting envelope that carries senderId/userName for multi-participant UI.
class TcpPacketTransport : public QObject, public INetworkTransport
{
    Q_OBJECT
public:
    explicit TcpPacketTransport(QObject *parent = nullptr);
    ~TcpPacketTransport() override;

    bool listen(quint16 port, const QString &meetingCode = QString());
    void connectToPeer(const QString &host, quint16 port, const QString &meetingCode = QString());
    void close();
    bool isConnected() const;
    bool isServerMode() const { return m_serverMode; }
    QString peerDescription() const;

    QString localUserId() const { return m_localUserId; }
    QString localUserName() const { return m_localUserName; }
    QString meetingCode() const { return m_meetingCode; }
    void setLocalUserName(const QString &name);

    bool sendPacket(const QByteArray &packet, bool reliable) override;

signals:
    // Legacy signal kept for older two-person code paths.
    void packetReceived(const QByteArray &packet);

    // New multiplayer signal. senderId identifies whose media/control packet this is.
    void packetReceivedFromPeer(const QString &senderId, const QByteArray &packet);
    void localIdentityAssigned(const QString &userId, const QString &userName);
    void participantJoined(const QString &userId, const QString &userName);
    void participantLeft(const QString &userId);

    void connectedChanged(bool connected, const QString &message);
    void transportError(const QString &message);

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onClientConnected();
    void onSocketDisconnected();
    void onSocketError();

private:
    enum class EnvelopeType : quint8 {
        Hello = 1,
        Welcome = 2,
        Reject = 3,
        PeerJoined = 4,
        PeerLeft = 5,
        Media = 6
    };

    struct ClientSession {
        QString userId;
        QString userName;
        QTcpSocket *socket = nullptr;
    };

    void attachSocket(QTcpSocket *socket);
    void removeSocket(QTcpSocket *socket, bool notifyPeerLeft);

    QByteArray buildEnvelope(EnvelopeType type,
                             const QString &senderId,
                             const QString &userName,
                             const QByteArray &payload) const;
    bool writeEnvelope(QTcpSocket *socket,
                       EnvelopeType type,
                       const QString &senderId,
                       const QString &userName,
                       const QByteArray &payload,
                       bool reliable);
    bool writeFramed(QTcpSocket *socket, const QByteArray &envelope, bool reliable);

    void sendHello();
    void sendWelcomeTo(const ClientSession &session);
    void broadcastPeerJoined(const ClientSession &session, QTcpSocket *exceptSocket = nullptr);
    void broadcastPeerLeft(const QString &userId, const QString &userName, QTcpSocket *exceptSocket = nullptr);
    void broadcastMedia(const QString &senderId,
                        const QString &userName,
                        const QByteArray &innerPacket,
                        QTcpSocket *exceptSocket,
                        bool reliable);

    void handleEnvelope(QTcpSocket *socket, const QByteArray &envelope);
    void handleServerEnvelope(QTcpSocket *socket,
                              EnvelopeType type,
                              const QString &senderId,
                              const QString &userName,
                              const QByteArray &payload);
    void handleClientEnvelope(QTcpSocket *socket,
                              EnvelopeType type,
                              const QString &senderId,
                              const QString &userName,
                              const QByteArray &payload);

    QString makeNextClientId() const;
    QString userNameForId(const QString &userId) const;
    QStringList participantPairs() const;

    QTcpServer *m_server = nullptr;
    QTcpSocket *m_clientSocket = nullptr;
    QHash<QTcpSocket*, QByteArray> m_readBuffers;
    QHash<QTcpSocket*, ClientSession> m_clients;

    bool m_serverMode = false;
    QString m_localUserId = QStringLiteral("local");
    QString m_localUserName = QStringLiteral("我");
    QString m_meetingCode;
    QString m_joinMeetingCode;
    int m_nextClientNumber = 2;
};
