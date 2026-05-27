#ifndef MEMBERLIST_H
#define MEMBERLIST_H

#include <QListWidget>

struct MemberInfo {
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
    void removeMember(const QString &name);
    void clearMembers();

    void setMyName(const QString &name);
    void updateMemberSharing(const QString &name, bool isSharing);

private:
    void setupUI();
    QWidget *createMemberItem(const MemberInfo &member);

    QString m_myName;
};

#endif // MEMBERLIST_H
