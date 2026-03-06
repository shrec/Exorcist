#include "largefileloader.h"

#include <QFile>
#include <QFileInfo>

#include "editorview.h"

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
    editor->setPlainText(result.previewText);
    editor->setProperty("filePath", path);
    editor->setProperty("isPartial", result.isPartial);
    editor->setReadOnly(result.isPartial);
}
