#include "sharesourcepicker.h"

#include "sourceenumerator.h"

#include <QCheckBox>
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
#include <QApplication>
#include <QMetaObject>
#include <QPointer>
#include <QPixmap>
#include <QSize>

#include <thread>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif
#endif

namespace {
constexpr int kThumbW = 240;
constexpr int kThumbH = 135;

class SourceCard : public QFrame
{
    Q_OBJECT
public:
    explicit SourceCard(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFrameShape(QFrame::StyledPanel);
        setStyleSheet(QStringLiteral("QFrame{border:2px solid transparent;border-radius:8px;background:#171a21;}"
                                     "QLabel{color:#d6d8de;}"));

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);

        m_thumbLabel = new QLabel(this);
        m_thumbLabel->setFixedSize(kThumbW, kThumbH);
        m_thumbLabel->setAlignment(Qt::AlignCenter);
        m_thumbLabel->setStyleSheet(QStringLiteral("background:#0f1116;border-radius:4px;"));

        m_noteLabel = new QLabel(this);
        m_noteLabel->setWordWrap(true);
        m_noteLabel->setStyleSheet(QStringLiteral("color:#8a93a3;font-size:11px;"));
        m_noteLabel->hide();

        m_titleLabel = new QLabel(this);
        m_titleLabel->setWordWrap(true);
        m_subtitleLabel = new QLabel(this);

        layout->addWidget(m_noteLabel);
        layout->addWidget(m_thumbLabel, 0, Qt::AlignCenter);
        layout->addWidget(m_titleLabel);
        layout->addWidget(m_subtitleLabel);
    }

    void setSelected(bool selected)
    {
        setStyleSheet(selected
                          ? QStringLiteral("QFrame{border:2px solid #2D7CF7;border-radius:8px;background:#171a21;}"
                                           "QLabel{color:#d6d8de;}")
                          : QStringLiteral("QFrame{border:2px solid transparent;border-radius:8px;background:#171a21;}"
                                           "QLabel{color:#d6d8de;}"));
    }

    void setThumbnail(const QPixmap& pix)
    {
        m_thumbLabel->setPixmap(pix.scaled(kThumbW, kThumbH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    void setTexts(const QString& title, const QString& subtitle)
    {
        m_titleLabel->setText(title);
        m_subtitleLabel->setText(subtitle);
    }

    void setNoteText(const QString& text)
    {
        m_noteLabel->setText(text);
        m_noteLabel->setVisible(!text.isEmpty());
    }

signals:
    void clicked();
    void doubleClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            emit clicked();
        }
        QFrame::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            emit doubleClicked();
        }
        QFrame::mouseDoubleClickEvent(event);
    }

private:
    QLabel* m_thumbLabel{nullptr};
    QLabel* m_noteLabel{nullptr};
    QLabel* m_titleLabel{nullptr};
    QLabel* m_subtitleLabel{nullptr};
};

QPixmap placeholderThumbnail(const QString& text)
{
    QPixmap pix(kThumbW, kThumbH);
    pix.fill(QColor("#10141f"));
    QPainter painter(&pix);
    painter.setPen(QColor("#7f8796"));
    painter.drawText(pix.rect(), Qt::AlignCenter, text);
    return pix;
}

