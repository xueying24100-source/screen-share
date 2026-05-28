#import "screen_share/ScreenCaptureManager.h"
#import <AudioToolbox/AudioToolbox.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <QCoreApplication>
#include <QDebug>
#include <QHash>
#include <QMetaObject>
#include <QMetaType>
#include <QtGlobal>
#include <algorithm>

struct CaptureContext {
    quint32 sourceId = 0;
    ScreenCaptureSourceInfo::SourceType sourceType = ScreenCaptureSourceInfo::SourceType::Display;
    SCStream *stream = nil;
    id outputBridge = nil;
    bool audioOutputAdded = false;
};

class ScreenCaptureManager::Impl {
public:
    QHash<quint64, CaptureContext *> contexts;
    QHash<quint32, SCDisplay *> displays;
    QHash<quint32, SCWindow *> windows;
    SCShareableContent *shareableContent = nil;
    SCRunningApplication *currentApplication = nil;
    bool includeCurrentApplicationContent = false;
    CaptureResolutionPreset resolutionPreset = CaptureResolutionPreset::Native;
    bool capturesAudio = false;
    dispatch_queue_t sampleQueue =
        dispatch_queue_create("com.yuzhuo.screen-share.sample-queue", DISPATCH_QUEUE_SERIAL);
};

static quint64 captureKey(quint32 sourceId, ScreenCaptureSourceInfo::SourceType sourceType) {
    return (static_cast<quint64>(static_cast<int>(sourceType)) << 32) | sourceId;
}

static QImage qImageFromPixelBuffer(CVPixelBufferRef pixelBuffer) {
    if (!pixelBuffer) {
        return QImage();
    }

    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    void *baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
    const int width = static_cast<int>(CVPixelBufferGetWidth(pixelBuffer));
    const int height = static_cast<int>(CVPixelBufferGetHeight(pixelBuffer));
    const int bytesPerRow = static_cast<int>(CVPixelBufferGetBytesPerRow(pixelBuffer));
    const OSType pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);

    QImage frame;
    if (baseAddress && pixelFormat == kCVPixelFormatType_32BGRA) {
        frame = QImage(static_cast<uchar *>(baseAddress),
                       width,
                       height,
                       bytesPerRow,
                       QImage::Format_ARGB32)
                    .copy();
    } else {
        frame = QImage(width, height, QImage::Format_ARGB32);
        frame.fill(Qt::black);
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    return frame;
}

static QSize resolvedOutputSize(const QSize &sourceSize, CaptureResolutionPreset preset) {
    if (!sourceSize.isValid()) {
        return QSize(1, 1);
    }

    switch (preset) {
    case CaptureResolutionPreset::Native:
        return sourceSize;
    case CaptureResolutionPreset::Half:
        return QSize(std::max(1, sourceSize.width() / 2),
                     std::max(1, sourceSize.height() / 2));
    case CaptureResolutionPreset::HD720:
        return sourceSize.scaled(1280, 720, Qt::KeepAspectRatio);
    case CaptureResolutionPreset::HD1080:
        return sourceSize.scaled(1920, 1080, Qt::KeepAspectRatio);
    }

    return sourceSize;
}

static QSize displayPixelSize(SCDisplay *display) {
    if (!display) {
        return QSize();
    }

    return QSize(static_cast<int>(CGDisplayPixelsWide(display.displayID)),
                 static_cast<int>(CGDisplayPixelsHigh(display.displayID)));
}

static QSize windowLogicalSize(SCWindow *window) {
    if (!window) {
        return QSize();
    }

    return QSize(static_cast<int>(std::max<NSInteger>(
                     1,
                     static_cast<NSInteger>(std::lround(window.frame.size.width)))),
                 static_cast<int>(std::max<NSInteger>(
                     1,
                     static_cast<NSInteger>(std::lround(window.frame.size.height)))));
}

static bool isFrameSampleComplete(CMSampleBufferRef sampleBuffer) {
    if (!sampleBuffer) {
        return false;
    }

    CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
    if (!attachmentsArray || CFArrayGetCount(attachmentsArray) == 0) {
        return true;
    }

    CFDictionaryRef attachment =
        static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, 0));
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

