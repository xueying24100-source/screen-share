// 1. 调用 SCShareableContent 获取系统可共享来源
// 2. 把 SCDisplay / SCWindow 转成 ScreenCaptureSourceInfo
// 3. 前端选择 id + type
// 4. startCapture 创建 SCContentFilter 和 SCStream
// 5. ScreenCaptureKit 持续输出 CMSampleBuffer
// 6. 视频转成 QImage，通过 frameCaptured 发给 Qt
// 7. 音频转成 QByteArray，通过 audioDataCaptured 发给 Qt
// 8. stopCapture 停止流并释放资源

#import "screen_share/ScreenCaptureManager.h"
#import <AudioToolbox/AudioToolbox.h>//处理音频数据
#import <CoreGraphics/CoreGraphics.h>//获取显示器信息
#import <CoreMedia/CoreMedia.h>//处理视频帧和音频数据
#import <CoreVideo/CoreVideo.h>//处理视频帧数据
#import <ScreenCaptureKit/ScreenCaptureKit.h>//使用ScreenCaptureKit进行屏幕捕获

#include <QCoreApplication>//获取当前进程ID
#include <QDebug>
#include <QHash>
#include <QMetaObject>
#include <QMetaType>
#include <QtGlobal>
#include <algorithm>

//定义捕获上下文结构体，用于存储屏幕捕获会话的相关信息
struct CaptureContext {
    quint32 sourceId = 0;//qt的quint32类型用于存储源ID
    ScreenCaptureSourceInfo::SourceType sourceType = ScreenCaptureSourceInfo::SourceType::Display;//源类型，默认为显示器
    SCStream *stream = nil;//ScreenCaptureKit的流对象，用于管理屏幕捕获会话
    id outputBridge = nil;//桥接对象，负责将ScreenCaptureKit的回调转换为Qt信号
    bool audioOutputAdded = false;//标志，指示是否已添加音频输出到流中
};

class ScreenCaptureManager::Impl {
public:
    QHash<quint64, CaptureContext *> contexts;//当前正在采集的所有来源
    QHash<quint32, SCDisplay *> displays;//缓存的屏幕对象
    QHash<quint32, SCWindow *> windows;//缓存的窗口对象
    SCShareableContent *shareableContent = nil;//当前可共享内容的快照
    SCRunningApplication *currentApplication = nil;//当前运行的应用程序对象
    bool includeCurrentApplicationContent = false;//标志，指示是否在捕获中包含当前应用程序的内容
    CaptureResolutionPreset resolutionPreset = CaptureResolutionPreset::Native;//捕获分辨率预设，默认为原生分辨率
    bool capturesAudio = false;//标志，指示是否捕获音频
    dispatch_queue_t sampleQueue = //ScreenCaptureKit 输出视频/音频 sample 的队列
        dispatch_queue_create("com.yuzhuo.screen-share.sample-queue", DISPATCH_QUEUE_SERIAL);//ScreenCaptureKit 输出视频/音频 sample 的队列
};

//生成采集 64位key
static quint64 captureKey(quint32 sourceId, ScreenCaptureSourceInfo::SourceType sourceType) {
    return (static_cast<quint64>(static_cast<int>(sourceType)) << 32) | sourceId;
}

//把 macOS 视频帧转成 QImage
//ScreenCaptureKit 给出来的视频帧是 CVPixelBufferRef，Qt 前端更适合用 QImage，所以需要转换
static QImage qImageFromPixelBuffer(CVPixelBufferRef pixelBuffer) {
    if (!pixelBuffer) {
        return QImage();
    }

    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);//锁住 pixel buffer，保证读取期间内存稳定
    //获取像素内存地址、宽、高、每行字节数、像素格式
    void *baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
    const int width = static_cast<int>(CVPixelBufferGetWidth(pixelBuffer));
    const int height = static_cast<int>(CVPixelBufferGetHeight(pixelBuffer));
    const int bytesPerRow = static_cast<int>(CVPixelBufferGetBytesPerRow(pixelBuffer));
    const OSType pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);

    QImage frame;
    //如果格式是 kCVPixelFormatType_32BGRA，就构造 QImage,否则创建一个黑色的空白图像
    if (baseAddress && pixelFormat == kCVPixelFormatType_32BGRA) {
        //调用 .copy() 复制一份数据
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

    //解锁 pixel buffer，允许其他线程访问内存
    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    return frame;
}

