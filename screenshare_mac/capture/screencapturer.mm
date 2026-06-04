#include "screencapturer.h"

#include <QGuiApplication>
#include <QMetaObject>
#include <QPixmap>
#include <QScreen>

#ifdef Q_OS_MACOS
#include <ApplicationServices/ApplicationServices.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <ScreenCaptureKit/ScreenCaptureKit.h>
#endif

namespace {
QImage prepareOutputFrame(const QImage &frame, const QSize &outputSize)
{
    if (frame.isNull()) {
        return {};
    }

    QImage output = frame;
    if (outputSize.isValid()
        && (frame.width() > outputSize.width() || frame.height() > outputSize.height())) {
        output = frame.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return output.convertToFormat(QImage::Format_RGB32);
}

#ifdef Q_OS_MACOS
QImage imageFromCGImage(CGImageRef imageRef)
{
    if (!imageRef) {
        return {};
    }

    const int width = static_cast<int>(CGImageGetWidth(imageRef));
    const int height = static_cast<int>(CGImageGetHeight(imageRef));
    if (width <= 0 || height <= 0) {
        return {};
    }

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return {};
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        image.bits(),
        static_cast<size_t>(width),
        static_cast<size_t>(height),
        8,
        static_cast<size_t>(image.bytesPerLine()),
        colorSpace,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);

    if (colorSpace) {
        CGColorSpaceRelease(colorSpace);
    }
    if (!context) {
        return {};
    }

    CGContextDrawImage(context, CGRectMake(0, 0, width, height), imageRef);
    CGContextRelease(context);
    return image;
}

QImage qImageFromPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    if (!pixelBuffer) {
        return QImage();
    }

    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    void *baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
    const int width = static_cast<int>(CVPixelBufferGetWidth(pixelBuffer));
    const int height = static_cast<int>(CVPixelBufferGetHeight(pixelBuffer));
    const int bytesPerRow = static_cast<int>(CVPixelBufferGetBytesPerRow(pixelBuffer));
    const OSType format = CVPixelBufferGetPixelFormatType(pixelBuffer);

    QImage frame;
    if (baseAddress && format == kCVPixelFormatType_32BGRA) {
        frame = QImage(static_cast<uchar *>(baseAddress),
                       width, height, bytesPerRow,
                       QImage::Format_ARGB32).copy();
    } else {
        frame = QImage(width, height, QImage::Format_ARGB32);
        frame.fill(Qt::black);
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    return frame;
}

bool isFrameSampleComplete(CMSampleBufferRef sampleBuffer)
{
    if (!sampleBuffer) {
        return false;
    }

    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
    if (!attachments || CFArrayGetCount(attachments) == 0) {
        return true;
    }

    CFDictionaryRef attachment =
        static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, 0));
    if (!attachment) {
        return true;
    }

    CFNumberRef statusValue =
        static_cast<CFNumberRef>(CFDictionaryGetValue(attachment, SCStreamFrameInfoStatus));
    if (!statusValue) {
        return true;
    }

    NSInteger status = SCFrameStatusComplete;
    CFNumberGetValue(statusValue, kCFNumberNSIntegerType, &status);
    return status == SCFrameStatusComplete || status == SCFrameStatusStarted;
}

