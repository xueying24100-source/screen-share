#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QColor>
#include <QDateTime>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRectF>
#include <QStringList>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

#import <CoreGraphics/CoreGraphics.h>

#include "screen_share/AnnotationTypes.h"
#include "screen_share/ScreenCaptureManager.h"

class AnnotationPreviewWidget : public QWidget {
public:
    explicit AnnotationPreviewWidget(QWidget *parent = nullptr)
        : QWidget(parent) {
        setMinimumSize(560, 360);
        setMouseTracking(true);
    }

    void setPlaceholderText(const QString &text) {
        placeholderText_ = text;
        if (frame_.isNull()) {
            update();
        }
    }

    void setFrame(const QImage &frame) {
        frame_ = frame;
        update();
    }

    void clearFrame() {
        frame_ = QImage();
        update();
    }

    void clearAnnotations() {
        if (!commands_.isEmpty()) {
            AnnotationCommand clearCommand;
            clearCommand.commandId = nextCommandId();
            clearCommand.objectId = clearCommand.commandId;
            clearCommand.userId = QStringLiteral("local-demo");
            clearCommand.tool = AnnotationTool::Clear;
            clearCommand.action = AnnotationAction::ClearAll;
            clearCommand.sourceCanvasSize = frame_.size();
            clearCommand.timestampMs = QDateTime::currentMSecsSinceEpoch();
            notifyCommandCommitted(clearCommand);
        }
        commands_.clear();
        drawing_ = false;
        currentCommand_ = AnnotationCommand();
        notifyAnnotationsChanged();
        update();
    }

    void setTool(AnnotationTool tool) {
        tool_ = tool;
    }

    AnnotationTool tool() const {
        return tool_;
    }

    void setStrokeColor(const QColor &color) {
        strokeColor_ = color;
        update();
    }

    QColor strokeColor() const {
        return strokeColor_;
    }

    void setStrokeWidth(int width) {
        strokeWidth_ = width;
        update();
    }

    int strokeWidth() const {
        return strokeWidth_;
    }

    int annotationCount() const {
        return commands_.size();
    }

    void setAnnotationsChangedCallback(std::function<void()> callback) {
        annotationsChangedCallback_ = std::move(callback);
    }

    void setCommandCommittedCallback(std::function<void(const AnnotationCommand &)> callback) {
        commandCommittedCallback_ = std::move(callback);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.fillRect(rect(), QColor("#000000"));

        const QRectF contentRect = renderedContentRect();
        if (!frame_.isNull()) {
            painter.drawImage(contentRect, frame_);
        }

        painter.setRenderHint(QPainter::Antialiasing, true);
        for (const auto &command : commands_) {
            drawCommand(&painter, contentRect, command);
        }

        if (drawing_) {
            drawCommand(&painter, contentRect, currentCommand_);
        }

        if (frame_.isNull()) {
            painter.setPen(Qt::white);
            painter.drawText(rect().adjusted(24, 24, -24, -24),
                             Qt::AlignCenter | Qt::TextWordWrap,
                             placeholderText_);
        }
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton || frame_.isNull()) {
            return;
        }

        const QRectF contentRect = renderedContentRect();
        const AnnotationPoint point = normalizedPoint(event->position(), contentRect);
        if (!isValidAnnotationPoint(point)) {
            return;
        }

        beginCommand(point);
        update();
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (!drawing_) {
            return;
        }

        const QRectF contentRect = renderedContentRect();
        const AnnotationPoint point = normalizedPoint(event->position(), contentRect);
        if (!isValidAnnotationPoint(point)) {
            return;
        }

        updateCurrentCommand(point);
        update();
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (!drawing_ || event->button() != Qt::LeftButton) {
            return;
        }

        const QRectF contentRect = renderedContentRect();
        const AnnotationPoint point = normalizedPoint(event->position(), contentRect);
        if (isValidAnnotationPoint(point)) {
            updateCurrentCommand(point);
        }

        commitCurrentCommand();
    }

