#include "MemberList.h"

#include <QHBoxLayout>
#include <QLabel>

MemberList::MemberList(QWidget *parent)
    : QListWidget(parent)
{
    setupUI();
}

void MemberList::setMembers(const QList<MemberInfo> &members)
{
    clear();
    for (const auto &member : members) {
        addMember(member);
    }
}

void MemberList::addMember(const MemberInfo &member)
{
    QListWidgetItem *item = new QListWidgetItem(this);
    item->setSizeHint(QSize(0, 40));
    item->setData(Qt::UserRole, member.name);

    QWidget *widget = createMemberItem(member);
    setItemWidget(item, widget);
    addItem(item);
}

void MemberList::removeMember(const QString &name)
{
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *item = this->item(i);
        if (item->data(Qt::UserRole).toString() == name) {
            delete takeItem(i);
            break;
        }
    }
}

void MemberList::clearMembers()
{
    clear();
}

void MemberList::setMyName(const QString &name)
{
    m_myName = name;
}

void MemberList::updateMemberSharing(const QString &name, bool isSharing)
{
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *item = this->item(i);
        if (item->data(Qt::UserRole).toString() == name) {
            bool isMe = (name == m_myName);
            MemberInfo info{name, isSharing, isMe};
            QWidget *widget = createMemberItem(info);
            item->setSizeHint(QSize(0, 40));
            setItemWidget(item, widget);
            break;
        }
    }
}

void MemberList::setupUI()
{
    setFixedWidth(200);
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(R"(
        QListWidget {
            background-color: #2b2b2b;
            border: none;
            outline: none;
        }
        QListWidget::item {
            padding: 4px 8px;
            border-bottom: 1px solid #3a3a3a;
        }
        QListWidget::item:hover {
            background-color: #353535;
        }
    )");
}

QWidget *MemberList::createMemberItem(const MemberInfo &member)
{
    QWidget *widget = new QWidget(this);
    widget->setStyleSheet("background: transparent;");

    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(8, 4, 8, 4);

    // 用户图标
    QLabel *avatarLabel = new QLabel("👤", widget);
    avatarLabel->setFixedSize(24, 24);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setStyleSheet("font-size: 16px; background: transparent;");

    // 名称
    QString displayName = member.name;
    QStringList tags;

    if (member.isMe) {
        tags << "我";
    }
    if (member.isSharing) {
        tags << "共享中";
    }

    if (!tags.isEmpty()) {
        displayName += QString(" [%1]").arg(tags.join(", "));
    }

    QLabel *nameLabel = new QLabel(displayName, widget);
    nameLabel->setStyleSheet(
        member.isSharing
            ? "color: #4fc3f7; font-size: 12px; background: transparent;"
            : "color: #cccccc; font-size: 12px; background: transparent;");

    layout->addWidget(avatarLabel);
    layout->addWidget(nameLabel, 1);

    return widget;
}