SCShareableContent *copyShareableContentSync(NSError **outError)
{
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    __block SCShareableContent *capturedContent = nil;
    __block NSError *capturedError = nil;

    [SCShareableContent getShareableContentExcludingDesktopWindows:YES
                                               onScreenWindowsOnly:YES
                                                 completionHandler:^(SCShareableContent *shareableContent,
                                                                     NSError *error) {
        capturedContent = [shareableContent retain];
        capturedError = [error retain];
        dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    if (outError) {
        *outError = capturedError;
    } else if (capturedError) {
        [capturedError release];
    }
    return capturedContent;
}

QImage captureWindowRawWithCoreGraphics(quintptr windowId)
{
    if (windowId == 0) {
        return {};
    }

    CGImageRef imageRef = CGWindowListCreateImage(
        CGRectNull,
        kCGWindowListOptionIncludingWindow,
        static_cast<CGWindowID>(windowId),
        kCGWindowImageBoundsIgnoreFraming | kCGWindowImageNominalResolution);

    if (!imageRef) {
        return {};
    }

    const QImage frame = imageFromCGImage(imageRef);
    CGImageRelease(imageRef);
    return frame;
}

QImage captureWindowRawWithScreenCaptureKit(quintptr windowId)
{
    if (windowId == 0) {
        return {};
    }

    if (@available(macOS 14.0, *)) {
        // Continue with ScreenCaptureKit below.
    } else {
        return {};
    }

    @autoreleasepool {
        dispatch_semaphore_t contentSemaphore = dispatch_semaphore_create(0);
        __block SCWindow *targetWindow = nil;

        [SCShareableContent getShareableContentExcludingDesktopWindows:YES
                                                   onScreenWindowsOnly:YES
                                                     completionHandler:^(SCShareableContent *shareableContent, NSError *error) {
            if (!error && shareableContent) {
                const CGWindowID targetId = static_cast<CGWindowID>(windowId);
                for (SCWindow *window in shareableContent.windows) {
                    if (window.windowID == targetId) {
                        targetWindow = [window retain];
                        break;
                    }
                }
            }
            dispatch_semaphore_signal(contentSemaphore);
        }];

        if (dispatch_semaphore_wait(contentSemaphore, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC)) != 0) {
            return {};
        }
        if (!targetWindow) {
            return {};
        }

        SCContentFilter *filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:targetWindow];
        SCStreamConfiguration *configuration = [[SCStreamConfiguration alloc] init];

        const CGFloat scale = filter.pointPixelScale > 0 ? filter.pointPixelScale : 2.0;
        const CGRect contentRect = filter.contentRect;
        configuration.width = static_cast<size_t>(qMax<CGFloat>(1, contentRect.size.width * scale));
        configuration.height = static_cast<size_t>(qMax<CGFloat>(1, contentRect.size.height * scale));
        configuration.pixelFormat = kCVPixelFormatType_32BGRA;
        configuration.scalesToFit = YES;
        configuration.showsCursor = NO;
        configuration.ignoreShadowsSingleWindow = YES;
        configuration.preservesAspectRatio = YES;

        dispatch_semaphore_t captureSemaphore = dispatch_semaphore_create(0);
        __block QImage capturedFrame;

        [SCScreenshotManager captureImageWithFilter:filter
                                      configuration:configuration
                                  completionHandler:^(CGImageRef imageRef, NSError *error) {
            if (!error && imageRef) {
                capturedFrame = imageFromCGImage(imageRef);
            }
            dispatch_semaphore_signal(captureSemaphore);
        }];

        dispatch_semaphore_wait(captureSemaphore, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));

        [configuration release];
        [filter release];
        [targetWindow release];

        return capturedFrame;
    }
}

QImage captureWindowRaw(quintptr windowId, QString *backendName = nullptr)
{
    QImage frame = captureWindowRawWithScreenCaptureKit(windowId);
    if (!frame.isNull()) {
        if (backendName) {
            *backendName = "ScreenCaptureKit";
        }
        return frame;
    }

    frame = captureWindowRawWithCoreGraphics(windowId);
    if (!frame.isNull() && backendName) {
        *backendName = "CGWindowListCreateImage";
    }
    return frame;
}

// === SCStream 流式采集相关 ===

struct macOSStreamContext {
    SCStream *stream = nil;
    SCStreamFrameBridge *bridge = nil;
};