private:
    QRectF renderedContentRect() const {
        if (frame_.isNull()) {
            return QRectF(rect());
        }

        QSizeF scaled = frame_.size();
        scaled.scale(size(), Qt::KeepAspectRatio);
        const qreal x = (width() - scaled.width()) / 2.0;
        const qreal y = (height() - scaled.height()) / 2.0;
        return QRectF(x, y, scaled.width(), scaled.height());
    }

    AnnotationPoint normalizedPoint(const QPointF &widgetPoint, const QRectF &contentRect) const {
        if (contentRect.width() <= 0.0 || contentRect.height() <= 0.0 ||
            !contentRect.contains(widgetPoint)) {
            return AnnotationPoint { -1.0f, -1.0f };
        }

        return AnnotationPoint {
            static_cast<float>((widgetPoint.x() - contentRect.left()) / contentRect.width()),
            static_cast<float>((widgetPoint.y() - contentRect.top()) / contentRect.height())
        };
    }

    QPointF denormalizedPoint(const AnnotationPoint &point, const QRectF &contentRect) const {
        return QPointF(contentRect.left() + point.x * contentRect.width(),
                       contentRect.top() + point.y * contentRect.height());
    }

    void drawCommand(QPainter *painter,
                     const QRectF &contentRect,
                     const AnnotationCommand &command) const {
        if (command.tool != AnnotationTool::Pen && command.tool != AnnotationTool::Rectangle) {
            return;
        }

        QPen pen(command.style.color,
                 command.style.width,
                 Qt::SolidLine,
                 Qt::RoundCap,
                 Qt::RoundJoin);
        painter->setPen(pen);

        if (command.tool == AnnotationTool::Pen) {
            if (command.points.size() == 1) {
                painter->drawPoint(denormalizedPoint(command.points.constFirst(), contentRect));
                return;
            }

            for (int index = 1; index < command.points.size(); ++index) {
                painter->drawLine(denormalizedPoint(command.points.at(index - 1), contentRect),
                                  denormalizedPoint(command.points.at(index), contentRect));
            }
            return;
        }

        const QRectF rect(QPointF(contentRect.left() + command.normalizedRect.left() * contentRect.width(),
                                  contentRect.top() + command.normalizedRect.top() * contentRect.height()),
                          QPointF(contentRect.left() + command.normalizedRect.right() * contentRect.width(),
                                  contentRect.top() + command.normalizedRect.bottom() * contentRect.height()));
        painter->drawRect(rect.normalized());
    }

    void beginCommand(const AnnotationPoint &point) {
        drawing_ = true;
        currentCommand_ = AnnotationCommand();
        currentCommand_.commandId = nextCommandId();
        currentCommand_.objectId = currentCommand_.commandId;
        currentCommand_.userId = QStringLiteral("local-demo");
        currentCommand_.tool = tool_;
        currentCommand_.action = AnnotationAction::Begin;
        currentCommand_.style.color = strokeColor_;
        currentCommand_.style.width = static_cast<float>(strokeWidth_);
        currentCommand_.sourceCanvasSize = frame_.size();
        currentCommand_.timestampMs = QDateTime::currentMSecsSinceEpoch();
        if (tool_ == AnnotationTool::Pen) {
            currentCommand_.points.append(point);
        } else if (tool_ == AnnotationTool::Rectangle) {
            currentCommand_.points.append(point);
            const QPointF qPoint = qPointFFromAnnotationPoint(point);
            currentCommand_.normalizedRect = QRectF(qPoint, qPoint);
        }
    }

    void updateCurrentCommand(const AnnotationPoint &point) {
        currentCommand_.action = AnnotationAction::Update;
        currentCommand_.timestampMs = QDateTime::currentMSecsSinceEpoch();
        if (currentCommand_.tool == AnnotationTool::Pen) {
            if (currentCommand_.points.isEmpty() || currentCommand_.points.constLast().x != point.x ||
                currentCommand_.points.constLast().y != point.y) {
                currentCommand_.points.append(point);
            }
            return;
        }

        if (currentCommand_.tool == AnnotationTool::Rectangle && !currentCommand_.points.isEmpty()) {
            const QPointF startPoint = qPointFFromAnnotationPoint(currentCommand_.points.constFirst());
            currentCommand_.normalizedRect =
                QRectF(startPoint, qPointFFromAnnotationPoint(point)).normalized();
            return;
        }

        if (currentCommand_.tool == AnnotationTool::Rectangle) {
            const QPointF qPoint = qPointFFromAnnotationPoint(point);
            currentCommand_.points.append(point);
            currentCommand_.normalizedRect = QRectF(qPoint, qPoint);
        }
    }

    void commitCurrentCommand() {
        currentCommand_.action = AnnotationAction::Commit;
        currentCommand_.timestampMs = QDateTime::currentMSecsSinceEpoch();
        bool committed = false;
        if (currentCommand_.tool == AnnotationTool::Pen && !currentCommand_.points.isEmpty()) {
            commands_.append(currentCommand_);
            committed = true;
        } else if (currentCommand_.tool == AnnotationTool::Rectangle &&
                   currentCommand_.normalizedRect.width() > 0.002 &&
                   currentCommand_.normalizedRect.height() > 0.002) {
            commands_.append(currentCommand_);
            committed = true;
        }
        if (committed) {
            notifyCommandCommitted(currentCommand_);
        }
        drawing_ = false;
        currentCommand_ = AnnotationCommand();
        notifyAnnotationsChanged();
        update();
    }

    QString nextCommandId() {
        ++commandSequence_;
        return QStringLiteral("annotation-%1").arg(commandSequence_);
    }

    QString placeholderText_ = QStringLiteral("选择来源后，点击“开始本地采集验证”。");
    QImage frame_;
    QVector<AnnotationCommand> commands_;
    AnnotationCommand currentCommand_;
    QColor strokeColor_ = QColor("#ff3b30");
    int strokeWidth_ = 3;
    AnnotationTool tool_ = AnnotationTool::Pen;
    bool drawing_ = false;
    int commandSequence_ = 0;
    std::function<void()> annotationsChangedCallback_;
    std::function<void(const AnnotationCommand &)> commandCommittedCallback_;

    void notifyAnnotationsChanged() {
        if (annotationsChangedCallback_) {
            annotationsChangedCallback_();
        }
    }

    void notifyCommandCommitted(const AnnotationCommand &command) {
        if (commandCommittedCallback_) {
            commandCommittedCallback_(command);
        }
    }
};

