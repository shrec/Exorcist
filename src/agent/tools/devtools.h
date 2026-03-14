#pragma once

#include "../itool.h"

#include <QJsonArray>
#include <QList>

#include <functional>

// ── CompileAndRunTool ────────────────────────────────────────────────────────
// Compile a C++ snippet and execute it. Wraps the compile+link+run cycle
// into a single tool call. Uses system compiler (g++/clang++/cl).

class CompileAndRunTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    static QString detectCompiler();

    QString m_workspaceRoot;
};

// ── ArchiveTool ──────────────────────────────────────────────────────────────
// Create and extract zip/tar.gz archives using system tools.

class ArchiveTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    ToolExecResult listArchive(const QString &path, const QString &fmt);
    ToolExecResult extractArchive(const QString &path, const QString &fmt,
                                  const QString &outDir);
    ToolExecResult createArchive(const QString &path, const QString &fmt,
                                 const QJsonArray &files);
    static QString detectFormat(const QString &path, const QString &hint);
    QString resolvePath(const QString &path) const;

    QString m_workspaceRoot;
};

// ── CreatePatchTool ──────────────────────────────────────────────────────────
// Generate a unified diff / patch file from file changes.

class CreatePatchTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    ToolExecResult diffFiles(const QJsonObject &args);
    ToolExecResult diffContent(const QJsonObject &args);
    ToolExecResult gitPatch(const QJsonObject &args);
    QString resolvePath(const QString &path) const;

    QString m_workspaceRoot;
};

// ── ImageInfoTool ────────────────────────────────────────────────────────────
// Get image metadata and basic analysis.

class ImageInfoTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString resolvePath(const QString &path) const;

    QString m_workspaceRoot;
};

// ── GenerateDiagramTool ──────────────────────────────────────────────────────
// Generate diagrams from Mermaid or PlantUML markup.
// Uses callback to the IDE's diagram renderer (or falls back to CLI tools).

class GenerateDiagramTool : public ITool
{
public:
    struct DiagramResult {
        bool    ok;
        QString svgContent;   // SVG output
        QString pngPath;      // Path to generated PNG (if applicable)
        QString error;
    };

    // renderer(markup, format, outputPath) → DiagramResult
    //   format: "mermaid" or "plantuml"
    using DiagramRenderer = std::function<DiagramResult(
        const QString &markup, const QString &format, const QString &outputPath)>;

    explicit GenerateDiagramTool(DiagramRenderer renderer);

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString resolvePath(const QString &path) const;

    DiagramRenderer m_renderer;
    QString         m_workspaceRoot;
};

// ── TreeSitterParseTool ──────────────────────────────────────────────────────
// Parse source code using Tree-sitter and return the AST.

class TreeSitterParseTool : public ITool
{
public:
    struct ParseResult {
        bool    ok;
        QString ast;          // S-expression AST
        int     nodeCount;
        int     errorCount;   // parse errors in the tree
        QString error;
    };

    // parser(code, language) → ParseResult
    using TreeSitterParser = std::function<ParseResult(
        const QString &code, const QString &language)>;

    explicit TreeSitterParseTool(TreeSitterParser parser);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    TreeSitterParser m_parser;
};

// ── PerformanceProfileTool ───────────────────────────────────────────────────
// Profile code execution for hotspot detection.

class PerformanceProfileTool : public ITool
{
public:
    struct ProfileResult {
        bool    ok;
        QString report;       // Human-readable profiling report
        QJsonArray hotspots;  // [{function, file, line, time_ms, percent}]
        int     totalSamples;
        double  totalMs;
        QString error;
    };

    // profiler(target, duration, type) → ProfileResult
    //   target: file path, process name, or PID
    //   duration: profiling duration in seconds
    //   type: "cpu", "memory", "io"
    using Profiler = std::function<ProfileResult(
        const QString &target, int durationSec, const QString &type)>;

    explicit PerformanceProfileTool(Profiler profiler);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    Profiler m_profiler;
};

// ── SymbolDocTool ────────────────────────────────────────────────────────────
// Get symbol documentation / hover info from LSP.

class SymbolDocTool : public ITool
{
public:
    struct DocResult {
        bool    ok;
        QString documentation;  // Markdown-formatted doc
        QString signature;      // Function/type signature
        QString docUrl;         // Link to online documentation
        QString error;
    };

    // getter(filePath, line, column) → DocResult
    using DocGetter = std::function<DocResult(
        const QString &filePath, int line, int column)>;

    explicit SymbolDocTool(DocGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    DocGetter m_getter;
};

// ── CodeCompletionTool ───────────────────────────────────────────────────────
// Get code completion suggestions from LSP.

class CodeCompletionTool : public ITool
{
public:
    struct CompletionItem {
        QString label;         // "push_back"
        QString kind;          // "method", "function", "variable", etc.
        QString detail;        // "void push_back(const T&)"
        QString documentation; // Brief doc
        QString insertText;    // Text to insert (may differ from label)
    };

    struct CompletionResult {
        bool    ok;
        QList<CompletionItem> items;
        bool    isIncomplete;  // More items available from server
        QString error;
    };

    // getter(filePath, line, column, prefix) → CompletionResult
    using CompletionGetter = std::function<CompletionResult(
        const QString &filePath, int line, int column, const QString &prefix)>;

    explicit CodeCompletionTool(CompletionGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    CompletionGetter m_getter;
};
