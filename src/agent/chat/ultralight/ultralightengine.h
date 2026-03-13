#pragma once
// ── UltralightEngine ─────────────────────────────────────────────────────────
//
// Process-wide singleton managing the shared Ultralight Renderer (C API).
// Ultralight requires exactly one Renderer per process; all Views share it.
//
// Call initialize() once from main thread before creating any UltralightWidget.
// The engine uses CPU rendering (BitmapSurface) — no GPU/OpenGL dependency.
//
// Uses the Ultralight C API to avoid ABI/mangling issues with llvm-mingw.

#ifdef EXORCIST_HAS_ULTRALIGHT

#include <cstdint>
#include <Ultralight/CAPI.h>
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
    ULRenderer renderer() const { return m_renderer; }

    bool isInitialized() const { return m_initialized; }

private:
    UltralightEngine() = default;
    ~UltralightEngine();
    UltralightEngine(const UltralightEngine &) = delete;
    UltralightEngine &operator=(const UltralightEngine &) = delete;

    ULRenderer m_renderer = nullptr;
    bool m_initialized = false;
};

} // namespace exorcist

#endif // EXORCIST_HAS_ULTRALIGHT