static QString sourceTypeText(ScreenCaptureSourceInfo::SourceType type) {
    return type == ScreenCaptureSourceInfo::SourceType::Display
        ? QStringLiteral("Display")
        : QStringLiteral("Window");
}

static QString annotationToolText(AnnotationTool tool) {
    switch (tool) {
    case AnnotationTool::Pen:
        return QStringLiteral("Pen");
    case AnnotationTool::Rectangle:
        return QStringLiteral("Rectangle");
    case AnnotationTool::Arrow:
        return QStringLiteral("Arrow");
    case AnnotationTool::Text:
        return QStringLiteral("Text");
    case AnnotationTool::Clear:
        return QStringLiteral("Clear");
    }

    return QStringLiteral("Unknown");
}

static QString annotationActionText(AnnotationAction action) {
    switch (action) {
    case AnnotationAction::Begin:
        return QStringLiteral("Begin");
    case AnnotationAction::Update:
        return QStringLiteral("Update");
    case AnnotationAction::Commit:
        return QStringLiteral("Commit");
    case AnnotationAction::ClearAll:
        return QStringLiteral("ClearAll");
    }

    return QStringLiteral("Unknown");
}

static QString annotationCommandSummary(const AnnotationCommand &command) {
    return QStringLiteral(
               "{\n"
               "  \"commandId\": \"%1\",\n"
               "  \"objectId\": \"%2\",\n"
               "  \"userId\": \"%3\",\n"
               "  \"tool\": \"%4\",\n"
               "  \"action\": \"%5\",\n"
               "  \"pointCount\": %6,\n"
               "  \"normalizedRect\": { \"x\": %7, \"y\": %8, \"width\": %9, \"height\": %10 },\n"
               "  \"style\": { \"color\": \"%11\", \"width\": %12 },\n"
               "  \"sourceCanvasSize\": { \"width\": %13, \"height\": %14 },\n"
               "  \"timestampMs\": %15\n"
               "}")
        .arg(command.commandId)
        .arg(command.objectId)
        .arg(command.userId)
        .arg(annotationToolText(command.tool))
        .arg(annotationActionText(command.action))
        .arg(command.points.size())
        .arg(command.normalizedRect.x(), 0, 'f', 4)
        .arg(command.normalizedRect.y(), 0, 'f', 4)
        .arg(command.normalizedRect.width(), 0, 'f', 4)
        .arg(command.normalizedRect.height(), 0, 'f', 4)
        .arg(command.style.color.name())
        .arg(command.style.width, 0, 'f', 1)
        .arg(command.sourceCanvasSize.width(), 0, 'f', 0)
        .arg(command.sourceCanvasSize.height(), 0, 'f', 0)
        .arg(command.timestampMs);
}

static QString resolutionPresetText(CaptureResolutionPreset preset) {
    switch (preset) {
    case CaptureResolutionPreset::Native:
        return QStringLiteral("原始分辨率");
    case CaptureResolutionPreset::Half:
        return QStringLiteral("1/2 分辨率");
    case CaptureResolutionPreset::HD720:
        return QStringLiteral("1280 x 720");
    case CaptureResolutionPreset::HD1080:
        return QStringLiteral("1920 x 1080");
    }

    return QStringLiteral("原始分辨率");
}

static QString sourceSummary(const ScreenCaptureSourceInfo &info) {
    return QStringLiteral(
               "type: %1\n"
               "id: %2\n"
               "name: %3\n"
               "size: %4 x %5\n"
               "scale: %6")
        .arg(sourceTypeText(info.type))
        .arg(info.id)
        .arg(info.name)
        .arg(info.size.width())
        .arg(info.size.height())
        .arg(info.scale, 0, 'f', 2);
}

static QString sourcePayload(const ScreenCaptureSourceInfo &info) {
    return QStringLiteral(
               "{\n"
               "  \"id\": %1,\n"
               "  \"type\": \"%2\",\n"
               "  \"name\": \"%3\",\n"
               "  \"size\": { \"width\": %4, \"height\": %5 },\n"
               "  \"scale\": %6\n"
               "}")
        .arg(info.id)
        .arg(sourceTypeText(info.type))
        .arg(info.name)
        .arg(info.size.width())
        .arg(info.size.height())
        .arg(info.scale, 0, 'f', 2);
}