//根据原始来源尺寸和分辨率预设，计算最终采集输出分辨率
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

//获取显示器的像素尺寸
static QSize displayPixelSize(SCDisplay *display) {
    if (!display) {
        return QSize();
    }

    return QSize(static_cast<int>(CGDisplayPixelsWide(display.displayID)),//获取显示器像素宽度
                 static_cast<int>(CGDisplayPixelsHigh(display.displayID)));
}

//获取窗口的尺寸
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

//判断视频帧是否完整
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

//从音频样本缓冲区中提取音频数据
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

//根据 SCDisplay 对象构造 ScreenCaptureSourceInfo 结构体,把系统对象转成前端容易使用的数据结构
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

//根据 SCWindow 对象构造 ScreenCaptureSourceInfo 结构体,把系统对象转成前端容易使用的数据结构
//底层兼容窗口来源，方便统一接口和联调
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

//同步获取系统可共享内容的快照，底层调用了 ScreenCaptureKit 的异步接口，但通过信号量实现了同步等待，方便上层逻辑编写
static SCShareableContent *copyShareableContentSync(NSError **outError) {
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);//信号量，用于同步等待
    //__block 表示这个变量可以在 Objective-C block 回调内部被修改。
    __block SCShareableContent *capturedContent = nil;//保存异步回调返回的可共享内容
    __block NSError *capturedError = nil;

    //Objective-C 的方法调用
    //向 ScreenCaptureKit 请求当前可共享内容，等系统完成后执行 completionHandler 里的 block
    [SCShareableContent getShareableContentExcludingDesktopWindows:YES
                                               onScreenWindowsOnly:YES
                                                 completionHandler:^(SCShareableContent * _Nullable shareableContent,
                                                                     NSError * _Nullable error) {
        capturedContent = [shareableContent retain];
        capturedError = [error retain];
        dispatch_semaphore_signal(semaphore);//signal 信号量，通知等待的线程可以继续执行了
    }];

    //wait 等待 semaphore 有信号。
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

    if (outError) {
        *outError = capturedError;
    } else if (capturedError) {
        [capturedError release];
    }
    return capturedContent;
}

//ScreenCaptureKit 的回调桥接类，负责将 SCStreamDelegate 和 SCStreamOutput 的回调转换为 Qt 信号，方便上层逻辑处理
@interface ScreenCaptureStreamBridge : NSObject <SCStreamDelegate, SCStreamOutput>
- (instancetype)initWithManager:(ScreenCaptureManager *)manager
                       sourceId:(quint32)sourceId
                     sourceType:(ScreenCaptureSourceInfo::SourceType)sourceType;
@end

//实现 ScreenCaptureStreamBridge 类，处理屏幕捕获的回调事件
@implementation ScreenCaptureStreamBridge {
    ScreenCaptureManager *_manager;
    quint32 _sourceId;
    ScreenCaptureSourceInfo::SourceType _sourceType;
}

//初始化方法，保存管理器引用、源ID和源类型，方便后续回调中使用
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

//接收视频/音频数据
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

//采集异常停止
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
    //同步获取共享内容，如果失败了就返回错误信息并退出函数
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

    //判断之前是否已经缓存过旧的 shareableContent
    if (d->shareableContent) {
        [d->shareableContent release];
    }
    d->shareableContent = content;
    d->displays.clear();
    d->windows.clear();
    d->currentApplication = nil;

    //遍历 content.displays 里的每个 SCDisplay。SCDisplay 表示一个可共享屏幕
    for (SCDisplay *display in content.displays) {
        d->displays.insert(static_cast<quint32>(display.displayID), display);
    }

    const pid_t currentPid = QCoreApplication::applicationPid();//获取当前 Qt 应用进程的 pid，用于判断哪个 SCRunningApplication 是当前 app
    //遍历系统返回的所有相关应用
    for (SCRunningApplication *application in content.applications) {
        if (application.processID == currentPid) {
            d->currentApplication = application;
            break;
        }
    }
    //遍历系统返回的窗口列表
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

