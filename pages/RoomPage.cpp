#include "RoomPage.h"
#include "widgets/ScreenView.h"
#include "widgets/MemberList.h"
#include "network/RoomClient.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QTabWidget>
#include <QVariant>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>

static QString sourceTypeLabel(ScreenCaptureSourceInfo::SourceType type)
{
    return type == ScreenCaptureSourceInfo::SourceType::Display
        ? QStringLiteral("屏幕")
        : QStringLiteral("窗口");
}

static QString sourceListText(const ScreenCaptureSourceInfo &source)
{
    return QStringLiteral("%1  %2 x %3  scale %4")
        .arg(source.name)
        .arg(source.size.width())
        .arg(source.size.height())
        .arg(source.scale, 0, 'f', 2);
}

RoomPage::RoomPage(QWidget *parent)
    : QWidget(parent)
    , m_screenView(nullptr)
    , m_memberList(nullptr)
    , m_shareBtn(nullptr)
    , m_grabBtn(nullptr)
    , m_micBtn(nullptr)
    , m_penBtn(nullptr)
    , m_clearAnnotationsBtn(nullptr)
    , m_leaveBtn(nullptr)
    , m_captureManager(new ScreenCaptureManager(this))
{
    setupUI();

    m_captureManager->setIncludeCurrentApplicationContent(false);
    m_captureManager->setResolutionPreset(CaptureResolutionPreset::HD720);
    m_captureManager->setCapturesAudio(false);

    connect(m_captureManager,
            &ScreenCaptureManager::frameCaptured,
            this,
            [this](quint32,
                   ScreenCaptureSourceInfo::SourceType,
                   const QImage &frame) {
                if (m_hasLocalCapture) {
                    m_screenView->updateFrame(frame);
                }
            });

    connect(m_captureManager,
            &ScreenCaptureManager::captureError,
            this,
            [this](quint32,
                   ScreenCaptureSourceInfo::SourceType,
                   const QString &error) {
                if (m_hasLocalCapture) {
                    m_hasLocalCapture = false;
                    m_screenView->setPlaceholderText("本机屏幕采集失败");
                }
                QMessageBox::warning(this, "屏幕采集失败", error);
            });

    connect(m_screenView,
            &ScreenView::annotationStrokeCommitted,
            this,
            &RoomPage::annotationStrokeCommitted);
}

void RoomPage::setRoomInfo(const RoomPageInfo &info)
{
    m_info = info;
    m_roomLabel->setText(QString("房间: %1").arg(info.roomId));

    QList<MemberInfo> members;
    members << MemberInfo{info.clientId, info.nickname, false, true};
    m_memberList->setMyClientId(info.clientId);
    m_memberList->setMembers(members);
    updateMemberCount();
}

void RoomPage::resetRoom()
{
    stopLocalCapturePreview();
    stopMicrophoneCapture();
    m_isSharing = false;
    m_micOn = false;
    m_currentSharer.clear();
    m_shareBtn->setText("共享屏幕");
    m_shareBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #2d5aa0;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #3670b8; }
        QPushButton:pressed { background-color: #244a82; }
    )");
    m_micBtn->setChecked(false);
    m_penBtn->setChecked(false);
    m_screenView->setAnnotationEnabled(false);
    m_screenView->clearContent();
    m_memberList->clearMembers();
    m_memberCountLabel->setText("在线: 0 人");
    m_roomLabel->setText("房间: --");
}

void RoomPage::onMemberListReceived(const QString &roomId, const QList<MemberEntry> &members)
{
    Q_UNUSED(roomId);

    QList<MemberInfo> memberInfos;
    m_currentSharer.clear();
    for (const MemberEntry &entry : members) {
        bool isMe = (entry.clientId == m_info.clientId);
        memberInfos << MemberInfo{entry.clientId, entry.name, entry.isSharing, isMe};
        if (entry.isSharing) {
            m_currentSharer = entry.name;
        }
    }
    m_memberList->setMyClientId(m_info.clientId);
    m_memberList->setMembers(memberInfos);
    updateMemberCount();
}

