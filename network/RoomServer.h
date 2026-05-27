#ifndef ROOMSERVER_H
#define ROOMSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QSet>

struct ClientInfo {
    QTcpSocket *socket;
    QString nickname;
    QString roomId;
    bool isSharing = false;
};

class RoomServer : public QObject
{
    Q_OBJECT

public:
    explicit RoomServer(QObject *parent = nullptr);
    ~RoomServer();

    bool start(quint16 port = 9527);
    void stop();
    bool isRunning() const;
    quint16 port() const;

private:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

    void handleMessage(QTcpSocket *socket, const QJsonObject &msg);
    void sendToClient(QTcpSocket *socket, const QJsonObject &msg);
    void broadcastToRoom(const QString &roomId, const QJsonObject &msg,
                         QTcpSocket *exclude = nullptr);

    void handleJoin(QTcpSocket *socket, const QJsonObject &msg);
    void handleLeave(QTcpSocket *socket);
    void handleShareStart(QTcpSocket *socket);
    void handleShareStop(QTcpSocket *socket);
    void handleGrabShare(QTcpSocket *socket);
    void handleGrabRespond(QTcpSocket *socket, const QJsonObject &msg);

    QTcpSocket* findSharerInRoom(const QString &roomId) const;

    QJsonArray buildMemberList(const QString &roomId) const;

    QTcpServer *m_server;
    QMap<QTcpSocket*, ClientInfo> m_clients;
    QMap<QString, QSet<QTcpSocket*>> m_rooms;
    // 被抢者 socket -> 抢夺者 socket
    QMap<QTcpSocket*, QTcpSocket*> m_pendingGrabs;
};

#endif // ROOMSERVER_H
