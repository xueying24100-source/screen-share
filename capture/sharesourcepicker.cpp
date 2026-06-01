#include "sharesourcepicker.h"

#include "sourceenumerator.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
constexpr int kThumbW = 220;
constexpr int kThumbH = 124;

class SourceCard : public QFrame
{
    Q_OBJECT

public:
    explicit SourceCard(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setStyleSheet("QFrame{border:2px solid transparent;border-radius:8px;background:#171a21;}"
                      "QLabel{color:#d6d8de;}");

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);

        m_thumbLabel = new QLabel(this);
        m_thumbLabel->setFixedSize(kThumbW, kThumbH);
        m_thumbLabel->setAlignment(Qt::AlignCenter);
        m_thumbLabel->setStyleSheet("background:#0f1116;border-radius:4px;");

        m_titleLabel = new QLabel(this);
        m_titleLabel->setWordWrap(true);
        m_subtitleLabel = new QLabel(this);
        m_subtitleLabel->setStyleSheet("color:#8a93a3;font-size:11px;");

        layout->addWidget(m_thumbLabel, 0, Qt::AlignCenter);
        layout->addWidget(m_titleLabel);
        layout->addWidget(m_subtitleLabel);
    }

    void setSelected(bool selected)
    {
        setStyleSheet(selected
            ? "QFrame{border:2px solid #2D7CF7;border-radius:8px;background:#171a21;}QLabel{color:#d6d8de;}"
            : "QFrame{border:2px solid transparent;border-radius:8px;background:#171a21;}QLabel{color:#d6d8de;}");
    }

    void setThumbnail(const QPixmap &pixmap)
    {
        m_thumbLabel->setPixmap(pixmap.scaled(kThumbW, kThumbH, Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation));
    }

    void setTexts(const QString &title, const QString &subtitle)
    {
        m_titleLabel->setText(title);
        m_subtitleLabel->setText(subtitle);
    }

signals:
    void clicked();
    void doubleClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            emit clicked();
        }
        QFrame::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            emit doubleClicked();
        }
        QFrame::mouseDoubleClickEvent(event);
    }

private:
    QLabel *m_thumbLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
};

QPixmap placeholderThumbnail(const QString &text)
{
    QPixmap pixmap(kThumbW, kThumbH);
    pixmap.fill(QColor("#10141f"));
    QPainter painter(&pixmap);
    painter.setPen(QColor("#7f8796"));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
    return pixmap;
}
}

ShareSourcePicker::ShareSourcePicker(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("选择共享内容");
    setModal(true);
    resize(900, 620);

    auto *mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);
    m_screenTab = new QWidget(this);
    m_windowTab = new QWidget(this);
    m_tabs->addTab(m_screenTab, "整个屏幕");
    m_tabs->addTab(m_windowTab, "应用窗口");

    auto *screenScroll = new QScrollArea(this);
    screenScroll->setWidgetResizable(true);
    auto *screenContainer = new QWidget(screenScroll);
    m_screenGrid = new QGridLayout(screenContainer);
    m_screenGrid->setContentsMargins(8, 8, 8, 8);
    m_screenGrid->setSpacing(10);
    screenScroll->setWidget(screenContainer);
    auto *screenLayout = new QVBoxLayout(m_screenTab);
    screenLayout->addWidget(screenScroll);

    auto *windowScroll = new QScrollArea(this);
    windowScroll->setWidgetResizable(true);
    auto *windowContainer = new QWidget(windowScroll);
    m_windowGrid = new QGridLayout(windowContainer);
    m_windowGrid->setContentsMargins(8, 8, 8, 8);
    m_windowGrid->setSpacing(10);
    windowScroll->setWidget(windowContainer);
    auto *windowLayout = new QVBoxLayout(m_windowTab);
    windowLayout->addWidget(windowScroll);

    mainLayout->addWidget(m_tabs, 1);

    auto *optionRow = new QHBoxLayout;
    m_fpsCombo = new QComboBox(this);
    m_fpsCombo->addItems({"5", "10", "15", "24", "30"});
    m_fpsCombo->setCurrentText("15");
    optionRow->addStretch(1);
    optionRow->addWidget(new QLabel("帧率:", this));
    optionRow->addWidget(m_fpsCombo);
    mainLayout->addLayout(optionRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_startShareButton = new QPushButton("开始采集", this);
    buttons->addButton(m_startShareButton, QDialogButtonBox::AcceptRole);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_startShareButton, &QPushButton::clicked, this, [this]() {
        m_selection.fps = m_fpsCombo->currentText().toInt();
        accept();
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        m_selection.kind = (index == 0) ? ShareSelection::Kind::Screen : ShareSelection::Kind::Window;
        updateConfirmEnabled();
    });

    populateScreens();
    populateWindows();
    updateConfirmEnabled();
}