#ifdef Q_OS_WIN
QPixmap captureWindowThumbnail(HWND hwnd)
{
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return placeholderThumbnail(QStringLiteral("窗口"));
    }
    const int w = qMax(1, rect.right - rect.left);
    const int h = qMax(1, rect.bottom - rect.top);

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) {
        return placeholderThumbnail(QStringLiteral("窗口"));
    }
    HDC memDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, w, h);
    if (!memDc || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (memDc) DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return placeholderThumbnail(QStringLiteral("窗口"));
    }

    HGDIOBJ oldObj = SelectObject(memDc, bitmap);
    const bool printOk = PrintWindow(hwnd, memDc, PW_RENDERFULLCONTENT) != FALSE;

    QPixmap pix;
    if (printOk) {
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        QImage image(w, h, QImage::Format_ARGB32);
        if (!image.isNull() &&
            GetDIBits(memDc, bitmap, 0, static_cast<UINT>(h), image.bits(), &bmi, DIB_RGB_COLORS) > 0) {
            pix = QPixmap::fromImage(image);
        }
    }

    SelectObject(memDc, oldObj);
    DeleteObject(bitmap);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    if (!pix.isNull()) {
        return pix;
    }

    HICON icon = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0));
    if (!icon) {
        icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICON));
    }
    if (!icon) {
        return placeholderThumbnail(QStringLiteral("窗口"));
    }

    QPixmap iconPix(kThumbW, kThumbH);
    iconPix.fill(QColor("#10141f"));
    HDC iconDc = CreateCompatibleDC(nullptr);
    if (!iconDc) {
        return placeholderThumbnail(QStringLiteral("窗口"));
    }
    HDC desktopDc = GetDC(nullptr);
    HBITMAP iconBmp = desktopDc ? CreateCompatibleBitmap(desktopDc, kThumbW, kThumbH) : nullptr;
    if (desktopDc) {
        ReleaseDC(nullptr, desktopDc);
    }
    if (!iconBmp) {
        DeleteDC(iconDc);
        return placeholderThumbnail(QStringLiteral("窗口"));
    }
    HGDIOBJ oldIconObj = SelectObject(iconDc, iconBmp);
    RECT fillRect{0, 0, kThumbW, kThumbH};
    HBRUSH brush = CreateSolidBrush(RGB(16, 20, 31));
    FillRect(iconDc, &fillRect, brush);
    DeleteObject(brush);
    DrawIconEx(iconDc, (kThumbW - 64) / 2, (kThumbH - 64) / 2, icon, 64, 64, 0, nullptr, DI_NORMAL);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kThumbW;
    bmi.bmiHeader.biHeight = -kThumbH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    QImage image(kThumbW, kThumbH, QImage::Format_ARGB32);
    if (!image.isNull()) {
        GetDIBits(iconDc, iconBmp, 0, kThumbH, image.bits(), &bmi, DIB_RGB_COLORS);
        iconPix = QPixmap::fromImage(image);
    }

    SelectObject(iconDc, oldIconObj);
    DeleteObject(iconBmp);
    DeleteDC(iconDc);
    return iconPix;
}
#endif

} // namespace