static QByteArray audioDataFromSampleBuffer(CMSampleBufferRef sampleBuffer) {
    if (!sampleBuffer) {
        return {};
    }

    CMBlockBufferRef blockBuffer = nullptr;
    constexpr size_t maxAudioBuffers = 8;
    alignas(AudioBufferList)
    char audioBufferListStorage[sizeof(AudioBufferList) +
                                sizeof(AudioBuffer) * (maxAudioBuffers - 1)] = {};
    auto *audioBufferList = reinterpret_cast<AudioBufferList *>(audioBufferListStorage);
    size_t neededOutSize = 0;
    OSStatus status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer,
        &neededOutSize,
        audioBufferList,
        sizeof(audioBufferListStorage),
        nullptr,
        nullptr,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment,
        &blockBuffer);
    if (status != noErr) {
        return {};
    }

    QByteArray audioData;
    for (UInt32 i = 0; i < audioBufferList->mNumberBuffers; ++i) {
        const AudioBuffer &buffer = audioBufferList->mBuffers[i];
        if (buffer.mData && buffer.mDataByteSize > 0) {
            audioData.append(static_cast<const char *>(buffer.mData),
                             static_cast<int>(buffer.mDataByteSize));
        }
    }

    if (blockBuffer) {
        CFRelease(blockBuffer);
    }
    return audioData;
}

static ScreenCaptureSourceInfo makeDisplayInfo(SCDisplay *display) {
    ScreenCaptureSourceInfo info;
    info.id = static_cast<quint32>(display.displayID);
    info.type = ScreenCaptureSourceInfo::SourceType::Display;
    info.name = QStringLiteral("Display %1").arg(info.id);
    info.size = displayPixelSize(display);
    info.scale = display.width > 0
        ? static_cast<float>(info.size.width()) / static_cast<float>(display.width)
        : 1.0f;
    return info;
}

static bool makeWindowInfo(SCWindow *window,
                           pid_t currentPid,
                           bool includeCurrentApplicationContent,
                           ScreenCaptureSourceInfo *info) {
    if (!window || !info) {
        return false;
    }
    if (window.windowLayer != 0) {
        return false;
    }
    if (!window.isOnScreen) {
        return false;
    }

    SCRunningApplication *owner = window.owningApplication;
    if (!owner || (!includeCurrentApplicationContent && owner.processID == currentPid)) {
        return false;
    }

    if (window.frame.size.width < 1.0 || window.frame.size.height < 1.0) {
        return false;
    }

    info->id = static_cast<quint32>(window.windowID);
    info->type = ScreenCaptureSourceInfo::SourceType::Window;
    info->name = QString::fromNSString(owner.applicationName ?: @"");
    const QString title = QString::fromNSString(window.title ?: @"");
    if (!title.trimmed().isEmpty()) {
        info->name += QStringLiteral(" - %1").arg(title);
    }
    if (info->name.trimmed().isEmpty()) {
        info->name = QStringLiteral("Window %1").arg(info->id);
    }
    info->size = windowLogicalSize(window);
    info->scale = 1.0f;
    return true;
}