bool startScreenStream(ScreenCapturer *capturer, int screenIndex, int fps,
                       const QSize &outputSize, void **outContext)
{
    NSError *error = nil;
    SCShareableContent *content = copyShareableContentSync(&error);
    if (!content) {
        if (error) [error release];
        return false;
    }

    NSArray<SCDisplay *> *displays = content.displays;
    SCDisplay *targetDisplay = nil;

    if (screenIndex >= 0 && screenIndex < static_cast<int>(displays.count)) {
        targetDisplay = displays[screenIndex];
    } else if (displays.count > 0) {
        targetDisplay = displays[0];
    }

    if (!targetDisplay) {
        [content release];
        return false;
    }

    SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:targetDisplay
                                                excludingApplications:@[]
                                                     exceptingWindows:@[]];

    const QSize resolvedSize = outputSize.isValid() ? outputSize :
        QSize(static_cast<int>(CGDisplayPixelsWide(targetDisplay.displayID)),
              static_cast<int>(CGDisplayPixelsHigh(targetDisplay.displayID)));

    SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
    config.width = static_cast<size_t>(qMax(1, resolvedSize.width()));
    config.height = static_cast<size_t>(qMax(1, resolvedSize.height()));
    config.minimumFrameInterval = CMTimeMake(1, qMax(1, fps));
    config.queueDepth = 3;
    config.pixelFormat = kCVPixelFormatType_32BGRA;
    config.showsCursor = YES;
    config.scalesToFit = YES;

    auto *ctx = new macOSStreamContext();
    ctx->bridge = [[SCStreamFrameBridge alloc] init];
    ctx->bridge.capturer = capturer;

    SCStream *stream = [[SCStream alloc] initWithFilter:filter
                                        configuration:config
                                             delegate:ctx->bridge];
    [config release];
    [filter release];
    [content release];

    if (!stream) {
        [ctx->bridge release];
        delete ctx;
        return false;
    }

    dispatch_queue_t sampleQueue =
        dispatch_queue_create("screen-share.capture", DISPATCH_QUEUE_SERIAL);

    NSError *addOutputError = nil;
    if (![stream addStreamOutput:ctx->bridge
                            type:SCStreamOutputTypeScreen
              sampleHandlerQueue:sampleQueue
                           error:&addOutputError]) {
        if (addOutputError) [addOutputError release];
        [stream release];
        [ctx->bridge release];
        delete ctx;
        return false;
    }

    __block NSError *startError = nil;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    [stream startCaptureWithCompletionHandler:^(NSError *err) {
        startError = [err retain];
        dispatch_semaphore_signal(sem);
    }];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

    if (startError) {
        [startError release];
        NSError *removeError = nil;
        [stream removeStreamOutput:ctx->bridge type:SCStreamOutputTypeScreen error:&removeError];
        [stream release];
        [ctx->bridge release];
        delete ctx;
        return false;
    }

    ctx->stream = stream;
    *outContext = ctx;
    return true;
}

void stopScreenStream(void *context)
{
    auto *ctx = static_cast<macOSStreamContext *>(context);

    if (ctx->stream) {
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        [ctx->stream stopCaptureWithCompletionHandler:^(NSError *err) {
            Q_UNUSED(err);
            dispatch_semaphore_signal(sem);
        }];
        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

        NSError *removeError = nil;
        [ctx->stream removeStreamOutput:ctx->bridge type:SCStreamOutputTypeScreen error:&removeError];
        [ctx->stream release];
    }

    if (ctx->bridge) {
        [ctx->bridge release];
    }

    delete ctx;
}
#endif
}

#ifdef Q_OS_MACOS
@interface SCStreamFrameBridge : NSObject <SCStreamDelegate, SCStreamOutput>
@property (nonatomic, unsafe_unretained) ScreenCapturer *capturer;
@end

@implementation SCStreamFrameBridge
- (void)stream:(SCStream *)stream
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       ofType:(SCStreamOutputType)type {
    Q_UNUSED(stream);
    if (type != SCStreamOutputTypeScreen) {
        return;
    }
    if (!isFrameSampleComplete(sampleBuffer)) {
        return;
    }

    CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    QImage rawImage = qImageFromPixelBuffer(pixelBuffer);
    if (rawImage.isNull()) {
        return;
    }

    ScreenCapturer *capturer = self.capturer;
    const QSize outputSize = capturer->outputSize();
    QImage frame = prepareOutputFrame(rawImage, outputSize);

    QMetaObject::invokeMethod(capturer, [capturer, frame]() {
        emit capturer->frameCaptured(frame);
    }, Qt::QueuedConnection);
}

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error {
    Q_UNUSED(stream);
    if (!error) {
        return;
    }

    ScreenCapturer *capturer = self.capturer;
    QString message = QString::fromNSString(error.localizedDescription);
    QMetaObject::invokeMethod(capturer, [capturer, message]() {
        emit capturer->captureError(message);
    }, Qt::QueuedConnection);
}
@end
#endif

ScreenCapturer::ScreenCapturer(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &ScreenCapturer::captureFrame);
}