static QString validationSummary(const ScreenCaptureSourceInfo &info,
                                 ScreenCaptureSourceInfo::SourceType expectedType) {
    QStringList lines;
    lines << QStringLiteral("接口校验结果：");
    lines << QStringLiteral("- type 匹配当前模式：%1")
                 .arg(info.type == expectedType ? QStringLiteral("通过") : QStringLiteral("失败"));
    lines << QStringLiteral("- id 有效：%1")
                 .arg(info.id > 0 ? QStringLiteral("通过") : QStringLiteral("失败"));
    lines << QStringLiteral("- name 非空：%1")
                 .arg(!info.name.trimmed().isEmpty() ? QStringLiteral("通过") : QStringLiteral("失败"));
    lines << QStringLiteral("- size 有效：%1")
                 .arg(info.size.width() > 0 && info.size.height() > 0
                          ? QStringLiteral("通过")
                          : QStringLiteral("失败"));
    lines << QStringLiteral("- scale 有效：%1")
                 .arg(info.scale > 0.0f ? QStringLiteral("通过") : QStringLiteral("失败"));
    lines << QStringLiteral("");
    lines << QStringLiteral("前端最少只需要保存：id + type");
    return lines.join('\n');
}

static QString preflightAccessSummary() {
    const bool allowed = CGPreflightScreenCaptureAccess();
    return QStringLiteral(
               "CGPreflightScreenCaptureAccess(): %1\n"
               "说明：这个值表示当前进程此刻看到的录屏权限状态。")
        .arg(allowed ? QStringLiteral("true") : QStringLiteral("false"));
}

static QString emptySourceSummary(ScreenCaptureSourceInfo::SourceType type, bool hasAccess) {
    if (!hasAccess) {
        return QStringLiteral("当前因为没有录屏权限，所以来源为空。");
    }

    return type == ScreenCaptureSourceInfo::SourceType::Display
        ? QStringLiteral("当前没有可用的屏幕来源。")
        : QStringLiteral("当前没有可用的窗口来源。");
}

