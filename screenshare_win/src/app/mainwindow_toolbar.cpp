#include "mainwindow_impl_includes.h"

void MainWindow::showShareToolbar()
{
    if (!shareToolbar) {
        shareToolbar = new ShareToolbar(nullptr);

        connect(shareToolbar, &ShareToolbar::pauseToggled, this, [this](bool paused) {
            sharePaused = paused;
            if (shareTimer) {
                if (sharePaused) {
                    shareTimer->stop();
                } else if (sharing) {
                    shareTimer->start();
                    captureScreen();
                }
            }
            ui->labelStatus->setText(sharePaused
                                         ? QStringLiteral("状态：共享已暂停")
                                         : QStringLiteral("状态：正在共享 ") + currentShareSource + optionSummary());
        });

        connect(shareToolbar, &ShareToolbar::annotationToggled, this, [this](bool enabled) {
            setAnnotationEditingEnabled(enabled);
        });

        connect(shareToolbar, &ShareToolbar::micMuteToggled, this, [this](bool muted) {
            micMuted = muted;
            if (sharing && micCapturer) {
                if (micMuted) {
                    micCapturer->stop();
                } else {
                    micCapturer->start();
                }
            }
            ui->labelStatus->setText(QStringLiteral("状态：正在共享 ") + currentShareSource
                                     + (micMuted ? QStringLiteral("，麦克风已静音") : QStringLiteral("，麦克风已开启")));
        });

        connect(shareToolbar, &ShareToolbar::systemAudioToggled, this, [this](bool enabled) {
            if (checkShareAudio) {
                checkShareAudio->setChecked(enabled);
            }
            if (sharing && systemAudioCapturer) {
                systemAudioCapturer->setEnabled(enabled);
            }
            ui->labelStatus->setText(QStringLiteral("状态：正在共享 ") + currentShareSource
                                     + (enabled ? QStringLiteral("，共享声音已开启") : QStringLiteral("，共享声音已关闭")));
        });

        connect(shareToolbar, &ShareToolbar::localPlaybackToggled, this, [this](bool enabled) {
            if (!localAudioPlayer) {
                return;
            }
            if (enabled) {
                localAudioPlayer->start();
            } else {
                localAudioPlayer->stop();
            }
            if (sharing) {
                ui->labelStatus->setText(QStringLiteral("状态：正在共享 ") + currentShareSource
                                         + (enabled ? QStringLiteral("，本地回放已开启") : QStringLiteral("，本地回放已关闭")));
            }
        });

        connect(shareToolbar, &ShareToolbar::backRequested, this, [this]() {
            showNormal();
            raise();
            activateWindow();
            positionShareToolbar();
        });

        connect(shareToolbar, &ShareToolbar::stopRequested, this, [this]() {
            endShare();
        });
    }

    sharePaused = false;
    micMuted = false;
    shareToolbar->setPaused(false);
    shareToolbar->setAnnotationEnabled(annotationWindow && annotationWindow->isVisible());
    shareToolbar->setMicMuted(false);
    shareToolbar->setSystemAudioEnabled(checkShareAudio && checkShareAudio->isChecked());
    shareToolbar->setLocalPlaybackEnabled(localAudioPlayer && localAudioPlayer->isRunning());
    shareToolbar->show();
    shareToolbar->raise();
    positionShareToolbar();
}

void MainWindow::hideShareToolbar()
{
    if (shareToolbar) {
        shareToolbar->hide();
        shareToolbar->setPaused(false);
        shareToolbar->setAnnotationEnabled(false);
        shareToolbar->setLocalPlaybackEnabled(false);
    }
}

void MainWindow::positionShareToolbar()
{
    if (!shareToolbar || !shareToolbar->isVisible()) {
        return;
    }

    shareToolbar->adjustSize();
    QRect base = frameGeometry();
    QScreen *targetScreen = QGuiApplication::screenAt(base.center());
    if (!targetScreen) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    const QRect available = targetScreen ? targetScreen->availableGeometry() : QRect();

    int x = base.center().x() - shareToolbar->width() / 2;
    int y = base.top() + 8;

    if (available.isValid()) {
        x = qBound(available.left() + 8, x, available.right() - shareToolbar->width() - 8);
        y = qBound(available.top() + 8, y, available.bottom() - shareToolbar->height() - 8);
    }

    shareToolbar->move(x, y);
}

void MainWindow::setAnnotationEditingEnabled(bool enabled)
{
    if (!sharing) {
        return;
    }

    if (enabled) {
        showAnnotationWindow();
    } else if (annotationWindow && annotationWindow->isVisible()) {
        annotationWindow->hide();
        ui->btnAnnotate->setText("画笔");
        ui->labelStatus->setText("状态：正在共享 " + currentShareSource + "，画笔已关闭");
        if (currentShareType == ShareSourceType::Whiteboard) {
            updateWhiteboardPreview();
        } else {
            captureScreen();
        }
    }

    if (shareToolbar) {
        shareToolbar->setAnnotationEnabled(annotationWindow && annotationWindow->isVisible());
    }
}