//核心接口：枚举屏幕来源
QVector<ScreenCaptureSourceInfo> ScreenCaptureManager::enumerateDisplays() {
    QVector<ScreenCaptureSourceInfo> displays;
    QString errorMessage;

    //枚举屏幕来源，需要刷新共享内容，如果失败了就发出错误信号并返回空列表
    if (!refreshShareableContent(&errorMessage)) {
        emit captureError(0,
                          ScreenCaptureSourceInfo::SourceType::Display,
                          errorMessage.isEmpty()
                              ? QStringLiteral("Unable to enumerate displays")
                              : errorMessage);
        return displays;
    }

    //
    for (SCDisplay *display in d->shareableContent.displays) {
        displays.append(makeDisplayInfo(display));
    }

    //按照 id 从小到大排列屏幕
    std::sort(displays.begin(),
              displays.end(),
              [](const ScreenCaptureSourceInfo &lhs, const ScreenCaptureSourceInfo &rhs) {
        return lhs.id < rhs.id;
    });

    emit displaysEnumerated(displays);//发出 Qt 信号 displaysEnumerated。表示屏幕枚举完成了，这是枚举结果。
    return displays;
}

//核心接口：枚举窗口来源
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

//核心接口：启动采集
bool ScreenCaptureManager::startCapture(quint32 sourceId,
                                        ScreenCaptureSourceInfo::SourceType sourceType) {
    const quint64 key = captureKey(sourceId, sourceType);
    if (d->contexts.contains(key)) {
        return true;
    }

    QString errorMessage;
    //刷新共享内容，如果失败了就发出错误信号并返回 false
    if (!refreshShareableContent(&errorMessage)) {
        //emit 是 Qt 的信号发送关键字，作用是发出一个Qt signal，通知外部：这里发生了某个事件
        //发出错误信号，表示刷新共享内容失败了
        emit captureError(sourceId,
                          sourceType,
                          errorMessage.isEmpty()
                              ? QStringLiteral("Unable to refresh shareable content")
                              : errorMessage);
        return false;
    }

    //SCContentFilter 是 ScreenCaptureKit 的采集目标过滤器，用来指定采集哪个屏幕或窗口，以及排除哪些应用/窗口。它负责“采集什么”
    //SCStreamConfiguration 负责“怎么采集”
    //SCStream 负责“真正开始采集”。
    SCContentFilter *filter = nil;//采集过滤器：要采集哪个屏幕或哪个窗口，以及要不要排除某些应用
    QSize outputSize;//输出分辨率：根据源的尺寸和用户选择的预设计算出来的最终采集分辨率

    if (sourceType == ScreenCaptureSourceInfo::SourceType::Display) {//如果是屏幕来源，就创建一个针对屏幕的过滤器
        SCDisplay *display = d->displays.value(sourceId, nil);//根据屏幕 id 查找对应的 SCDisplay *对象
        if (!display) {//如果找不到对应的屏幕对象，说明用户选的屏幕不存在了，就发出错误信号并返回 false
            emit captureError(sourceId, sourceType, QStringLiteral("Selected display no longer exists"));
            return false;
        }

        //要排除的应用列表 = 当前 app
        NSArray<SCRunningApplication *> *excludedApps = //定义一个数组，里面存放要从屏幕采集中排除的应用
            (!d->includeCurrentApplicationContent //不包含当前应用内容
            && d->currentApplication) //d->currentApplication 是当前 app 对应的 SCRunningApplication *。
                                    //是在 refreshShareableContent() 里通过当前进程 pid 找出来的
                                    //如果不是空，说明已经知道当前 app 是哪个系统应用对象
            ? @[ d->currentApplication ]
            : @[];
        //[SCContentFilter alloc]创建一块内存，然后[]初始化
        filter = [[SCContentFilter alloc] initWithDisplay:display
                                    excludingApplications:excludedApps//排除当前应用，防止递归
                                         exceptingWindows:@[]];//排除的窗口列表，这里不排除任何窗口
        outputSize = resolvedOutputSize(displayPixelSize(display), d->resolutionPreset);//根据屏幕的像素尺寸和用户选择的预设计算输出分辨率
    } else {//如果是窗口来源，就创建一个针对窗口的过滤器
        SCWindow *window = d->windows.value(sourceId, nil);
        if (!window) {
            emit captureError(sourceId, sourceType, QStringLiteral("Selected window no longer exists"));
            return false;
        }

        filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:window];//初始化一个针对窗口的采集过滤器
        outputSize = resolvedOutputSize(windowLogicalSize(window), d->resolutionPreset);
    }

    if (!filter || !outputSize.isValid()) {
        if (filter) {
            [filter release];//释放当前代码对 filter 这个对象的持有权
        }
        emit captureError(sourceId, sourceType, QStringLiteral("Invalid content filter or output size"));
        return false;
    }

    //SCStreamConfiguration 是 ScreenCaptureKit 里的采集配置类
    SCStreamConfiguration *configuration = [[SCStreamConfiguration alloc] init];
    configuration.width = static_cast<size_t>(outputSize.width());
    configuration.height = static_cast<size_t>(outputSize.height());
    configuration.minimumFrameInterval = CMTimeMake(1, 30);//设置最小帧间隔，30fps
    configuration.queueDepth = 3;//系统最多缓存 3 帧还没处理的视频帧
    configuration.pixelFormat = kCVPixelFormatType_32BGRA;//设置输出视频帧的像素格式，Qt 前端更适合处理 BGRA 格式的帧
    configuration.showsCursor = YES;//设置是否在捕获的视频帧中显示鼠标光标
    configuration.scalesToFit = YES;//设置是否缩放捕获内容以适应输出分辨率
    if (d->capturesAudio) {
        configuration.capturesAudio = YES;
        configuration.sampleRate = 48000;//音频采样率，48000Hz
        configuration.channelCount = 2;//双声道
        configuration.excludesCurrentProcessAudio = !d->includeCurrentApplicationContent;//是否排除当前应用的音频
    }

    //ScreenCaptureStreamBridge 是 ScreenCaptureKit 采集结果的接收者，负责把采集结果转换成 Qt 前端能处理的格式
    ScreenCaptureStreamBridge *bridge =
        [[ScreenCaptureStreamBridge alloc] initWithManager:this
                                                  sourceId:sourceId
                                                sourceType:sourceType];
    SCStream *stream = [[SCStream alloc] initWithFilter:filter
                                          configuration:configuration
                                               delegate:bridge];//初始化一个采集流，并把 ScreenCaptureStreamBridge 作为它的代理
    [configuration release];
    [filter release];

    if (!stream) {
        [bridge release];
        emit captureError(sourceId, sourceType, QStringLiteral("Failed to create ScreenCaptureKit stream"));
        return false;
    }

    //把ScreenCaptureStreamBridge 注册为 SCStream 的视频帧输出接收者
    //后续系统采集到的屏幕帧会通过 didOutputSampleBuffer 回调到这个 bridge
    NSError *addOutputError = nil; //接收添加采集输出时可能出现的错误信息
    if (![stream addStreamOutput:bridge//添加一个采集输出，这里用的是 ScreenCaptureStreamBridge 作为输出的接收者
                            type:SCStreamOutputTypeScreen//输出类型是视频帧
              sampleHandlerQueue:d->sampleQueue//指定输出回调在哪个队列执行
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

//核心接口：停止采集
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
