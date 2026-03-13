#ifdef EXORCIST_HAS_ULTRALIGHT

#include "ultralightengine.h"
#include <AppCore/CAPI.h>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace exorcist {

UltralightEngine &UltralightEngine::instance()
{
    static UltralightEngine s;
    return s;
}

UltralightEngine::~UltralightEngine()
{
    shutdown();
}

void UltralightEngine::initialize(const QString &resourcePath)
{
    if (m_initialized)
        return;

    // Configure the renderer via C API
    ULConfig config = ulCreateConfig();

    // Resource path prefix
    const QByteArray resPathUtf8 = resourcePath.toUtf8();
    ULString ulResPath = ulCreateStringUTF8(resPathUtf8.constData(),
                                            resPathUtf8.size());
    ulConfigSetResourcePathPrefix(config, ulResPath);

    // Cache path: use app-local temp
    const QString cachePath = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation) + QStringLiteral("/ultralight");
    QDir().mkpath(cachePath);
    const QByteArray cachePathUtf8 = cachePath.toUtf8();
    ULString ulCachePath = ulCreateStringUTF8(cachePathUtf8.constData(),
                                              cachePathUtf8.size());
    ulConfigSetCachePath(config, ulCachePath);

    // CPU rendering — no GPU
    ulConfigSetForceRepaint(config, false);
    ulConfigSetAnimationTimerDelay(config, 1.0 / 60.0); // 60fps

    // Use AppCore default platform providers
    ulEnablePlatformFontLoader();
    ulEnablePlatformFileSystem(ulResPath);

    m_renderer = ulCreateRenderer(config);
    m_initialized = true;

    ulDestroyString(ulResPath);
    ulDestroyString(ulCachePath);
    ulDestroyConfig(config);
}

void UltralightEngine::shutdown()
{
    if (!m_initialized)
        return;
    ulDestroyRenderer(m_renderer);
    m_renderer = nullptr;
    m_initialized = false;
}

} // namespace exorcist

#endif // EXORCIST_HAS_ULTRALIGHT