ScreenCapturer::~ScreenCapturer()
{
    stop();
}

QImage ScreenCapturer::captureWindowOnce(quintptr windowId, const QSize &outputSize)
{
#ifdef Q_OS_MACOS
    return prepareOutputFrame(captureWindowRaw(windowId), outputSize);
#else
    Q_UNUSED(windowId);
    Q_UNUSED(outputSize);
    return {};
#endif
}

void ScreenCapturer::start(int fps)
{
    if (m_running) {
        return;
    }

    m_frameIndex = 0;

#ifdef Q_OS_MACOS
    if (m_captureMode == CaptureMode::IndexedScreen ||
        m_captureMode == CaptureMode::PrimaryScreen) {
        if (@available(macOS 13.0, *)) {
            if (startScreenStream(this, m_screenIndex, fps, m_outputSize, &m_streamContext)) {
                m_running = true;
                return;
            }
        }
        // SCStream 不可用，降级到定时器截图
    }
#endif

    const int interval = (fps > 0) ? qMax(16, 1000 / fps) : 66;
    m_timer->start(interval);
    m_running = true;
}

void ScreenCapturer::startScreen(int screenIndex, int fps)
{
    if (m_running) {
        stop();
    }

    m_captureMode = CaptureMode::IndexedScreen;
    m_screenIndex = screenIndex;
    m_windowHandle = 0;
    start(fps);
}

void ScreenCapturer::startWindow(quintptr windowId, int fps)
{
    if (m_running) {
        stop();
    }

    if (windowId == 0) {
        emit captureError("Invalid window handle");
        return;
    }

    m_captureMode = CaptureMode::Window;
    m_windowHandle = windowId;
    start(fps);
}

void ScreenCapturer::stop()
{
    if (!m_running) {
        return;
    }

#ifdef Q_OS_MACOS
    if (m_streamContext) {
        stopScreenStream(m_streamContext);
        m_streamContext = nullptr;
        m_running = false;
        m_frameIndex = 0;
        return;
    }
#endif

    m_timer->stop();
    m_running = false;
    m_frameIndex = 0;
}

void ScreenCapturer::captureFrame()
{
    if (m_captureMode == CaptureMode::Window) {
        captureWindow();
    } else {
        captureScreen();
    }
}

void ScreenCapturer::captureScreen()
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    QScreen *screen = nullptr;

    if (m_captureMode == CaptureMode::PrimaryScreen) {
        screen = QGuiApplication::primaryScreen();
    } else if (m_screenIndex >= 0 && m_screenIndex < screens.size()) {
        screen = screens.at(m_screenIndex);
    }

    if (!screen) {
        emit captureError("Screen not available");
        return;
    }

    const QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) {
        emit captureError("grabWindow() returned null pixmap");
        return;
    }

    emitFrame(prepareOutputFrame(pixmap.toImage(), m_outputSize),
              "GrabWindow",
              pixmap.size(),
              screen->geometry());
}

void ScreenCapturer::captureWindow()
{
#ifdef Q_OS_MACOS
    QString backendName;
    const QImage rawFrame = captureWindowRaw(m_windowHandle, &backendName);
    if (rawFrame.isNull()) {
        emit captureError("无法捕获该窗口，请检查屏幕录制权限或窗口是否仍然存在");
        return;
    }

    emitFrame(prepareOutputFrame(rawFrame, m_outputSize),
              backendName,
              rawFrame.size());
#else
    emit captureError("Window capture is only implemented on macOS");
#endif
}

void ScreenCapturer::emitFrame(const QImage &frame, const QString &backendName,
                              const QSize &sourceSize, const QRect &sourceGeometry)
{
    if (frame.isNull()) {
        emit captureError("Captured frame is empty");
        return;
    }

    ++m_frameIndex;
    emit frameCaptured(frame);

    CaptureFrameMetadata meta;
    meta.sourceSize = sourceSize;
    meta.sourceGeometry = sourceGeometry;
    meta.windowHandle = (m_captureMode == CaptureMode::Window) ? m_windowHandle : 0;
    meta.backendName = backendName;
    meta.frameIndex = m_frameIndex;
    emit frameMetadataChanged(meta);
}
