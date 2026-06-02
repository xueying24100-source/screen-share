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
constexpr quint32 kMaxPacketSize = 64 * 1024 * 1024;
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

bool TcpPacketTransport::listen(quint16 port)
{
    close();
    if (!m_server->listen(QHostAddress::Any, port)) {
        emit transportError(QStringLiteral("监听端口失败：%1").arg(m_server->errorString()));
        return false;
    }
    emit connectedChanged(false, QStringLiteral("已开启监听，端口 %1，等待另一个程序连接").arg(port));
    return true;
}

void TcpPacketTransport::connectToPeer(const QString &host, quint16 port)
{
    close();
    QTcpSocket *socket = new QTcpSocket(this);
    adoptSocket(socket);
    socket->connectToHost(host, port);
    emit connectedChanged(false, QStringLiteral("正在连接 %1:%2").arg(host).arg(port));
}

void TcpPacketTransport::close()
{
    if (m_server && m_server->isListening()) {
        m_server->close();
    }
    clearSocket();
    emit connectedChanged(false, QStringLiteral("网络连接已关闭"));
}

bool TcpPacketTransport::isConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

QString TcpPacketTransport::peerDescription() const
{
    if (!m_socket) {
        return QStringLiteral("未连接");
    }
    return QStringLiteral("%1:%2")
        .arg(m_socket->peerAddress().toString())
        .arg(m_socket->peerPort());
}

bool TcpPacketTransport::sendPacket(const QByteArray &packet, bool reliable)
{
    if (packet.isEmpty()) {
        return false;
    }

    // Sender v4 may call sendPacket() from the encode/network worker thread.
    // QTcpSocket must be used from its owning thread, so bounce the write back to this QObject thread.
    if (QThread::currentThread() != thread()) {
        const QByteArray packetCopy = packet;
        QMetaObject::invokeMethod(this, [this, packetCopy, reliable]() {
            sendPacket(packetCopy, reliable);
        }, Qt::QueuedConnection);
        return true;
    }

    if (!isConnected()) {
        return false;
    }

    // Video packets are intentionally unreliable in this phase. If the TCP write buffer is
    // already large, drop the current video packet instead of building seconds of latency.
    if (!reliable && m_socket && m_socket->bytesToWrite() > 4 * 1024 * 1024) {
        return false;
    }

    QByteArray framed;
    framed.reserve(static_cast<int>(sizeof(quint32)) + packet.size());
    QDataStream stream(&framed, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << static_cast<quint32>(packet.size());
    framed.append(packet);

    const qint64 written = m_socket->write(framed);
    if (written != framed.size()) {
        emit transportError(QStringLiteral("网络发送失败"));
        return false;
    }
    return true;
}

void TcpPacketTransport::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        adoptSocket(socket);
    }
    if (m_server->isListening()) {
        // 第一阶段只做一对一双开通信，收到一个连接后停止继续监听，避免多个客户端抢同一路画面。
        m_server->close();
    }
}

void TcpPacketTransport::adoptSocket(QTcpSocket *socket)
{
    clearSocket();
    m_socket = socket;
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_readBuffer.clear();

    connect(m_socket, &QTcpSocket::readyRead,
            this, &TcpPacketTransport::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        emit connectedChanged(true, QStringLiteral("已连接：%1").arg(peerDescription()));
    });
    connect(m_socket, &QTcpSocket::disconnected,
            this, &TcpPacketTransport::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &TcpPacketTransport::onSocketError);

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        emit connectedChanged(true, QStringLiteral("已连接：%1").arg(peerDescription()));
    }
}

void TcpPacketTransport::clearSocket()
{
    if (!m_socket) {
        return;
    }
    QTcpSocket *old = m_socket;
    m_socket = nullptr;
    old->disconnect(this);
    old->disconnectFromHost();
    old->deleteLater();
    m_readBuffer.clear();
}

void TcpPacketTransport::onReadyRead()
{
    if (!m_socket) {
        return;
    }

    m_readBuffer.append(m_socket->readAll());

    while (true) {
        if (m_readBuffer.size() < static_cast<int>(sizeof(quint32))) {
            return;
        }

        quint32 packetSize = 0;
        QByteArray headerBytes = m_readBuffer.left(static_cast<int>(sizeof(quint32)));
        QDataStream sizeStream(&headerBytes, QIODevice::ReadOnly);
        sizeStream.setVersion(QDataStream::Qt_6_0);
        sizeStream >> packetSize;

        if (packetSize == 0 || packetSize > kMaxPacketSize) {
            emit transportError(QStringLiteral("收到异常数据包，连接已关闭"));
            close();
            return;
        }

        const int framedSize = static_cast<int>(sizeof(quint32) + packetSize);
        if (m_readBuffer.size() < framedSize) {
            return;
        }

        QByteArray packet = m_readBuffer.mid(sizeof(quint32), packetSize);
        m_readBuffer.remove(0, framedSize);
        emit packetReceived(packet);
    }
}

void TcpPacketTransport::onSocketDisconnected()
{
    m_readBuffer.clear();
    emit connectedChanged(false, QStringLiteral("对端已断开"));
}

void TcpPacketTransport::onSocketError()
{
    if (!m_socket) {
        return;
    }
    emit transportError(QStringLiteral("网络错误：%1").arg(m_socket->errorString()));
}
