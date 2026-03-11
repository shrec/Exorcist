#ifdef EXORCIST_HAS_ULTRALIGHT

#include "ultralightengine.h"
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Config.h>
#include <Ultralight/platform/FontLoader.h>
#include <Ultralight/platform/FileSystem.h>
#include <Ultralight/platform/Logger.h>
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

    namespace ul = ultralight;

    // Configure the renderer
    ul::Config config;
    config.resource_path_prefix = ul::String16(
        reinterpret_cast<const ul::Char16 *>(resourcePath.utf16()),
        resourcePath.size());

    // Cache path: use app-local temp
    const QString cachePath = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation) + QStringLiteral("/ultralight");
    QDir().mkpath(cachePath);
    config.cache_path = ul::String16(
        reinterpret_cast<const ul::Char16 *>(cachePath.utf16()),
        cachePath.size());

    // CPU rendering — no GPU
    config.force_repaint = false;
    config.animation_timer_delay = 1.0 / 60.0; // 60fps

    // Default font
    config.font_family_standard = "Segoe UI";
    config.font_family_fixed = "Cascadia Code";

    ul::Platform &platform = ul::Platform::instance();
    platform.set_config(config);

    // Use platform default font loader & file system if available.
    // Ultralight 1.4 bundles default implementations.
    platform.set_font_loader(ul::GetPlatformFontLoader());
    platform.set_file_system(ul::GetPlatformFileSystem(
        ul::String16(reinterpret_cast<const ul::Char16 *>(resourcePath.utf16()),
                     resourcePath.size())));

    m_renderer = ul::Renderer::Create();
    m_initialized = true;
}

void UltralightEngine::shutdown()
{
    if (!m_initialized)
        return;
    m_renderer = nullptr;
    m_initialized = false;
}

} // namespace exorcist

#endif // EXORCIST_HAS_ULTRALIGHT
