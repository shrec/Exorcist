#include "largefileloader.h"

#include <QFile>
#include <QFileInfo>

#include "editorview.h"

namespace {
constexpr const char *kPropLoadedBytes = "loadedBytes";
constexpr const char *kPropTotalBytes = "totalBytes";
}

LargeFileResult LargeFileLoader::loadPreview(const QString &path, qint64 maxBytes)
{
    LargeFileResult result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return result;
    }

    const QFileInfo info(path);
    const qint64 size = info.size();
    const qint64 toRead = qMin(size, maxBytes);
    result.previewText = QString::fromUtf8(file.read(toRead));
    result.isPartial = (size > maxBytes);
    return result;
}

void LargeFileLoader::applyToEditor(EditorView *editor, const QString &path, qint64 maxBytes)
{
    const LargeFileResult result = loadPreview(path, maxBytes);
    editor->setLargeFilePreview(result.previewText, result.isPartial);
    editor->setProperty("filePath", path);
    editor->setProperty("isPartial", result.isPartial);
    const qint64 totalBytes = QFileInfo(path).size();
    editor->setProperty(kPropLoadedBytes, result.previewText.toUtf8().size());
    editor->setProperty(kPropTotalBytes, totalBytes);
}

void LargeFileLoader::appendNextChunk(EditorView *editor, qint64 chunkBytes)
{
    const QString path = editor->property("filePath").toString();
    if (path.isEmpty()) {
        return;
    }

    const qint64 loaded = editor->property(kPropLoadedBytes).toLongLong();
    const qint64 total = editor->property(kPropTotalBytes).toLongLong();
    if (total == 0 || loaded >= total) {
        editor->setProperty("isPartial", false);
        editor->appendLargeFileChunk(QString(), true);
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    if (!file.seek(loaded)) {
        return;
    }

    const qint64 toRead = qMin(chunkBytes, total - loaded);
    const QByteArray bytes = file.read(toRead);
    const QString text = QString::fromUtf8(bytes);
    const qint64 newLoaded = loaded + bytes.size();
    editor->setProperty(kPropLoadedBytes, newLoaded);

    const bool isFinal = newLoaded >= total;
    editor->setProperty("isPartial", !isFinal);
    editor->appendLargeFileChunk(text, isFinal);
}
