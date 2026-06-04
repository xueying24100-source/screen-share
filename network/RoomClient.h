#ifndef ROOMCLIENT_H
#define ROOMCLIENT_H

#include <QObject>
#include <QPointF>
#include <QTcpSocket>
#include <QVector>

struct MemberEntry {
    QString clientId;
    QString name;
    bool isSharing = false;
};

class RoomClient : public QObject
{
    Q_OBJECT

public:
    explicit RoomClient(QObject *parent = nullptr);
    ~RoomClient();

    bool connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;
    QString clientId() const;

    void joinRoom(const QString &roomId, const QString &nickname);
    void leaveRoom();
    void requestShareStart(quint32 sourceId, int sourceType);
    void requestShareStop();
    void requestGrabShare();
    void respondGrab(bool granted);
    void sendAnnotationStroke(const QVector<QPointF> &points);
    void sendAnnotationClear();

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);

    void memberListReceived(const QString &roomId, const QList<MemberEntry> &members);
    void memberJoined(const QString &roomId, const QString &clientId, const QString &name, bool isSharing);
    void memberLeft(const QString &roomId, const QString &clientId, const QString &name);

    void shareStarted(const QString &roomId, const QString &clientId, const QString &name, quint32 sourceId, int sourceType);
    void shareStopped(const QString &roomId, const QString &clientId, const QString &name);
    void shareRejected(const QString &reason);
    void grabRequested(const QString &fromName);
    void grabResult(bool granted, const QString &fromName);
    void annotationStrokeReceived(const QString &fromName, const QVector<QPointF> &points);
    void annotationClearReceived(const QString &fromName);

private:
    void onReadyRead();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

    void handleMessage(const QJsonObject &msg);
    void sendMessage(const QJsonObject &msg);

    QTcpSocket *m_socket;
    QString m_host;
    quint16 m_port = 0;
    QString m_roomId;
    QString m_nickname;
    QString m_clientId;
};

#endif // ROOMCLIENT_H