void RoomPage::onMemberJoined(const QString &roomId, const QString &clientId, const QString &name, bool isSharing)
{
    Q_UNUSED(roomId);
    m_memberList->addMember(MemberInfo{clientId, name, isSharing, clientId == m_info.clientId});
    if (isSharing) m_currentSharer = name;
    updateMemberCount();
}

void RoomPage::onMemberLeft(const QString &roomId, const QString &clientId, const QString &name)
{
    Q_UNUSED(roomId);
    m_memberList->removeMember(clientId);
    if (m_currentSharer == name) {
        m_currentSharer.clear();
    }
    updateMemberCount();
}

void RoomPage::onShareStarted(const QString &roomId, const QString &clientId, const QString &name, quint32 sourceId, int sourceType)
{
    Q_UNUSED(roomId);
    m_currentSharer = name;
    m_memberList->updateMemberSharing(clientId, true);

    ScreenCaptureSourceInfo source;
    source.id = sourceId;
    source.type = static_cast<ScreenCaptureSourceInfo::SourceType>(sourceType);

    if (clientId == m_info.clientId) {
        // 自己是共享者
        m_isSharing = true;
        setSharingUI(true);
        m_grabBtn->setDisabled(true);
        if (source.id == 0 && m_hasPendingCaptureSource) {
            source = m_pendingCaptureSource;
        }
        m_hasPendingCaptureSource = false;
        if (source.id > 0) {
            startCapturePreview(source, name);
        } else {
            m_screenView->setPlaceholderText("没有可用的共享来源");
        }
    } else {
        // 本机双客户端演示：观看端也采集同一个本机来源。
        if (source.id > 0) {
            startCapturePreview(source, name);
        } else {
            m_screenView->startSimulatedView(name);
        }
        m_grabBtn->setEnabled(true);
    }
}

void RoomPage::onShareStopped(const QString &roomId, const QString &clientId, const QString &name)
{
    Q_UNUSED(roomId);
    m_memberList->updateMemberSharing(clientId, false);
    m_currentSharer.clear();

    if (clientId == m_info.clientId) {
        // 自己停止共享
        m_isSharing = false;
        stopLocalCapturePreview();
        setSharingUI(false);
        m_grabBtn->setDisabled(false);
    } else {
        // 观看者：停止本机同步采集或模拟画面
        stopLocalCapturePreview();
    }
}

void RoomPage::onShareRejected(const QString &reason)
{
    QMessageBox::information(this, "无法共享", reason);
}

