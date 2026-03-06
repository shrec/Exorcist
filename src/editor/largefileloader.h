#pragma once

#include <QString>

class EditorView;

struct LargeFileResult
{
    QString previewText;
    bool isPartial = false;
};

class LargeFileLoader
{
public:
    static LargeFileResult loadPreview(const QString &path, qint64 maxBytes);
    static void applyToEditor(EditorView *editor, const QString &path, qint64 maxBytes);
    static void appendNextChunk(EditorView *editor, qint64 chunkBytes);
};
