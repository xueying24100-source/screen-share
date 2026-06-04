#pragma once

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "annotationwindow.h"
#include "audiocapturer.h"
#include "systemaudiocapturer.h"
#include "audiomixer.h"
#include "sender.h"
#include "sharetoolbar.h"
#include "audioplayer.h"
#include "cameramanager.h"
#include "networktransport.h"
#include "mediareceiver.h"
#include "screencapturer.h"
#include "sourceenumerator.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QFontMetrics>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QCheckBox>
#include <QStyle>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QApplication>
#include <QWindow>
#include <QCursor>
#include <QDebug>
#include <QStringList>
#include <QPolygonF>
#include <QPen>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRandomGenerator>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