void RoomPage::onGrabRequested(const QString &fromName)
{
    int ret = QMessageBox::question(this, "抢共享请求",
        QString("%1 请求获取共享权限，是否同意？").arg(fromName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    emit grabShareResponded(ret == QMessageBox::Yes);
}

void RoomPage::onGrabResult(bool granted, const QString &fromName)
{
    if (granted) {
        // 后续会收到 share_started 广播自动切换状态
    } else {
        QMessageBox::information(this, "抢共享",
            QString("%1 拒绝了您的抢共享请求").arg(fromName));
    }
}

void RoomPage::onRemoteAnnotationStroke(const QString &fromName, const QVector<QPointF> &points)
{
    Q_UNUSED(fromName);
    m_screenView->addRemoteStroke(points);
}

void RoomPage::onRemoteAnnotationClear(const QString &fromName)
{
    Q_UNUSED(fromName);
    m_screenView->clearAnnotations();
}

void RoomPage::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // === 顶部信息栏 ===
    QWidget *topBar = new QWidget(this);
    topBar->setFixedHeight(40);
    topBar->setStyleSheet("background-color: #252525; border-bottom: 1px solid #3a3a3a;");

    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(16, 0, 16, 0);

    m_roomLabel = new QLabel("房间: --", topBar);
    m_roomLabel->setStyleSheet("color: #cccccc; font-size: 13px; background: transparent;");

    m_memberCountLabel = new QLabel("在线: 0 人", topBar);
    m_memberCountLabel->setStyleSheet("color: #888888; font-size: 12px; background: transparent;");

    topLayout->addWidget(m_roomLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_memberCountLabel);

    mainLayout->addWidget(topBar);

    // === 中间区域 ===
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setStyleSheet("QSplitter::handle { background: #3a3a3a; width: 1px; }");

    m_screenView = new ScreenView(splitter);
    m_screenView->setPlaceholderText("等待屏幕共享...");

    QWidget *rightPanel = new QWidget(splitter);
    rightPanel->setStyleSheet("background-color: #2b2b2b;");

    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    QLabel *memberTitle = new QLabel("成员列表", rightPanel);
    memberTitle->setAlignment(Qt::AlignCenter);
    memberTitle->setFixedHeight(36);
    memberTitle->setStyleSheet("color: #aaaaaa; font-size: 12px; border-bottom: 1px solid #3a3a3a; background: transparent;");

    m_memberList = new MemberList(rightPanel);

    rightLayout->addWidget(memberTitle);
    rightLayout->addWidget(m_memberList);

    splitter->addWidget(m_screenView);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    mainLayout->addWidget(splitter, 1);

    // === 底部工具栏 ===
    QWidget *toolbar = new QWidget(this);
    toolbar->setFixedHeight(72);
    toolbar->setStyleSheet("background-color: #252525; border-top: 1px solid #3a3a3a;");

    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setAlignment(Qt::AlignCenter);
    toolbarLayout->setSpacing(12);

    // 共享屏幕按钮
    m_shareBtn = new QPushButton("共享屏幕", toolbar);
    m_shareBtn->setFixedSize(100, 42);
    m_shareBtn->setCursor(Qt::PointingHandCursor);
    m_shareBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #2d5aa0;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #3670b8; }
        QPushButton:pressed { background-color: #244a82; }
    )");
    connect(m_shareBtn, &QPushButton::clicked, this, &RoomPage::onShareClicked);

    // 抢共享按钮
    m_grabBtn = new QPushButton("抢共享", toolbar);
    m_grabBtn->setFixedSize(80, 42);
    m_grabBtn->setCursor(Qt::PointingHandCursor);
    m_grabBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #cccccc;
            border: none;
            border-radius: 6px;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #4a4a4a; }
        QPushButton:pressed { background-color: #555555; }
    )");
    connect(m_grabBtn, &QPushButton::clicked, this, &RoomPage::onGrabClicked);

    // 麦克风按钮
    m_micBtn = new QPushButton("🎤 麦克风", toolbar);
    m_micBtn->setFixedSize(90, 42);
    m_micBtn->setCursor(Qt::PointingHandCursor);
    m_micBtn->setCheckable(true);
    m_micBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #cccccc;
            border: none;
            border-radius: 6px;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #4a4a4a; }
        QPushButton:pressed { background-color: #555555; }
        QPushButton:checked {
            background-color: #2d5aa0;
            color: white;
        }
    )");
    connect(m_micBtn, &QPushButton::clicked, this, &RoomPage::onMicClicked);

    m_penBtn = new QPushButton("画笔", toolbar);
    m_penBtn->setFixedSize(72, 42);
    m_penBtn->setCursor(Qt::PointingHandCursor);
    m_penBtn->setCheckable(true);
    m_penBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #cccccc;
            border: none;
            border-radius: 6px;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #4a4a4a; }
        QPushButton:pressed { background-color: #555555; }
        QPushButton:checked {
            background-color: #2d5aa0;
            color: white;
        }
    )");
    connect(m_penBtn, &QPushButton::clicked, this, &RoomPage::onPenClicked);

    m_clearAnnotationsBtn = new QPushButton("清空标注", toolbar);
    m_clearAnnotationsBtn->setFixedSize(90, 42);
    m_clearAnnotationsBtn->setCursor(Qt::PointingHandCursor);
    m_clearAnnotationsBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #cccccc;
            border: none;
            border-radius: 6px;
            font-size: 13px;
        }
        QPushButton:hover { background-color: #4a4a4a; }
        QPushButton:pressed { background-color: #555555; }
    )");
    connect(m_clearAnnotationsBtn, &QPushButton::clicked,
            this, &RoomPage::onClearAnnotationsClicked);

    // 退出房间按钮
    m_leaveBtn = new QPushButton("退出房间", toolbar);
    m_leaveBtn->setFixedSize(90, 42);
    m_leaveBtn->setCursor(Qt::PointingHandCursor);
    m_leaveBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #c0392b;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #e74c3c; }
        QPushButton:pressed { background-color: #a93226; }
    )");
    connect(m_leaveBtn, &QPushButton::clicked, this, &RoomPage::leaveRoomRequested);

    toolbarLayout->addWidget(m_shareBtn);
    toolbarLayout->addWidget(m_grabBtn);
    toolbarLayout->addWidget(m_micBtn);
    toolbarLayout->addWidget(m_penBtn);
    toolbarLayout->addWidget(m_clearAnnotationsBtn);
    toolbarLayout->addWidget(m_leaveBtn);

    mainLayout->addWidget(toolbar);
}

void RoomPage::onShareClicked()
{
    if (!m_isSharing) {
        ScreenCaptureSourceInfo source;
        if (!selectCaptureSource(&source)) {
            m_screenView->setPlaceholderText("已取消屏幕共享来源选择");
            return;
        }

        m_pendingCaptureSource = source;
        m_hasPendingCaptureSource = true;
        emit shareScreenRequested(source);
    } else {
        emit stopShareRequested();
    }
}

void RoomPage::onGrabClicked()
{
    emit grabShareRequested();
}

void RoomPage::onMicClicked()
{
    if (m_micBtn->isChecked()) {
        if (!startMicrophoneCapture()) {
            m_micBtn->setChecked(false);
            m_micOn = false;
            emit micToggleRequested(false);
            return;
        }
        m_micOn = true;
        emit micToggleRequested(true);
        return;
    }

    stopMicrophoneCapture();
    m_micOn = false;
    emit micToggleRequested(false);
}

void RoomPage::onPenClicked()
{
    m_screenView->setAnnotationEnabled(m_penBtn->isChecked());
}

void RoomPage::onClearAnnotationsClicked()
{
    m_screenView->clearAnnotations();
    emit annotationsCleared();
}

void RoomPage::updateMemberCount()
{
    int count = m_memberList->count();
    m_memberCountLabel->setText(QString("在线: %1 人").arg(count));
}

void RoomPage::setSharingUI(bool sharing)
{
    if (sharing) {
        m_shareBtn->setText("停止共享");
        m_shareBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #c0392b;
                color: white;
                border: none;
                border-radius: 6px;
                font-size: 13px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #e74c3c; }
            QPushButton:pressed { background-color: #a93226; }
        )");
        m_screenView->setPlaceholderText("正在共享屏幕...");
    } else {
        m_shareBtn->setText("共享屏幕");
        m_shareBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #2d5aa0;
                color: white;
                border: none;
                border-radius: 6px;
                font-size: 13px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #3670b8; }
            QPushButton:pressed { background-color: #244a82; }
        )");
        m_screenView->setPlaceholderText("等待屏幕共享...");
    }
}

void RoomPage::startCapturePreview(const ScreenCaptureSourceInfo &source, const QString &ownerName)
{
    stopLocalCapturePreview();

    if (!m_captureManager->startCapture(source.id, source.type)) {
        m_screenView->setPlaceholderText("本机屏幕采集启动失败");
        return;
    }

    m_hasLocalCapture = true;
    m_screenView->setPlaceholderText(
        QStringLiteral("正在等待%1首帧：%2")
            .arg(sourceTypeLabel(source.type),
                 source.name.isEmpty() ? ownerName : source.name));
}

void RoomPage::stopLocalCapturePreview()
{
    if (!m_captureManager) {
        return;
    }

    m_captureManager->stopAllCaptures();
    m_hasLocalCapture = false;
    if (m_screenView) {
        m_screenView->clearContent();
    }
    if (m_penBtn) {
        m_penBtn->setChecked(false);
    }
    if (m_screenView) {
        m_screenView->setAnnotationEnabled(false);
    }
}

bool RoomPage::selectCaptureSource(ScreenCaptureSourceInfo *source)
{
    if (!source) {
        return false;
    }

    const QVector<ScreenCaptureSourceInfo> displays = m_captureManager->enumerateDisplays();
    const QVector<ScreenCaptureSourceInfo> windows = m_captureManager->enumerateWindows();
    if (displays.isEmpty() && windows.isEmpty()) {
        QMessageBox::warning(this,
                             "选择共享来源",
                             "没有枚举到可共享屏幕或窗口。请检查 macOS 录屏权限后重启应用。");
        return false;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("选择共享来源");
    dialog.resize(620, 460);

    auto *layout = new QVBoxLayout(&dialog);
    auto *tabs = new QTabWidget(&dialog);
    auto *displayList = new QListWidget(tabs);
    auto *windowList = new QListWidget(tabs);

    auto populateList = [](QListWidget *list, const QVector<ScreenCaptureSourceInfo> &sources) {
        for (const auto &itemSource : sources) {
            auto *item = new QListWidgetItem(sourceListText(itemSource));
            item->setData(Qt::UserRole, QVariant::fromValue(itemSource));
            list->addItem(item);
        }
        if (list->count() > 0) {
            list->setCurrentRow(0);
        }
    };

    populateList(displayList, displays);
    populateList(windowList, windows);

    tabs->addTab(displayList, QStringLiteral("屏幕 (%1)").arg(displays.size()));
    tabs->addTab(windowList, QStringLiteral("窗口 (%1)").arg(windows.size()));
    if (displays.isEmpty() && !windows.isEmpty()) {
        tabs->setCurrentWidget(windowList);
    }

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                           Qt::Horizontal,
                                           &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText("开始共享");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");

    auto updateOkState = [&]() {
        auto *list = qobject_cast<QListWidget *>(tabs->currentWidget());
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(list && list->currentItem());
    };
    updateOkState();

    connect(tabs, &QTabWidget::currentChanged, &dialog, [&](int) {
        updateOkState();
    });
    connect(displayList, &QListWidget::itemDoubleClicked, &dialog, [&](QListWidgetItem *) {
        dialog.accept();
    });
    connect(windowList, &QListWidget::itemDoubleClicked, &dialog, [&](QListWidgetItem *) {
        dialog.accept();
    });
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    layout->addWidget(new QLabel("请选择要共享的屏幕或窗口。", &dialog));
    layout->addWidget(tabs, 1);
    layout->addWidget(buttonBox);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    auto *selectedList = qobject_cast<QListWidget *>(tabs->currentWidget());
    if (!selectedList || !selectedList->currentItem()) {
        return false;
    }

    *source = selectedList->currentItem()
                  ->data(Qt::UserRole)
                  .value<ScreenCaptureSourceInfo>();
    return source->id > 0;
}

bool RoomPage::startMicrophoneCapture()
{
    stopMicrophoneCapture();

    const QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (inputDevice.isNull()) {
        QMessageBox::warning(this, "麦克风", "没有找到可用的麦克风输入设备。");
        return false;
    }

    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!inputDevice.isFormatSupported(format)) {
        format = inputDevice.preferredFormat();
    }

    m_audioSource = new QAudioSource(inputDevice, format, this);
    m_audioSource->setBufferSize(format.bytesForDuration(100000));
    m_audioInput = m_audioSource->start();
    if (!m_audioInput) {
        delete m_audioSource;
        m_audioSource = nullptr;
        QMessageBox::warning(this, "麦克风", "麦克风采集启动失败，请检查系统麦克风权限。");
        return false;
    }

    m_micBytesCaptured = 0;
    m_micBtn->setText("麦克风中");
    connect(m_audioInput, &QIODevice::readyRead, this, [this]() {
        if (!m_audioInput) {
            return;
        }

        const QByteArray pcm = m_audioInput->readAll();
        if (pcm.isEmpty()) {
            return;
        }

        m_micBytesCaptured += pcm.size();
        m_micBtn->setToolTip(
            QStringLiteral("已采集麦克风 PCM：%1 字节").arg(m_micBytesCaptured));
        // TODO: Send `pcm` to WebRTC/audio transport when the media channel is integrated.
    });

    return true;
}

void RoomPage::stopMicrophoneCapture()
{
    if (m_audioSource) {
        m_audioSource->stop();
        m_audioSource->deleteLater();
        m_audioSource = nullptr;
    }
    m_audioInput = nullptr;
    m_micBytesCaptured = 0;
    if (m_micBtn) {
        m_micBtn->setText("麦克风");
        m_micBtn->setToolTip(QString());
    }
}
