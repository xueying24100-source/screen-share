#pragma once

#include <QObject>
#include <QByteArray>
#include <QHostAddress>

#include "../media/sender.h"

class QTcpServer;
class QTcpSocket;

class TcpPacketTransport : public QObject, public INetworkTransport
{
    Q_OBJECT
public:
    explicit TcpPacketTransport(QObject *parent = nullptr);
    ~TcpPacketTransport() override;

    bool listen(quint16 port);
    void connectToPeer(const QString &host, quint16 port);
    void close();
    bool isConnected() const;
    QString peerDescription() const;

    bool sendPacket(const QByteArray &packet, bool reliable) override;

signals:
    void packetReceived(const QByteArray &packet);
    void connectedChanged(bool connected, const QString &message);
    void transportError(const QString &message);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketDisconnected();
    void onSocketError();

private:
    void adoptSocket(QTcpSocket *socket);
    void clearSocket();

    QTcpServer *m_server = nullptr;
    QTcpSocket *m_socket = nullptr;
    QByteArray m_readBuffer;
};