ShareSourcePicker::ShareSourcePicker(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("选择共享内容"));
    setModal(true);
    resize(980, 680);

    auto* mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);
    m_screenTab = new QWidget(this);
    m_windowTab = new QWidget(this);
    m_tabs->addTab(m_screenTab, QStringLiteral("整个屏幕"));
    m_tabs->addTab(m_windowTab, QStringLiteral("应用窗口"));

    auto* screenScroll = new QScrollArea(this);
    screenScroll->setWidgetResizable(true);
    auto* screenContainer = new QWidget(screenScroll);
    m_screenGrid = new QGridLayout(screenContainer);
    m_screenGrid->setContentsMargins(8, 8, 8, 8);
    m_screenGrid->setSpacing(10);
    screenScroll->setWidget(screenContainer);
    auto* screenLayout = new QVBoxLayout(m_screenTab);
    screenLayout->addWidget(screenScroll);

    auto* windowScroll = new QScrollArea(this);
    windowScroll->setWidgetResizable(true);
    auto* windowContainer = new QWidget(windowScroll);
    m_windowGrid = new QGridLayout(windowContainer);
    m_windowGrid->setContentsMargins(8, 8, 8, 8);
    m_windowGrid->setSpacing(10);
    windowScroll->setWidget(windowContainer);
    auto* windowLayout = new QVBoxLayout(m_windowTab);
    windowLayout->addWidget(windowScroll);

    mainLayout->addWidget(m_tabs, 1);

    auto* optionRow = new QHBoxLayout;
    m_systemAudioCheck = new QCheckBox(QStringLiteral("共享计算机声音"), this);
    m_cursorCheck = new QCheckBox(QStringLiteral("显示鼠标指针"), this);
    m_borderCheck = new QCheckBox(QStringLiteral("显示共享边框"), this);
    m_fpsCombo = new QComboBox(this);
    m_fpsCombo->addItems({QStringLiteral("15"), QStringLiteral("24"), QStringLiteral("30"), QStringLiteral("60")});
    m_fpsCombo->setCurrentText(QStringLiteral("30"));

    m_systemAudioCheck->setChecked(true);
    m_cursorCheck->setChecked(true);
    m_borderCheck->setChecked(true);

    optionRow->addWidget(m_systemAudioCheck);
    optionRow->addWidget(m_cursorCheck);
    optionRow->addWidget(m_borderCheck);
    optionRow->addStretch(1);
    optionRow->addWidget(new QLabel(QStringLiteral("帧率:"), this));
    optionRow->addWidget(m_fpsCombo);

    mainLayout->addLayout(optionRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_startShareButton = new QPushButton(QStringLiteral("开始共享"), this);
    buttons->addButton(m_startShareButton, QDialogButtonBox::AcceptRole);
    m_startShareButton->setEnabled(false);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_startShareButton, &QPushButton::clicked, this, [this]() {
        m_selection.includeSystemAudio = m_systemAudioCheck->isChecked();
        m_selection.includeCursor = m_cursorCheck->isChecked();
        m_selection.showBorder = m_borderCheck->isChecked();
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

    connect(this, &QDialog::accepted, this, [this]() {
        m_selection.includeSystemAudio = m_systemAudioCheck->isChecked();
        m_selection.includeCursor = m_cursorCheck->isChecked();
        m_selection.showBorder = m_borderCheck->isChecked();
        m_selection.fps = m_fpsCombo->currentText().toInt();
    });

}

void ShareSourcePicker::populateScreens()
{
    QWidget* parent = parentWidget();
    const bool restoreParent = parent && parent->isVisible();
    if (restoreParent) {
        parent->hide();
        QApplication::processEvents();
    }

    const QList<ScreenInfo> screens = SourceEnumerator::enumerateScreens();
    for (int i = 0; i < screens.size(); ++i) {
        const ScreenInfo& info = screens.at(i);

        auto* card = new SourceCard(m_screenTab);
        card->setNoteText(QStringLiteral("（缩略图为打开本对话框瞬间的屏幕截图）"));
        card->setTexts(info.name, QStringLiteral("%1 × %2").arg(info.resolution.width()).arg(info.resolution.height()));

        QPixmap thumb = placeholderThumbnail(QStringLiteral("屏幕"));
        const QList<QScreen*> allScreens = QGuiApplication::screens();
        if (info.index >= 0 && info.index < allScreens.size() && allScreens.at(info.index)) {
            thumb = allScreens.at(info.index)->grabWindow(0);
        }
        card->setThumbnail(thumb);

        const int row = i / 3;
        const int col = i % 3;
        m_screenGrid->addWidget(card, row, col);

        connect(card, &SourceCard::clicked, this, [this, card, index = info.index]() {
            if (m_selectedScreenWidget) {
                if (auto* oldCard = qobject_cast<SourceCard*>(m_selectedScreenWidget.data())) {
                    oldCard->setSelected(false);
                }
            }
            m_selectedScreenWidget = card;
            card->setSelected(true);
            setSelectedScreen(index);
        });
        connect(card, &SourceCard::doubleClicked, this, [this, card, index = info.index]() {
            if (m_selectedScreenWidget) {
                if (auto* oldCard = qobject_cast<SourceCard*>(m_selectedScreenWidget.data())) {
                    oldCard->setSelected(false);
                }
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
            if (auto* first = qobject_cast<SourceCard*>(m_screenGrid->itemAt(0)->widget())) {
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
        const WindowInfo& info = windows.at(i);

        auto* card = new SourceCard(m_windowTab);
        card->setTexts(info.title, info.minimized ? QStringLiteral("已最小化") : QStringLiteral("应用窗口"));
        card->setThumbnail(placeholderThumbnail(QStringLiteral("加载中...")));

        const int row = i / 3;
        const int col = i % 3;
        m_windowGrid->addWidget(card, row, col);

        connect(card, &SourceCard::clicked, this, [this, card, hwnd = info.handle]() {
            if (m_selectedWindowWidget) {
                if (auto* oldCard = qobject_cast<SourceCard*>(m_selectedWindowWidget.data())) {
                    oldCard->setSelected(false);
                }
            }
            m_selectedWindowWidget = card;
            card->setSelected(true);
            setSelectedWindow(hwnd);
        });
        connect(card, &SourceCard::doubleClicked, this, [this, card, hwnd = info.handle]() {
            if (m_selectedWindowWidget) {
                if (auto* oldCard = qobject_cast<SourceCard*>(m_selectedWindowWidget.data())) {
                    oldCard->setSelected(false);
                }
            }
            m_selectedWindowWidget = card;
            card->setSelected(true);
            setSelectedWindow(hwnd);
            accept();
        });

#ifdef Q_OS_WIN
        QPointer<SourceCard> safeCard(card);
        std::thread([safeCard, hwnd = info.handle]() {
            QPixmap pix = captureWindowThumbnail(reinterpret_cast<HWND>(hwnd));
            QMetaObject::invokeMethod(qApp, [safeCard, pix]() {
                if (safeCard) {
                    safeCard->setThumbnail(pix);
                }
            }, Qt::QueuedConnection);
        }).detach();
#else
        card->setThumbnail(placeholderThumbnail(QStringLiteral("窗口")));
#endif
    }
}

void ShareSourcePicker::updateConfirmEnabled()
{
    if (m_tabs->currentIndex() == 0) {
        m_selection.kind = ShareSelection::Kind::Screen;
    } else {
        m_selection.kind = ShareSelection::Kind::Window;
    }

    if (m_startShareButton) {
        const bool hasSelection =
            (m_selection.kind == ShareSelection::Kind::Screen) ? (m_selection.screenIndex >= 0)
                                                                : (m_selection.hwnd != 0);
        m_startShareButton->setEnabled(hasSelection);
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

#include "sharesourcepicker.moc"