static void promptForDisplayCaptureAccess(QWidget *parent) {
    CGRequestScreenCaptureAccess();
    QMessageBox::warning(
        parent,
        QStringLiteral("屏幕采集启动失败"),
        QStringLiteral(
            "当前进程看到的录屏权限仍然不可用，或者系统尚未把授权状态同步到这个进程。\n\n"
            "请确认“系统设置 > 隐私与安全性 > 录屏与系统录音”里已经允许 screen_capture.app，"
            "然后完全退出并重新打开这个 demo 后再试。"));
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QStringLiteral("macOS 来源枚举与本地采集验证 Demo"));
    window.resize(1380, 840);

    auto *modeCombo = new QComboBox;
    modeCombo->addItem(QStringLiteral("屏幕"), static_cast<int>(ScreenCaptureSourceInfo::SourceType::Display));
    modeCombo->addItem(QStringLiteral("窗口"), static_cast<int>(ScreenCaptureSourceInfo::SourceType::Window));

    auto *includeSelfCheck = new QCheckBox(QStringLiteral("包含 screen_capture 自身内容"));
    auto *resolutionCombo = new QComboBox;
    resolutionCombo->addItem(QStringLiteral("原始分辨率"), static_cast<int>(CaptureResolutionPreset::Native));
    resolutionCombo->addItem(QStringLiteral("1/2 分辨率"), static_cast<int>(CaptureResolutionPreset::Half));
    resolutionCombo->addItem(QStringLiteral("1280 x 720"), static_cast<int>(CaptureResolutionPreset::HD720));
    resolutionCombo->addItem(QStringLiteral("1920 x 1080"), static_cast<int>(CaptureResolutionPreset::HD1080));
    auto *audioCheck = new QCheckBox(QStringLiteral("采集系统音频"));
    auto *refreshButton = new QPushButton(QStringLiteral("刷新来源"));

    auto *sourceHintLabel = new QLabel(QStringLiteral("点击“刷新来源”开始枚举。"));
    auto *listWidget = new QListWidget;
    auto *countLabel = new QLabel(QStringLiteral("来源数量：0"));

    auto *detailTitle = new QLabel(QStringLiteral("当前选中来源"));
    auto *detailLabel = new QLabel(QStringLiteral("请先从左侧列表中选择一个来源。"));
    auto *copyButton = new QPushButton(QStringLiteral("复制当前来源数据"));

    auto *validationTitle = new QLabel(QStringLiteral("接口校验"));
    auto *validationLabel = new QLabel(QStringLiteral("请选择一个来源后查看校验结果。"));

    auto *permissionTitle = new QLabel(QStringLiteral("权限调试信息"));
    auto *permissionLabel = new QLabel(preflightAccessSummary());

    auto *previewTitle = new QLabel(QStringLiteral("本地采集验证"));
    auto *previewWidget = new AnnotationPreviewWidget;
    auto *previewStatusLabel = new QLabel(QStringLiteral("状态：未开始"));
    auto *audioStatusLabel = new QLabel(QStringLiteral("音频：关闭"));
    auto *startPreviewButton = new QPushButton(QStringLiteral("开始本地采集验证"));
    auto *stopPreviewButton = new QPushButton(QStringLiteral("停止采集"));
    auto *annotationToolCombo = new QComboBox;
    annotationToolCombo->addItem(QStringLiteral("自由画笔"),
                                 static_cast<int>(AnnotationTool::Pen));
    annotationToolCombo->addItem(QStringLiteral("矩形框"),
                                 static_cast<int>(AnnotationTool::Rectangle));
    auto *annotationColorCombo = new QComboBox;
    annotationColorCombo->addItem(QStringLiteral("红色"), QStringLiteral("#ff3b30"));
    annotationColorCombo->addItem(QStringLiteral("黄色"), QStringLiteral("#ffd60a"));
    annotationColorCombo->addItem(QStringLiteral("绿色"), QStringLiteral("#30d158"));
    auto *annotationWidthCombo = new QComboBox;
    annotationWidthCombo->addItem(QStringLiteral("细"), 2);
    annotationWidthCombo->addItem(QStringLiteral("中"), 4);
    annotationWidthCombo->addItem(QStringLiteral("粗"), 6);
    annotationWidthCombo->setCurrentIndex(1);
    auto *clearAnnotationsButton = new QPushButton(QStringLiteral("清空标注"));
    auto *annotationStatusLabel = new QLabel(QStringLiteral("标注：0 个对象"));
    auto *annotationCommandTitle = new QLabel(QStringLiteral("最近一条 AnnotationCommand"));
    auto *annotationCommandLabel = new QLabel(QStringLiteral("还没有产生标注命令。"));

    auto *handoffTitle = new QLabel(QStringLiteral("给前端同学的接入方式"));
    auto *handoffLabel = new QLabel(
        QStringLiteral(
            "1. 调用 enumerateDisplays() 或 enumerateWindows() 获取来源\n"
            "2. 在列表里展示 name / size / scale，保存用户选择的 id + type\n"
            "3. 可选配置：setIncludeCurrentApplicationContent() / setResolutionPreset() / setCapturesAudio()\n"
            "4. 调用 startCapture(id, type) 做本地预览或后续推流接入\n"
            "5. 画面回调走 frameCaptured(...)，音频回调走 audioDataCaptured(...)\n"
            "6. 标注协议统一使用 AnnotationTypes.h 里的 AnnotationCommand\n"
            "7. 停止时调用 stopCapture(...) 或 stopAllCaptures()"));

    listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    sourceHintLabel->setWordWrap(true);
    sourceHintLabel->setStyleSheet("color: #8a5a00;");
    detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detailLabel->setWordWrap(true);
    validationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    validationLabel->setWordWrap(true);
    permissionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    permissionLabel->setWordWrap(true);
    handoffLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    handoffLabel->setWordWrap(true);
    annotationCommandLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    annotationCommandLabel->setWordWrap(true);
    previewWidget->setStyleSheet("background-color: black;");
    stopPreviewButton->setEnabled(false);

    auto *leftLayout = new QVBoxLayout;
    auto *toolbarLayout = new QHBoxLayout;
    toolbarLayout->addWidget(new QLabel(QStringLiteral("来源类型：")));
    toolbarLayout->addWidget(modeCombo);
    toolbarLayout->addWidget(refreshButton);
    toolbarLayout->addStretch();

    auto *optionsLayout = new QHBoxLayout;
    optionsLayout->addWidget(includeSelfCheck);
    optionsLayout->addSpacing(12);
    optionsLayout->addWidget(new QLabel(QStringLiteral("共享分辨率：")));
    optionsLayout->addWidget(resolutionCombo);
    optionsLayout->addSpacing(12);
    optionsLayout->addWidget(audioCheck);
    optionsLayout->addStretch();

    leftLayout->addLayout(toolbarLayout);
    leftLayout->addLayout(optionsLayout);
    leftLayout->addWidget(new QLabel(QStringLiteral("可选来源")));
    leftLayout->addWidget(sourceHintLabel);
    leftLayout->addWidget(listWidget, 1);
    leftLayout->addWidget(countLabel);

    auto *detailCard = new QFrame;
    detailCard->setFrameShape(QFrame::StyledPanel);
    auto *detailLayout = new QVBoxLayout(detailCard);
    detailLayout->addWidget(detailTitle);
    detailLayout->addWidget(detailLabel);
    detailLayout->addWidget(copyButton);

    auto *validationCard = new QFrame;
    validationCard->setFrameShape(QFrame::StyledPanel);
    auto *validationLayout = new QVBoxLayout(validationCard);
    validationLayout->addWidget(validationTitle);
    validationLayout->addWidget(validationLabel);

    auto *previewCard = new QFrame;
    previewCard->setFrameShape(QFrame::StyledPanel);
    auto *previewLayout = new QVBoxLayout(previewCard);
    auto *previewToolbarLayout = new QHBoxLayout;
    previewToolbarLayout->addWidget(startPreviewButton);
    previewToolbarLayout->addWidget(stopPreviewButton);
    previewToolbarLayout->addStretch();
    auto *annotationToolbarLayout = new QHBoxLayout;
    annotationToolbarLayout->addWidget(new QLabel(QStringLiteral("本地标注工具：")));
    annotationToolbarLayout->addWidget(annotationToolCombo);
    annotationToolbarLayout->addWidget(annotationColorCombo);
    annotationToolbarLayout->addWidget(annotationWidthCombo);
    annotationToolbarLayout->addWidget(clearAnnotationsButton);
    annotationToolbarLayout->addStretch();
    previewLayout->addWidget(previewTitle);
    previewLayout->addWidget(previewStatusLabel);
    previewLayout->addWidget(audioStatusLabel);
    previewLayout->addLayout(previewToolbarLayout);
    previewLayout->addLayout(annotationToolbarLayout);
    previewLayout->addWidget(annotationStatusLabel);
    previewLayout->addWidget(previewWidget);
    previewLayout->addWidget(annotationCommandTitle);
    previewLayout->addWidget(annotationCommandLabel);

    auto *permissionCard = new QFrame;
    permissionCard->setFrameShape(QFrame::StyledPanel);
    auto *permissionLayout = new QVBoxLayout(permissionCard);
    permissionLayout->addWidget(permissionTitle);
    permissionLayout->addWidget(permissionLabel);

    auto *handoffCard = new QFrame;
    handoffCard->setFrameShape(QFrame::StyledPanel);
    auto *handoffLayout = new QVBoxLayout(handoffCard);
    handoffLayout->addWidget(handoffTitle);
    handoffLayout->addWidget(handoffLabel);

    auto *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(detailCard);
    rightLayout->addWidget(validationCard);
    rightLayout->addWidget(permissionCard);
    rightLayout->addWidget(previewCard);
    rightLayout->addWidget(handoffCard);
    rightLayout->addStretch();

    auto *mainLayout = new QHBoxLayout(&window);
    mainLayout->addLayout(leftLayout, 3);
    mainLayout->addLayout(rightLayout, 2);

    ScreenCaptureManager manager;
    manager.setIncludeCurrentApplicationContent(includeSelfCheck->isChecked());
    manager.setResolutionPreset(static_cast<CaptureResolutionPreset>(resolutionCombo->currentData().toInt()));
    manager.setCapturesAudio(audioCheck->isChecked());

    quint32 activeSourceId = 0;
    ScreenCaptureSourceInfo::SourceType activeSourceType = ScreenCaptureSourceInfo::SourceType::Display;
    bool hasActivePreview = false;
    qint64 receivedAudioBytes = 0;
    qint64 receivedAudioPackets = 0;

    auto refreshPermissionDebugInfo = [&]() {
        permissionLabel->setText(preflightAccessSummary());
    };

    auto refreshAnnotationStatus = [&]() {
        annotationStatusLabel->setText(
            QStringLiteral("标注：%1 个对象，当前工具：%2")
                .arg(previewWidget->annotationCount())
                .arg(annotationToolCombo->currentText()));
    };
    previewWidget->setAnnotationsChangedCallback(refreshAnnotationStatus);
    previewWidget->setCommandCommittedCallback([&](const AnnotationCommand &command) {
        annotationCommandLabel->setText(annotationCommandSummary(command));
    });

    auto applyAnnotationOptions = [&]() {
        previewWidget->setTool(
            static_cast<AnnotationTool>(annotationToolCombo->currentData().toInt()));
        previewWidget->setStrokeColor(QColor(annotationColorCombo->currentData().toString()));
        previewWidget->setStrokeWidth(annotationWidthCombo->currentData().toInt());
        refreshAnnotationStatus();
    };

    auto resetAudioStatus = [&]() {
        if (audioCheck->isChecked()) {
            audioStatusLabel->setText(QStringLiteral("音频：已启用，等待采集开始"));
        } else {
            audioStatusLabel->setText(QStringLiteral("音频：关闭"));
        }
    };

    auto stopPreview = [&]() {
        if (!hasActivePreview) {
            previewStatusLabel->setText(QStringLiteral("状态：未开始"));
            previewWidget->clearFrame();
            previewWidget->clearAnnotations();
            previewWidget->setPlaceholderText(QStringLiteral("选择来源后，点击“开始本地采集验证”。"));
            resetAudioStatus();
            refreshAnnotationStatus();
            return;
        }

        manager.stopCapture(activeSourceId, activeSourceType);
        hasActivePreview = false;
        activeSourceId = 0;
        activeSourceType = ScreenCaptureSourceInfo::SourceType::Display;
        receivedAudioBytes = 0;
        receivedAudioPackets = 0;
        previewStatusLabel->setText(QStringLiteral("状态：已停止"));
        previewWidget->clearFrame();
        previewWidget->clearAnnotations();
        previewWidget->setPlaceholderText(QStringLiteral("本地采集已停止。"));
        startPreviewButton->setEnabled(true);
        stopPreviewButton->setEnabled(false);
        resetAudioStatus();
        refreshAnnotationStatus();
        refreshPermissionDebugInfo();
    };

    auto applyCurrentOptions = [&]() {
        manager.setIncludeCurrentApplicationContent(includeSelfCheck->isChecked());
        manager.setResolutionPreset(
            static_cast<CaptureResolutionPreset>(resolutionCombo->currentData().toInt()));
        manager.setCapturesAudio(audioCheck->isChecked());
    };

    auto selectedMode = [&]() {
        return static_cast<ScreenCaptureSourceInfo::SourceType>(modeCombo->currentData().toInt());
    };

    auto populateSources = [&](const QVector<ScreenCaptureSourceInfo> &sources) {
        stopPreview();
        listWidget->clear();
        countLabel->setText(QStringLiteral("来源数量：%1").arg(sources.size()));

        const bool hasAccess = CGPreflightScreenCaptureAccess();
        const auto currentMode = selectedMode();
        if (sources.isEmpty()) {
            listWidget->setEnabled(false);
            sourceHintLabel->setText(emptySourceSummary(currentMode, hasAccess));
            detailLabel->setText(emptySourceSummary(currentMode, hasAccess));
            validationLabel->setText(
                !hasAccess
                    ? QStringLiteral("接口校验暂停：当前因为没有录屏权限，所以没有可用来源。")
                    : QStringLiteral("当前模式下没有可校验的来源。"));
            return;
        }

        listWidget->setEnabled(true);
        sourceHintLabel->setText(
            QStringLiteral("已枚举到 %1 个来源。当前分辨率模式：%2。")
                .arg(sources.size())
                .arg(resolutionPresetText(manager.resolutionPreset())));

        for (const auto &info : sources) {
            auto *item = new QListWidgetItem(
                QStringLiteral("%1 (%2 x %3)")
                    .arg(info.name)
                    .arg(info.size.width())
                    .arg(info.size.height()));
            item->setData(Qt::UserRole, QVariant::fromValue(info));
            listWidget->addItem(item);
        }

        listWidget->setCurrentRow(0);
        const auto info = listWidget->currentItem()->data(Qt::UserRole).value<ScreenCaptureSourceInfo>();
        detailLabel->setText(sourceSummary(info));
        validationLabel->setText(validationSummary(info, currentMode));
    };

    auto refreshSources = [&]() {
        applyCurrentOptions();
        refreshPermissionDebugInfo();
        if (selectedMode() == ScreenCaptureSourceInfo::SourceType::Display) {
            populateSources(manager.enumerateDisplays());
        } else {
            populateSources(manager.enumerateWindows());
        }
    };

    auto markConfigChanged = [&]() {
        if (hasActivePreview) {
            stopPreview();
            previewStatusLabel->setText(QStringLiteral("状态：配置已更新"));
            previewWidget->setPlaceholderText(QStringLiteral("采集配置已变更，请重新开始本地采集验证。"));
        } else {
            resetAudioStatus();
        }
    };

    QObject::connect(refreshButton, &QPushButton::clicked, [&]() {
        refreshSources();
    });

    QObject::connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int) {
        refreshSources();
    });

    QObject::connect(includeSelfCheck, &QCheckBox::toggled, [&](bool) {
        applyCurrentOptions();
        markConfigChanged();
        refreshSources();
    });

    QObject::connect(resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int) {
        applyCurrentOptions();
        markConfigChanged();
        if (!listWidget->isEnabled()) {
            sourceHintLabel->setText(emptySourceSummary(selectedMode(), CGPreflightScreenCaptureAccess()));
        } else {
            sourceHintLabel->setText(
                QStringLiteral("当前分辨率模式：%1。重新开始采集后生效。")
                    .arg(resolutionPresetText(manager.resolutionPreset())));
        }
    });

    QObject::connect(audioCheck, &QCheckBox::toggled, [&](bool) {
        applyCurrentOptions();
        markConfigChanged();
    });

    QObject::connect(annotationToolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int) {
        applyAnnotationOptions();
    });

    QObject::connect(annotationColorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int) {
        applyAnnotationOptions();
    });

    QObject::connect(annotationWidthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int) {
        applyAnnotationOptions();
    });

    QObject::connect(clearAnnotationsButton, &QPushButton::clicked, [&]() {
        previewWidget->clearAnnotations();
        refreshAnnotationStatus();
    });

    QObject::connect(listWidget, &QListWidget::currentItemChanged,
                     [&](QListWidgetItem *current, QListWidgetItem *) {
        if (!current) {
            detailLabel->setText(QStringLiteral("请先从左侧列表中选择一个来源。"));
            validationLabel->setText(QStringLiteral("请选择一个来源后查看校验结果。"));
            return;
        }

        const auto info = current->data(Qt::UserRole).value<ScreenCaptureSourceInfo>();
        detailLabel->setText(sourceSummary(info));
        validationLabel->setText(validationSummary(info, selectedMode()));
    });

    QObject::connect(copyButton, &QPushButton::clicked, [&]() {
        const auto *current = listWidget->currentItem();
        if (!current || !current->data(Qt::UserRole).isValid()) {
            return;
        }

        const auto info = current->data(Qt::UserRole).value<ScreenCaptureSourceInfo>();
        QGuiApplication::clipboard()->setText(sourcePayload(info));
    });

    QObject::connect(startPreviewButton, &QPushButton::clicked, [&]() {
        const auto *current = listWidget->currentItem();
        if (!current || !current->data(Qt::UserRole).isValid()) {
            previewStatusLabel->setText(QStringLiteral("状态：请先选择有效来源"));
            previewWidget->setPlaceholderText(QStringLiteral("当前没有可启动采集的来源。"));
            previewWidget->clearFrame();
            return;
        }

        const auto info = current->data(Qt::UserRole).value<ScreenCaptureSourceInfo>();
        applyCurrentOptions();
        refreshPermissionDebugInfo();

        if (hasActivePreview) {
            manager.stopCapture(activeSourceId, activeSourceType);
            hasActivePreview = false;
        }

        if (!manager.startCapture(info.id, info.type)) {
            previewStatusLabel->setText(QStringLiteral("状态：启动失败"));
            previewWidget->clearFrame();
            previewWidget->setPlaceholderText(
                QStringLiteral("无法启动本地采集，请检查权限、来源状态或共享配置。"));
            startPreviewButton->setEnabled(true);
            stopPreviewButton->setEnabled(false);
            resetAudioStatus();
            if (info.type == ScreenCaptureSourceInfo::SourceType::Display &&
                !CGPreflightScreenCaptureAccess()) {
                promptForDisplayCaptureAccess(&window);
            }
            refreshPermissionDebugInfo();
            return;
        }

        activeSourceId = info.id;
        activeSourceType = info.type;
        hasActivePreview = true;
        receivedAudioBytes = 0;
        receivedAudioPackets = 0;
        previewStatusLabel->setText(QStringLiteral("状态：正在采集 %1").arg(info.name));
        previewWidget->clearAnnotations();
        previewWidget->clearFrame();
        previewWidget->setPlaceholderText(
            QStringLiteral("正在等待首帧...\n当前分辨率模式：%1")
                .arg(resolutionPresetText(manager.resolutionPreset())));
        audioStatusLabel->setText(
            manager.capturesAudio()
                ? QStringLiteral("音频：已启用，等待数据...")
                : QStringLiteral("音频：关闭"));
        startPreviewButton->setEnabled(false);
        stopPreviewButton->setEnabled(true);
        refreshAnnotationStatus();
        refreshPermissionDebugInfo();
    });

    QObject::connect(stopPreviewButton, &QPushButton::clicked, [&]() {
        stopPreview();
    });

    QObject::connect(&manager, &ScreenCaptureManager::captureError,
                     [&](quint32 sourceId,
                         ScreenCaptureSourceInfo::SourceType sourceType,
                         const QString &error) {
        if (sourceId == 0) {
            detailLabel->setText(QStringLiteral("枚举失败：%1").arg(error));
            validationLabel->setText(QStringLiteral("接口校验中断：%1").arg(error));
        }

        if (hasActivePreview && sourceId == activeSourceId && sourceType == activeSourceType) {
            previewStatusLabel->setText(QStringLiteral("状态：采集失败"));
            previewWidget->clearFrame();
            previewWidget->setPlaceholderText(QStringLiteral("本地采集失败：%1").arg(error));
            startPreviewButton->setEnabled(true);
            stopPreviewButton->setEnabled(false);
            hasActivePreview = false;
            resetAudioStatus();
        }

        refreshPermissionDebugInfo();
    });

    QObject::connect(&manager, &ScreenCaptureManager::frameCaptured, &window,
                     [&](quint32 sourceId,
                         ScreenCaptureSourceInfo::SourceType sourceType,
                         const QImage &frame) {
        if (!hasActivePreview || sourceId != activeSourceId || sourceType != activeSourceType) {
            return;
        }

        previewWidget->setFrame(frame);
        previewStatusLabel->setText(QStringLiteral("状态：采集中，已收到画面"));
    });

    QObject::connect(&manager, &ScreenCaptureManager::audioDataCaptured, &window,
                     [&](quint32 sourceId,
                         ScreenCaptureSourceInfo::SourceType sourceType,
                         const QByteArray &audioData,
                         int sampleRate,
                         int channelCount) {
        if (!hasActivePreview || sourceId != activeSourceId || sourceType != activeSourceType) {
            return;
        }

        receivedAudioBytes += audioData.size();
        ++receivedAudioPackets;
        audioStatusLabel->setText(
            QStringLiteral("音频：已收到 %1 个数据包 / %2 字节，%3 Hz，%4 声道")
                .arg(receivedAudioPackets)
                .arg(receivedAudioBytes)
                .arg(sampleRate)
                .arg(channelCount));
    });

    QObject::connect(&manager, &ScreenCaptureManager::captureStopped,
                     [&](quint32 sourceId, ScreenCaptureSourceInfo::SourceType sourceType) {
        if (sourceId == activeSourceId && sourceType == activeSourceType) {
            hasActivePreview = false;
            activeSourceId = 0;
            activeSourceType = ScreenCaptureSourceInfo::SourceType::Display;
            startPreviewButton->setEnabled(true);
            stopPreviewButton->setEnabled(false);
            receivedAudioBytes = 0;
            receivedAudioPackets = 0;
            resetAudioStatus();
            refreshAnnotationStatus();
        }

        refreshPermissionDebugInfo();
    });

    applyAnnotationOptions();
    resetAudioStatus();
    refreshSources();
    window.show();
    return app.exec();
}
