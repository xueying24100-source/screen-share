#ifndef ROOMPAGE_H
#define ROOMPAGE_H

#include <QPushButton>

class ScreenView;
class MemberList;
class QLabel;

struct MemberEntry;
struct RoomPageInfo {
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
    void onMemberJoined(const QString &roomId, const QString &name, bool isSharing);
    void onMemberLeft(const QString &roomId, const QString &name);

    void onShareStarted(const QString &roomId, const QString &name);
    void onShareStopped(const QString &roomId, const QString &name);
    void onShareRejected(const QString &reason);
    void onGrabRequested(const QString &fromName);
    void onGrabResult(bool granted, const QString &fromName);

signals:
    void leaveRoomRequested();
    void shareScreenRequested();
    void stopShareRequested();
    void grabShareRequested();
    void grabShareResponded(bool granted);
    void micToggleRequested(bool enabled);

private:
    void setupUI();
    void onShareClicked();
    void onGrabClicked();
    void onMicClicked();
    void updateMemberCount();
    void setSharingUI(bool sharing);

    RoomPageInfo m_info;

    QLabel *m_roomLabel;
    QLabel *m_memberCountLabel;
    ScreenView *m_screenView;
    MemberList *m_memberList;

    QPushButton *m_shareBtn;
    QPushButton *m_grabBtn;
    QPushButton *m_micBtn;
    QPushButton *m_leaveBtn;

    bool m_isSharing = false;
    bool m_micOn = false;
    QString m_currentSharer;
};

#endif // ROOMPAGE_H