static SCShareableContent *copyShareableContentSync(NSError **outError) {
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    __block SCShareableContent *capturedContent = nil;
    __block NSError *capturedError = nil;

    [SCShareableContent getShareableContentExcludingDesktopWindows:YES
                                               onScreenWindowsOnly:YES
                                                 completionHandler:^(SCShareableContent * _Nullable shareableContent,
                                                                     NSError * _Nullable error) {
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

@interface ScreenCaptureStreamBridge : NSObject <SCStreamDelegate, SCStreamOutput>
- (instancetype)initWithManager:(ScreenCaptureManager *)manager
                       sourceId:(quint32)sourceId
                     sourceType:(ScreenCaptureSourceInfo::SourceType)sourceType;
@end

@implementation ScreenCaptureStreamBridge {
    ScreenCaptureManager *_manager;
    quint32 _sourceId;
    ScreenCaptureSourceInfo::SourceType _sourceType;
}

- (instancetype)initWithManager:(ScreenCaptureManager *)manager
                       sourceId:(quint32)sourceId
                     sourceType:(ScreenCaptureSourceInfo::SourceType)sourceType {
    self = [super init];
    if (self) {
        _manager = manager;
        _sourceId = sourceId;
        _sourceType = sourceType;
    }
    return self;
}

- (void)stream:(SCStream *)stream
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       ofType:(SCStreamOutputType)type {
    Q_UNUSED(stream);
    ScreenCaptureManager *manager = _manager;
    const quint32 sourceId = _sourceId;
    const auto sourceType = _sourceType;
    if (type == SCStreamOutputTypeScreen) {
        if (!isFrameSampleComplete(sampleBuffer)) {
            return;
        }

        CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
        QImage image = qImageFromPixelBuffer(pixelBuffer);
        if (image.isNull()) {
            return;
        }

        QMetaObject::invokeMethod(manager,
                                  [manager, sourceId, sourceType, image]() {
            emit manager->frameCaptured(sourceId, sourceType, image);
        },
                                  Qt::QueuedConnection);
        return;
    }

    if (type == SCStreamOutputTypeAudio) {
        const QByteArray audioData = audioDataFromSampleBuffer(sampleBuffer);
        if (audioData.isEmpty()) {
            return;
        }

        int sampleRate = 48000;
        int channelCount = 2;
        CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer);
        if (formatDescription) {
            const AudioStreamBasicDescription *streamDescription =
                CMAudioFormatDescriptionGetStreamBasicDescription(
                    static_cast<CMAudioFormatDescriptionRef>(formatDescription));
            if (streamDescription) {
                if (streamDescription->mSampleRate > 0) {
                    sampleRate = static_cast<int>(streamDescription->mSampleRate);
                }
                if (streamDescription->mChannelsPerFrame > 0) {
                    channelCount = static_cast<int>(streamDescription->mChannelsPerFrame);
                }
            }
        }

        QMetaObject::invokeMethod(manager,
                                  [manager, sourceId, sourceType, audioData, sampleRate, channelCount]() {
            emit manager->audioDataCaptured(sourceId,
                                            sourceType,
                                            audioData,
                                            sampleRate,
                                            channelCount);
        },
                                  Qt::QueuedConnection);
    }
}

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error {
    Q_UNUSED(stream);
    ScreenCaptureManager *manager = _manager;
    const quint32 sourceId = _sourceId;
    const auto sourceType = _sourceType;
    const QString message = error ? QString::fromNSString(error.localizedDescription)
                                  : QStringLiteral("ScreenCaptureKit stream stopped");

    QMetaObject::invokeMethod(manager,
                              [manager, sourceId, sourceType, message]() {
        emit manager->captureError(sourceId, sourceType, message);
        manager->stopCapture(sourceId, sourceType);
    },
                              Qt::QueuedConnection);
}

@end

ScreenCaptureManager::ScreenCaptureManager(QObject *parent)
    : QObject(parent), d(new Impl) {
    qRegisterMetaType<ScreenCaptureSourceInfo>("ScreenCaptureSourceInfo");
    qRegisterMetaType<QVector<ScreenCaptureSourceInfo>>("QVector<ScreenCaptureSourceInfo>");
}

ScreenCaptureManager::~ScreenCaptureManager() {
    stopAllCaptures();
    if (d->shareableContent) {
        [d->shareableContent release];
        d->shareableContent = nil;
    }
    delete d;
}

void ScreenCaptureManager::setIncludeCurrentApplicationContent(bool include) {
    d->includeCurrentApplicationContent = include;
}

bool ScreenCaptureManager::includeCurrentApplicationContent() const {
    return d->includeCurrentApplicationContent;
}

void ScreenCaptureManager::setResolutionPreset(CaptureResolutionPreset preset) {
    d->resolutionPreset = preset;
}

CaptureResolutionPreset ScreenCaptureManager::resolutionPreset() const {
    return d->resolutionPreset;
}

void ScreenCaptureManager::setCapturesAudio(bool enabled) {
    d->capturesAudio = enabled;
}

bool ScreenCaptureManager::capturesAudio() const {
    return d->capturesAudio;
}

bool ScreenCaptureManager::refreshShareableContent(QString *errorMessage) {
    NSError *error = nil;
    SCShareableContent *content = copyShareableContentSync(&error);
    if (!content) {
        if (errorMessage) {
            *errorMessage = error
                ? QString::fromNSString(error.localizedDescription)
                : QStringLiteral("Unable to fetch shareable content");
        }
        if (error) {
            [error release];
        }
        return false;
    }

    if (d->shareableContent) {
        [d->shareableContent release];
    }
    d->shareableContent = content;
    d->displays.clear();
    d->windows.clear();
    d->currentApplication = nil;

    for (SCDisplay *display in content.displays) {
        d->displays.insert(static_cast<quint32>(display.displayID), display);
    }

    const pid_t currentPid = QCoreApplication::applicationPid();
    for (SCRunningApplication *application in content.applications) {
        if (application.processID == currentPid) {
            d->currentApplication = application;
            break;
        }
    }

    for (SCWindow *window in content.windows) {
        ScreenCaptureSourceInfo info;
        if (makeWindowInfo(window, currentPid, d->includeCurrentApplicationContent, &info)) {
            d->windows.insert(info.id, window);
        }
    }

    if (error) {
        [error release];
    }
    return true;
}

QVector<ScreenCaptureSourceInfo> ScreenCaptureManager::enumerateDisplays() {
    QVector<ScreenCaptureSourceInfo> displays;
    QString errorMessage;
    if (!refreshShareableContent(&errorMessage)) {
        emit captureError(0,
                          ScreenCaptureSourceInfo::SourceType::Display,
                          errorMessage.isEmpty()
                              ? QStringLiteral("Unable to enumerate displays")
                              : errorMessage);
        return displays;
    }

    for (SCDisplay *display in d->shareableContent.displays) {
        displays.append(makeDisplayInfo(display));
    }

    std::sort(displays.begin(),
              displays.end(),
              [](const ScreenCaptureSourceInfo &lhs, const ScreenCaptureSourceInfo &rhs) {
        return lhs.id < rhs.id;
    });

    emit displaysEnumerated(displays);
    return displays;
}

QVector<ScreenCaptureSourceInfo> ScreenCaptureManager::enumerateWindows() {
    QVector<ScreenCaptureSourceInfo> windows;
    QString errorMessage;
    if (!refreshShareableContent(&errorMessage)) {
        emit captureError(0,
                          ScreenCaptureSourceInfo::SourceType::Window,
                          errorMessage.isEmpty()
                              ? QStringLiteral("Unable to enumerate windows")
                              : errorMessage);
        return windows;
    }

    const pid_t currentPid = QCoreApplication::applicationPid();
    for (SCWindow *window in d->shareableContent.windows) {
        ScreenCaptureSourceInfo info;
        if (makeWindowInfo(window, currentPid, d->includeCurrentApplicationContent, &info)) {
            windows.append(info);
        }
    }

    std::sort(windows.begin(),
              windows.end(),
              [](const ScreenCaptureSourceInfo &lhs, const ScreenCaptureSourceInfo &rhs) {
        const int nameCompare = QString::localeAwareCompare(lhs.name, rhs.name);
        if (nameCompare != 0) {
            return nameCompare < 0;
        }
        return lhs.id < rhs.id;
    });

    emit windowsEnumerated(windows);
    return windows;
}

bool ScreenCaptureManager::startCapture(quint32 sourceId,
                                        ScreenCaptureSourceInfo::SourceType sourceType) {
    const quint64 key = captureKey(sourceId, sourceType);
    if (d->contexts.contains(key)) {
        return true;
    }

    QString errorMessage;
    if (!refreshShareableContent(&errorMessage)) {
        emit captureError(sourceId,
                          sourceType,
                          errorMessage.isEmpty()
                              ? QStringLiteral("Unable to refresh shareable content")
                              : errorMessage);
        return false;
    }

    SCContentFilter *filter = nil;
    QSize outputSize;

    if (sourceType == ScreenCaptureSourceInfo::SourceType::Display) {
        SCDisplay *display = d->displays.value(sourceId, nil);
        if (!display) {
            emit captureError(sourceId, sourceType, QStringLiteral("Selected display no longer exists"));
            return false;
        }

        NSArray<SCRunningApplication *> *excludedApps =
            (!d->includeCurrentApplicationContent && d->currentApplication)
            ? @[ d->currentApplication ]
            : @[];
        filter = [[SCContentFilter alloc] initWithDisplay:display
                                    excludingApplications:excludedApps
                                         exceptingWindows:@[]];
        outputSize = resolvedOutputSize(displayPixelSize(display), d->resolutionPreset);
    } else {
        SCWindow *window = d->windows.value(sourceId, nil);
        if (!window) {
            emit captureError(sourceId, sourceType, QStringLiteral("Selected window no longer exists"));
            return false;
        }

        filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:window];
        outputSize = resolvedOutputSize(windowLogicalSize(window), d->resolutionPreset);
    }

    if (!filter || !outputSize.isValid()) {
        if (filter) {
            [filter release];
        }
        emit captureError(sourceId, sourceType, QStringLiteral("Invalid content filter or output size"));
        return false;
    }

    SCStreamConfiguration *configuration = [[SCStreamConfiguration alloc] init];
    configuration.width = static_cast<size_t>(outputSize.width());
    configuration.height = static_cast<size_t>(outputSize.height());
    configuration.minimumFrameInterval = CMTimeMake(1, 30);
    configuration.queueDepth = 3;
    configuration.pixelFormat = kCVPixelFormatType_32BGRA;
    configuration.showsCursor = YES;
    configuration.scalesToFit = YES;
    if (d->capturesAudio) {
        configuration.capturesAudio = YES;
        configuration.sampleRate = 48000;
        configuration.channelCount = 2;
        configuration.excludesCurrentProcessAudio = !d->includeCurrentApplicationContent;
    }

    ScreenCaptureStreamBridge *bridge =
        [[ScreenCaptureStreamBridge alloc] initWithManager:this
                                                  sourceId:sourceId
                                                sourceType:sourceType];
    SCStream *stream = [[SCStream alloc] initWithFilter:filter
                                          configuration:configuration
                                               delegate:bridge];
    [configuration release];
    [filter release];

    if (!stream) {
        [bridge release];
        emit captureError(sourceId, sourceType, QStringLiteral("Failed to create ScreenCaptureKit stream"));
        return false;
    }

    NSError *addOutputError = nil;
    if (![stream addStreamOutput:bridge
                            type:SCStreamOutputTypeScreen
              sampleHandlerQueue:d->sampleQueue
                           error:&addOutputError]) {
        const QString message = addOutputError
            ? QString::fromNSString(addOutputError.localizedDescription)
            : QStringLiteral("Failed to add stream output");
        [stream release];
        [bridge release];
        emit captureError(sourceId, sourceType, message);
        return false;
    }

    bool audioOutputAdded = false;
    if (d->capturesAudio) {
        NSError *addAudioOutputError = nil;
        if (![stream addStreamOutput:bridge
                                type:SCStreamOutputTypeAudio
                  sampleHandlerQueue:d->sampleQueue
                               error:&addAudioOutputError]) {
            const QString message = addAudioOutputError
                ? QString::fromNSString(addAudioOutputError.localizedDescription)
                : QStringLiteral("Failed to add audio stream output");
            NSError *removeError = nil;
            [stream removeStreamOutput:bridge type:SCStreamOutputTypeScreen error:&removeError];
            Q_UNUSED(removeError);
            [stream release];
            [bridge release];
            emit captureError(sourceId, sourceType, message);
            return false;
        }
        audioOutputAdded = true;
    }

    __block NSError *startError = nil;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    [stream startCaptureWithCompletionHandler:^(NSError * _Nullable error) {
        startError = [error retain];
        dispatch_semaphore_signal(semaphore);
    }];
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

    if (startError) {
        const QString message = QString::fromNSString(startError.localizedDescription);
        [startError release];
        NSError *removeError = nil;
        [stream removeStreamOutput:bridge type:SCStreamOutputTypeScreen error:&removeError];
        Q_UNUSED(removeError);
        if (audioOutputAdded) {
            NSError *removeAudioError = nil;
            [stream removeStreamOutput:bridge type:SCStreamOutputTypeAudio error:&removeAudioError];
            Q_UNUSED(removeAudioError);
        }
        [stream release];
        [bridge release];
        emit captureError(sourceId, sourceType, message);
        return false;
    }

    auto *context = new CaptureContext;
    context->sourceId = sourceId;
    context->sourceType = sourceType;
    context->stream = stream;
    context->outputBridge = bridge;
    context->audioOutputAdded = audioOutputAdded;
    d->contexts.insert(key, context);
    return true;
}

void ScreenCaptureManager::stopCapture(quint32 sourceId,
                                       ScreenCaptureSourceInfo::SourceType sourceType) {
    const quint64 key = captureKey(sourceId, sourceType);
    if (!d->contexts.contains(key)) {
        return;
    }

    CaptureContext *context = d->contexts.take(key);

    if (context->stream) {
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        [context->stream stopCaptureWithCompletionHandler:^(NSError * _Nullable error) {
            Q_UNUSED(error);
            dispatch_semaphore_signal(semaphore);
        }];
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

        NSError *removeError = nil;
        [context->stream removeStreamOutput:context->outputBridge
                                       type:SCStreamOutputTypeScreen
                                      error:&removeError];
        Q_UNUSED(removeError);
        if (context->audioOutputAdded) {
            NSError *removeAudioError = nil;
            [context->stream removeStreamOutput:context->outputBridge
                                           type:SCStreamOutputTypeAudio
                                          error:&removeAudioError];
            Q_UNUSED(removeAudioError);
        }
        [context->stream release];
    }

    if (context->outputBridge) {
        [context->outputBridge release];
    }

    delete context;
    emit captureStopped(sourceId, sourceType);
}

void ScreenCaptureManager::stopAllCaptures() {
    const auto keys = d->contexts.keys();
    for (quint64 key : keys) {
        const auto sourceType = static_cast<ScreenCaptureSourceInfo::SourceType>(key >> 32);
        const auto sourceId = static_cast<quint32>(key & 0xffffffff);
        stopCapture(sourceId, sourceType);
    }
}
