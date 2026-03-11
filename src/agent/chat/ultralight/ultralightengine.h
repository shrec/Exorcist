#pragma once
// ── UltralightEngine ─────────────────────────────────────────────────────────
//
// Process-wide singleton managing the shared ultralight::Renderer.
// Ultralight requires exactly one Renderer per process; all Views share it.
//
// Call initialize() once from main thread before creating any UltralightWidget.
// The engine uses CPU rendering (BitmapSurface) — no GPU/OpenGL dependency.

#ifdef EXORCIST_HAS_ULTRALIGHT

#include <Ultralight/Ultralight.h>
#include <QString>

namespace exorcist {

class UltralightEngine
{
public:
    static UltralightEngine &instance();

    /// Initialize the renderer. Must be called once from the main thread.
    /// @param resourcePath  Path to ultralight-resources/ directory.
    void initialize(const QString &resourcePath);

    /// Shutdown and release the renderer.
    void shutdown();

    /// Access the shared renderer. Returns nullptr if not initialized.
    ultralight::RefPtr<ultralight::Renderer> renderer() const { return m_renderer; }

    bool isInitialized() const { return m_initialized; }

private:
    UltralightEngine() = default;
    ~UltralightEngine();
    UltralightEngine(const UltralightEngine &) = delete;
    UltralightEngine &operator=(const UltralightEngine &) = delete;

    ultralight::RefPtr<ultralight::Renderer> m_renderer;
    bool m_initialized = false;
};

} // namespace exorcist

#endif // EXORCIST_HAS_ULTRALIGHT
