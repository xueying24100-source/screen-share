#ifndef ROOMPAGE_H
#define ROOMPAGE_H

#include <QPushButton>
#include <QPointF>
#include <QVector>

#include "screen_share/ScreenCaptureManager.h"

class ScreenView;
class MemberList;
class QLabel;
class QAudioSource;
class QIODevice;

struct MemberEntry;
struct RoomPageInfo {
    QString clientId;
    QString nickname;
    QString roomId;
};

class RoomPage : public QWidget
{
    Q_OBJECT

public:
    explicit RoomPage(QWidget *parent = nullptr);

    void setRoomInfo(const RoomPageInfo &info);
    void resetRoom();

public slots:
    void onMemberListReceived(const QString &roomId, const QList<MemberEntry> &members);
    void onMemberJoined(const QString &roomId, const QString &clientId, const QString &name, bool isSharing);
    void onMemberLeft(const QString &roomId, const QString &clientId, const QString &name);

    void onShareStarted(const QString &roomId, const QString &clientId, const QString &name, quint32 sourceId, int sourceType);
    void onShareStopped(const QString &roomId, const QString &clientId, const QString &name);
    void onShareRejected(const QString &reason);
    void onGrabRequested(const QString &fromName);
    void onGrabResult(bool granted, const QString &fromName);
    void onRemoteAnnotationStroke(const QString &fromName, const QVector<QPointF> &points);
    void onRemoteAnnotationClear(const QString &fromName);

signals:
    void leaveRoomRequested();
    void shareScreenRequested(const ScreenCaptureSourceInfo &source);
    void stopShareRequested();
    void grabShareRequested();
    void grabShareResponded(bool granted);
    void micToggleRequested(bool enabled);
    void annotationStrokeCommitted(const QVector<QPointF> &points);
    void annotationsCleared();

private:
    void setupUI();
    void onShareClicked();
    void onGrabClicked();
    void onMicClicked();
    void onPenClicked();
    void onClearAnnotationsClicked();
    void updateMemberCount();
    void setSharingUI(bool sharing);
    void startCapturePreview(const ScreenCaptureSourceInfo &source, const QString &ownerName);
    void stopLocalCapturePreview();
    bool selectCaptureSource(ScreenCaptureSourceInfo *source);
    bool startMicrophoneCapture();
    void stopMicrophoneCapture();

    RoomPageInfo m_info;

    QLabel *m_roomLabel;
    QLabel *m_memberCountLabel;
    ScreenView *m_screenView;
    MemberList *m_memberList;

    QPushButton *m_shareBtn;
    QPushButton *m_grabBtn;
    QPushButton *m_micBtn;
    QPushButton *m_penBtn;
    QPushButton *m_clearAnnotationsBtn;
    QPushButton *m_leaveBtn;
    ScreenCaptureManager *m_captureManager;
    QAudioSource *m_audioSource = nullptr;
    QIODevice *m_audioInput = nullptr;

    bool m_isSharing = false;
    bool m_micOn = false;
    bool m_hasLocalCapture = false;
    bool m_hasPendingCaptureSource = false;
    qint64 m_micBytesCaptured = 0;
    QString m_currentSharer;
    ScreenCaptureSourceInfo m_pendingCaptureSource;
};

#endif // ROOMPAGE_H