void ShareSourcePicker::populateScreens()
{
    QWidget *parent = parentWidget();
    const bool restoreParent = parent && parent->isVisible();
    if (restoreParent) {
        parent->hide();
        QApplication::processEvents();
    }

    const QList<ScreenInfo> screens = SourceEnumerator::enumerateScreens();
    const QList<QScreen*> qtScreens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        const ScreenInfo &info = screens.at(i);
        auto *card = new SourceCard(m_screenTab);
        card->setTexts(info.name,
                       QString("%1 x %2").arg(info.resolution.width()).arg(info.resolution.height()));

        QPixmap thumb = placeholderThumbnail("屏幕");
        if (info.index >= 0 && info.index < qtScreens.size() && qtScreens.at(info.index)) {
            thumb = qtScreens.at(info.index)->grabWindow(0);
        }
        card->setThumbnail(thumb);

        m_screenGrid->addWidget(card, i / 3, i % 3);

        connect(card, &SourceCard::clicked, this, [this, card, index = info.index]() {
            if (auto *oldCard = qobject_cast<SourceCard*>(m_selectedScreenWidget.data())) {
                oldCard->setSelected(false);
            }
            m_selectedScreenWidget = card;
            card->setSelected(true);
            setSelectedScreen(index);
        });
        connect(card, &SourceCard::doubleClicked, this, [this, card, index = info.index]() {
            if (auto *oldCard = qobject_cast<SourceCard*>(m_selectedScreenWidget.data())) {
                oldCard->setSelected(false);
            }
            m_selectedScreenWidget = card;
            card->setSelected(true);
            setSelectedScreen(index);
            accept();
        });
    }

    if (!screens.isEmpty()) {
        setSelectedScreen(screens.first().index);
        if (m_screenGrid->count() > 0) {
            if (auto *first = qobject_cast<SourceCard*>(m_screenGrid->itemAt(0)->widget())) {
                first->setSelected(true);
                m_selectedScreenWidget = first;
            }
        }
    }

    if (restoreParent) {
        parent->show();
        parent->raise();
    }
}

void ShareSourcePicker::populateWindows()
{
    const QList<WindowInfo> windows = SourceEnumerator::enumerateWindows();
    for (int i = 0; i < windows.size(); ++i) {
        const WindowInfo &info = windows.at(i);
        auto *card = new SourceCard(m_windowTab);
        card->setTexts(info.title, "应用窗口");
        card->setThumbnail(placeholderThumbnail("窗口"));
        m_windowGrid->addWidget(card, i / 3, i % 3);

        connect(card, &SourceCard::clicked, this, [this, card, hwnd = info.handle]() {
            if (auto *oldCard = qobject_cast<SourceCard*>(m_selectedWindowWidget.data())) {
                oldCard->setSelected(false);
            }
            m_selectedWindowWidget = card;
            card->setSelected(true);
            setSelectedWindow(hwnd);
        });
        connect(card, &SourceCard::doubleClicked, this, [this, card, hwnd = info.handle]() {
            if (auto *oldCard = qobject_cast<SourceCard*>(m_selectedWindowWidget.data())) {
                oldCard->setSelected(false);
            }
            m_selectedWindowWidget = card;
            card->setSelected(true);
            setSelectedWindow(hwnd);
            accept();
        });
    }
}

void ShareSourcePicker::setSelectedScreen(int index)
{
    m_selection.kind = ShareSelection::Kind::Screen;
    m_selection.screenIndex = index;
    m_selection.hwnd = 0;
    updateConfirmEnabled();
}

void ShareSourcePicker::setSelectedWindow(quintptr hwnd)
{
    m_selection.kind = ShareSelection::Kind::Window;
    m_selection.hwnd = hwnd;
    updateConfirmEnabled();
}

void ShareSourcePicker::updateConfirmEnabled()
{
    if (!m_startShareButton) {
        return;
    }

    const bool hasSelection = (m_selection.kind == ShareSelection::Kind::Screen)
        ? (m_selection.screenIndex >= 0)
        : (m_selection.hwnd != 0);
    m_startShareButton->setEnabled(hasSelection);
}

#include "sharesourcepicker.moc"

