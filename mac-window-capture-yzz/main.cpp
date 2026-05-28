#include "screencapturer.h"
#include "sharesourcepicker.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

class PreviewWidget : public QWidget
{
public:
    explicit PreviewWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(800, 480);
    }

    void setFrame(const QImage &frame)
    {
        m_frame = frame;
        m_placeholder = QString();
        update();
    }

    void clearFrame(const QString &placeholder = "等待开始采集...")
    {
        m_frame = QImage();
        m_placeholder = placeholder;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#0b1020"));

        if (!m_frame.isNull()) {
            const QPixmap pixmap = QPixmap::fromImage(m_frame).scaled(
                size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            const int x = (width() - pixmap.width()) / 2;
            const int y = (height() - pixmap.height()) / 2;
            painter.drawPixmap(x, y, pixmap);
            return;
        }

        painter.setPen(QColor("#8f98aa"));
        painter.setFont(QFont("Microsoft YaHei", 16));
        painter.drawText(rect(), Qt::AlignCenter, m_placeholder);
    }

private:
    QImage m_frame;
    QString m_placeholder = "等待开始采集...";
};

class DemoWindow : public QMainWindow
{
public:
    DemoWindow()
        : m_preview(new PreviewWidget(this))
        , m_capturer(new ScreenCapturer(this))
    {
        setWindowTitle("macOS 窗口采集 Demo - yzz");
        resize(1100, 720);

        auto *central = new QWidget(this);
        auto *layout = new QVBoxLayout(central);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);

        auto *toolbar = new QHBoxLayout;
        auto *startButton = new QPushButton("选择并开始采集", central);
        auto *stopButton = new QPushButton("停止采集", central);
        stopButton->setEnabled(false);

        toolbar->addWidget(startButton);
        toolbar->addWidget(stopButton);
        toolbar->addStretch(1);

        layout->addLayout(toolbar);
        layout->addWidget(m_preview, 1);
        setCentralWidget(central);
        statusBar()->showMessage("Ready");

        connect(startButton, &QPushButton::clicked, this, [this, stopButton]() {
            ShareSourcePicker picker(this);
            if (picker.exec() != QDialog::Accepted) {
                return;
            }

            const ShareSelection selection = picker.selection();
            m_capturer->stop();
            if (selection.kind == ShareSelection::Kind::Window) {
                m_capturer->startWindow(selection.hwnd, selection.fps);
                statusBar()->showMessage("Capturing window...");
            } else {
                m_capturer->startScreen(selection.screenIndex, selection.fps);
                statusBar()->showMessage("Capturing screen...");
            }
            stopButton->setEnabled(true);
        });

        connect(stopButton, &QPushButton::clicked, this, [this, stopButton]() {
            m_capturer->stop();
            m_preview->clearFrame();
            statusBar()->showMessage("Stopped");
            stopButton->setEnabled(false);
        });

        connect(m_capturer, &ScreenCapturer::frameCaptured,
                m_preview, &PreviewWidget::setFrame);
        connect(m_capturer, &ScreenCapturer::captureError, this, [this](const QString &msg) {
            m_preview->clearFrame("采集失败");
            statusBar()->showMessage(msg);
        });
        connect(m_capturer, &ScreenCapturer::frameMetadataChanged, this,
                [this](const CaptureFrameMetadata &meta) {
                    statusBar()->showMessage(
                        QString("%1 | %2x%3 | frame %4")
                            .arg(meta.backendName)
                            .arg(meta.sourceSize.width())
                            .arg(meta.sourceSize.height())
                            .arg(meta.frameIndex));
                });
    }

private:
    PreviewWidget *m_preview = nullptr;
    ScreenCapturer *m_capturer = nullptr;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    DemoWindow window;
    window.show();

    return QApplication::exec();
}

