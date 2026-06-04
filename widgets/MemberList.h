#ifndef MEMBERLIST_H
#define MEMBERLIST_H

#include <QListWidget>

struct MemberInfo {
    QString clientId;
    QString name;
    bool isSharing = false;
    bool isMe = false;
};

class MemberList : public QListWidget
{
    Q_OBJECT

public:
    explicit MemberList(QWidget *parent = nullptr);

    void setMembers(const QList<MemberInfo> &members);
    void addMember(const MemberInfo &member);
    void removeMember(const QString &clientId);
    void clearMembers();

    void setMyClientId(const QString &clientId);
    void updateMemberSharing(const QString &clientId, bool isSharing);

private:
    void setupUI();
    QWidget *createMemberItem(const MemberInfo &member);

    QString m_myClientId;
};

#endif // MEMBERLIST_H
