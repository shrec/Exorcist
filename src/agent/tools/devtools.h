#pragma once

#include "../itool.h"
#include "filesystemtools.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QProcess>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <functional>

// ── CompileAndRunTool ────────────────────────────────────────────────────────
// Compile a C++ snippet and execute it. Wraps the compile+link+run cycle
// into a single tool call. Uses system compiler (g++/clang++/cl).

class CompileAndRunTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("compile_and_run");
        s.description = QStringLiteral(
            "Compile a C++ code snippet and run the resulting executable. "
            "The code is written to a temp file, compiled with the system "
            "compiler (g++/clang++/cl), and executed. Returns compiler output, "
            "exit code, and program stdout/stderr.\n"
            "Supports C++17. Include headers as needed.\n"
            "Use for: quick experiments, verifying code behavior, running tests.");
        s.permission  = AgentToolPermission::Dangerous;
        s.timeoutMs   = 60000;
        s.contexts    = {QStringLiteral("cpp"), QStringLiteral("c")};
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("code"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("C++ source code to compile and run.")}
                }},
                {QStringLiteral("compiler"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Compiler to use. Default: auto-detect (g++, clang++, or cl).")}
                }},
                {QStringLiteral("args"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("array")},
                    {QStringLiteral("description"),
                     QStringLiteral("Additional compiler arguments (e.g. [\"-O2\", \"-lm\"]).")},
                    {QStringLiteral("items"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")}
                    }}
                }},
                {QStringLiteral("stdin"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Input to feed to the program's stdin.")}
                }},
                {QStringLiteral("timeout"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("Execution timeout in seconds (default: 10, max: 30).")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("code")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString code = args[QLatin1String("code")].toString();
        if (code.isEmpty())
            return {false, {}, {}, QStringLiteral("'code' is required.")};

        QTemporaryDir tmpDir;
        if (!tmpDir.isValid())
            return {false, {}, {}, QStringLiteral("Failed to create temp directory.")};

        // Write source file
        const QString srcPath = tmpDir.filePath(QStringLiteral("snippet.cpp"));
        QFile srcFile(srcPath);
        if (!srcFile.open(QIODevice::WriteOnly | QIODevice::Text))
            return {false, {}, {}, QStringLiteral("Failed to write source file.")};
        srcFile.write(code.toUtf8());
        srcFile.close();

#ifdef Q_OS_WIN
        const QString outPath = tmpDir.filePath(QStringLiteral("snippet.exe"));
#else
        const QString outPath = tmpDir.filePath(QStringLiteral("snippet"));
#endif

        // Detect compiler
        QString compiler = args[QLatin1String("compiler")].toString();
        if (compiler.isEmpty())
            compiler = detectCompiler();
        if (compiler.isEmpty())
            return {false, {}, {},
                    QStringLiteral("No C++ compiler found (tried g++, clang++, cl).")};

        // Build compiler arguments
        QStringList compileArgs;
#ifdef Q_OS_WIN
        if (compiler.contains(QLatin1String("cl"))) {
            compileArgs << QStringLiteral("/std:c++17")
                        << QStringLiteral("/EHsc")
                        << srcPath
                        << QStringLiteral("/Fe:") + outPath;
        } else
#endif
        {
            compileArgs << QStringLiteral("-std=c++17")
                        << QStringLiteral("-o") << outPath
                        << srcPath;
        }

        // Add user-specified args
        const QJsonArray extraArgs = args[QLatin1String("args")].toArray();
        for (const QJsonValue &a : extraArgs)
            compileArgs << a.toString();

        // ── Compile ──────────────────────────────────────────────────────
        QProcess compileProc;
        compileProc.setWorkingDirectory(tmpDir.path());
        compileProc.start(compiler, compileArgs);
        compileProc.waitForFinished(30000);

        const QString compileStdout = compileProc.readAllStandardOutput();
        const QString compileStderr = compileProc.readAllStandardError();
        const int compileExit = compileProc.exitCode();

        QString text = QStringLiteral("── Compile (%1) ──\n").arg(compiler);
        if (!compileStdout.isEmpty()) text += compileStdout;
        if (!compileStderr.isEmpty()) text += compileStderr;

        if (compileExit != 0) {
            text += QStringLiteral("\nCompilation FAILED (exit code %1)").arg(compileExit);
            return {false, {},  text,
                    QStringLiteral("Compilation failed with exit code %1").arg(compileExit)};
        }
        text += QStringLiteral("Compilation OK.\n");

        // ── Execute ──────────────────────────────────────────────────────
        const int timeoutSec = qBound(1, args[QLatin1String("timeout")].toInt(10), 30);

        QProcess runProc;
        runProc.setWorkingDirectory(m_workspaceRoot.isEmpty() ? tmpDir.path() : m_workspaceRoot);
        runProc.start(outPath, {});

        // Feed stdin if provided
        const QString stdinData = args[QLatin1String("stdin")].toString();
        if (!stdinData.isEmpty()) {
            runProc.write(stdinData.toUtf8());
            runProc.closeWriteChannel();
        }

        runProc.waitForFinished(timeoutSec * 1000);

        const QString runStdout = runProc.readAllStandardOutput();
        const QString runStderr = runProc.readAllStandardError();
        const int runExit = runProc.exitCode();

        text += QStringLiteral("\n── Run ──\n");
        if (!runStdout.isEmpty()) text += runStdout;
        if (!runStderr.isEmpty()) text += QStringLiteral("stderr: ") + runStderr;
        text += QStringLiteral("\nExit code: %1").arg(runExit);

        QJsonObject data;
        data[QLatin1String("compileExitCode")] = compileExit;
        data[QLatin1String("runExitCode")]     = runExit;
        data[QLatin1String("stdout")]          = runStdout;
        data[QLatin1String("stderr")]          = runStderr;
        data[QLatin1String("compiler")]        = compiler;

        return {runExit == 0, data, text, {}};
    }

private:
    static QString detectCompiler()
    {
        for (const QString &cc : {
                 QStringLiteral("g++"),
                 QStringLiteral("clang++"),
#ifdef Q_OS_WIN
                 QStringLiteral("cl"),
#endif
             }) {
            QProcess test;
            test.start(cc, {QStringLiteral("--version")});
            if (test.waitForFinished(3000) && test.exitCode() == 0)
                return cc;
        }
        return {};
    }

    QString m_workspaceRoot;
};

// ── ArchiveTool ──────────────────────────────────────────────────────────────
// Create and extract zip/tar.gz archives using system tools.

class ArchiveTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("archive");
        s.description = QStringLiteral(
            "Create or extract archives (zip, tar.gz). Operations:\n"
            "  'create' — pack files into an archive\n"
            "  'extract' — extract an archive to a directory\n"
            "  'list' — list contents of an archive\n"
            "Uses system 'tar' and 'zip/unzip' commands.");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 60000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("operation"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("One of: create, extract, list")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("create"), QStringLiteral("extract"),
                        QStringLiteral("list")}}
                }},
                {QStringLiteral("archivePath"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Path to the archive file to create/extract/list.")}
                }},
                {QStringLiteral("files"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("array")},
                    {QStringLiteral("description"),
                     QStringLiteral("Files/directories to add (for 'create').")},
                    {QStringLiteral("items"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")}
                    }}
                }},
                {QStringLiteral("outputDir"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Output directory for 'extract'. Default: current directory.")}
                }},
                {QStringLiteral("format"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Archive format: zip or tar.gz. Default: auto-detect from extension.")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("zip"), QStringLiteral("tar.gz")}}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("operation"), QStringLiteral("archivePath")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString op          = args[QLatin1String("operation")].toString();
        const QString archivePath = resolvePath(args[QLatin1String("archivePath")].toString());

        if (archivePath.isEmpty())
            return {false, {}, {}, QStringLiteral("'archivePath' is required.")};

        const QString format = detectFormat(archivePath,
                                            args[QLatin1String("format")].toString());

        if (op == QLatin1String("list"))
            return listArchive(archivePath, format);
        if (op == QLatin1String("extract"))
            return extractArchive(archivePath, format,
                                  resolvePath(args[QLatin1String("outputDir")].toString()));
        if (op == QLatin1String("create"))
            return createArchive(archivePath, format, args[QLatin1String("files")].toArray());

        return {false, {}, {}, QStringLiteral("Unknown operation: %1").arg(op)};
    }

private:
    ToolExecResult listArchive(const QString &path, const QString &fmt)
    {
        QProcess proc;
        if (fmt == QLatin1String("zip")) {
#ifdef Q_OS_WIN
            proc.start(QStringLiteral("tar"), {QStringLiteral("-tf"), path});
#else
            proc.start(QStringLiteral("unzip"), {QStringLiteral("-l"), path});
#endif
        } else {
            proc.start(QStringLiteral("tar"),
                       {QStringLiteral("-tzf"), path});
        }
        proc.waitForFinished(30000);

        if (proc.exitCode() != 0)
            return {false, {}, {}, proc.readAllStandardError()};

        return {true, {}, proc.readAllStandardOutput(), {}};
    }

    ToolExecResult extractArchive(const QString &path, const QString &fmt,
                                  const QString &outDir)
    {
        const QString targetDir = outDir.isEmpty() ? m_workspaceRoot : outDir;
        QDir().mkpath(targetDir);

        QProcess proc;
        proc.setWorkingDirectory(targetDir);
        if (fmt == QLatin1String("zip")) {
#ifdef Q_OS_WIN
            proc.start(QStringLiteral("tar"),
                       {QStringLiteral("-xf"), path, QStringLiteral("-C"), targetDir});
#else
            proc.start(QStringLiteral("unzip"),
                       {QStringLiteral("-o"), path, QStringLiteral("-d"), targetDir});
#endif
        } else {
            proc.start(QStringLiteral("tar"),
                       {QStringLiteral("-xzf"), path, QStringLiteral("-C"), targetDir});
        }
        proc.waitForFinished(60000);

        if (proc.exitCode() != 0)
            return {false, {}, {}, proc.readAllStandardError()};

        return {true, {},
                QStringLiteral("Extracted to %1\n%2")
                    .arg(targetDir, proc.readAllStandardOutput()), {}};
    }

    ToolExecResult createArchive(const QString &path, const QString &fmt,
                                 const QJsonArray &files)
    {
        if (files.isEmpty())
            return {false, {}, {}, QStringLiteral("'files' list is required for 'create'.")};

        QStringList fileList;
        for (const QJsonValue &v : files)
            fileList << v.toString();

        QProcess proc;
        proc.setWorkingDirectory(m_workspaceRoot.isEmpty() ? QDir::currentPath() : m_workspaceRoot);

        if (fmt == QLatin1String("zip")) {
#ifdef Q_OS_WIN
            QStringList tarArgs{QStringLiteral("-cf"), path};
            tarArgs.append(fileList);
            proc.start(QStringLiteral("tar"), tarArgs);
#else
            QStringList zipArgs{path};
            zipArgs << QStringLiteral("-r");
            zipArgs.append(fileList);
            proc.start(QStringLiteral("zip"), zipArgs);
#endif
        } else {
            QStringList tarArgs{QStringLiteral("-czf"), path};
            tarArgs.append(fileList);
            proc.start(QStringLiteral("tar"), tarArgs);
        }
        proc.waitForFinished(60000);

        if (proc.exitCode() != 0)
            return {false, {}, {}, proc.readAllStandardError()};

        const QFileInfo fi(path);
        return {true, {},
                QStringLiteral("Created %1 (%2 bytes)")
                    .arg(fi.fileName())
                    .arg(fi.size()), {}};
    }

    static QString detectFormat(const QString &path, const QString &hint)
    {
        if (!hint.isEmpty()) return hint;
        if (path.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive))
            return QStringLiteral("zip");
        return QStringLiteral("tar.gz");
    }

    QString resolvePath(const QString &path) const
    {
        if (path.isEmpty() || QFileInfo(path).isAbsolute() || m_workspaceRoot.isEmpty())
            return path;
        return QDir(m_workspaceRoot).absoluteFilePath(path);
    }

    QString m_workspaceRoot;
};

// ── CreatePatchTool ──────────────────────────────────────────────────────────
// Generate a unified diff / patch file from file changes.

class CreatePatchTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("create_patch_file");
        s.description = QStringLiteral(
            "Generate a unified diff (.patch/.diff) file. Operations:\n"
            "  'diff_files' — diff two files and save patch\n"
            "  'diff_content' — diff two text strings and return patch\n"
            "  'git_patch' — generate a git-format patch for staged/unstaged changes\n"
            "Useful for code review, sharing changes, and creating pull requests.");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 15000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("operation"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("One of: diff_files, diff_content, git_patch")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("diff_files"), QStringLiteral("diff_content"),
                        QStringLiteral("git_patch")}}
                }},
                {QStringLiteral("fileA"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("First file path (for diff_files).")}
                }},
                {QStringLiteral("fileB"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Second file path (for diff_files).")}
                }},
                {QStringLiteral("contentA"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("First text content (for diff_content).")}
                }},
                {QStringLiteral("contentB"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Second text content (for diff_content).")}
                }},
                {QStringLiteral("outputPath"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Path to save the patch file (optional).")}
                }},
                {QStringLiteral("staged"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"),
                     QStringLiteral("For git_patch: diff staged changes only. Default: false.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString op = args[QLatin1String("operation")].toString();

        if (op == QLatin1String("diff_files"))
            return diffFiles(args);
        if (op == QLatin1String("diff_content"))
            return diffContent(args);
        if (op == QLatin1String("git_patch"))
            return gitPatch(args);

        return {false, {}, {}, QStringLiteral("Unknown operation: %1").arg(op)};
    }

private:
    ToolExecResult diffFiles(const QJsonObject &args)
    {
        const QString fileA = resolvePath(args[QLatin1String("fileA")].toString());
        const QString fileB = resolvePath(args[QLatin1String("fileB")].toString());
        if (fileA.isEmpty() || fileB.isEmpty())
            return {false, {}, {}, QStringLiteral("'fileA' and 'fileB' are required.")};

        QProcess proc;
#ifdef Q_OS_WIN
        proc.start(QStringLiteral("git"),
                   {QStringLiteral("diff"), QStringLiteral("--no-index"),
                    QStringLiteral("--"), fileA, fileB});
#else
        proc.start(QStringLiteral("diff"),
                   {QStringLiteral("-u"), fileA, fileB});
#endif
        proc.waitForFinished(10000);

        const QString output = proc.readAllStandardOutput();
        if (output.isEmpty() && proc.exitCode() == 0)
            return {true, {}, QStringLiteral("Files are identical."), {}};

        // Save patch if output path given
        const QString outPath = args[QLatin1String("outputPath")].toString();
        if (!outPath.isEmpty()) {
            const QString resolved = resolvePath(outPath);
            QFile out(resolved);
            if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
                out.write(output.toUtf8());
                return {true, {},
                        QStringLiteral("Patch saved to %1 (%2 bytes)")
                            .arg(resolved).arg(output.size()), {}};
            }
        }

        return {true, {}, output, {}};
    }

    ToolExecResult diffContent(const QJsonObject &args)
    {
        const QString contentA = args[QLatin1String("contentA")].toString();
        const QString contentB = args[QLatin1String("contentB")].toString();
        if (contentA.isEmpty() && contentB.isEmpty())
            return {false, {}, {}, QStringLiteral("'contentA'/'contentB' are required.")};

        QTemporaryDir tmpDir;
        if (!tmpDir.isValid())
            return {false, {}, {}, QStringLiteral("Failed to create temp directory.")};

        const QString pathA = tmpDir.filePath(QStringLiteral("a.txt"));
        const QString pathB = tmpDir.filePath(QStringLiteral("b.txt"));

        QFile fA(pathA);
        if (!fA.open(QIODevice::WriteOnly))
            return {false, {}, {}, QStringLiteral("Failed to write temp file A.")};
        fA.write(contentA.toUtf8());
        fA.close();

        QFile fB(pathB);
        if (!fB.open(QIODevice::WriteOnly))
            return {false, {}, {}, QStringLiteral("Failed to write temp file B.")};
        fB.write(contentB.toUtf8());
        fB.close();

        QProcess proc;
#ifdef Q_OS_WIN
        proc.start(QStringLiteral("git"),
                   {QStringLiteral("diff"), QStringLiteral("--no-index"),
                    QStringLiteral("--"), pathA, pathB});
#else
        proc.start(QStringLiteral("diff"),
                   {QStringLiteral("-u"), pathA, pathB});
#endif
        proc.waitForFinished(10000);

        const QString output = proc.readAllStandardOutput();
        if (output.isEmpty())
            return {true, {}, QStringLiteral("Contents are identical."), {}};

        return {true, {}, output, {}};
    }

    ToolExecResult gitPatch(const QJsonObject &args)
    {
        if (m_workspaceRoot.isEmpty())
            return {false, {}, {}, QStringLiteral("No workspace root set.")};

        QStringList gitArgs{QStringLiteral("diff")};
        if (args[QLatin1String("staged")].toBool(false))
            gitArgs << QStringLiteral("--staged");

        QProcess proc;
        proc.setWorkingDirectory(m_workspaceRoot);
        proc.start(QStringLiteral("git"), gitArgs);
        proc.waitForFinished(15000);

        const QString output = proc.readAllStandardOutput();
        if (output.isEmpty())
            return {true, {}, QStringLiteral("No changes to patch."), {}};

        const QString outPath = args[QLatin1String("outputPath")].toString();
        if (!outPath.isEmpty()) {
            const QString resolved = resolvePath(outPath);
            QFile out(resolved);
            if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
                out.write(output.toUtf8());
                return {true, {},
                        QStringLiteral("Git patch saved to %1 (%2 bytes)")
                            .arg(resolved).arg(output.size()), {}};
            }
        }

        return {true, {}, output, {}};
    }

    QString resolvePath(const QString &path) const
    {
        if (path.isEmpty() || QFileInfo(path).isAbsolute() || m_workspaceRoot.isEmpty())
            return path;
        return QDir(m_workspaceRoot).absoluteFilePath(path);
    }

    QString m_workspaceRoot;
};

// ── ImageInfoTool ────────────────────────────────────────────────────────────
// Get image metadata and basic analysis.

class ImageInfoTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("image_info");
        s.description = QStringLiteral(
            "Get information about an image file. Returns dimensions, format, "
            "color depth, file size, and basic pixel statistics. Supports "
            "PNG, JPG, BMP, GIF, SVG, ICO, and other Qt-supported formats.\n"
            "Use for verifying screenshots, checking image properties, "
            "and inspecting generated diagrams.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("path"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Image file path.")}
                }},
                {QStringLiteral("includeHistogram"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"),
                     QStringLiteral("Include color channel histograms. Default: false.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString rawPath = args[QLatin1String("path")].toString();
        if (rawPath.isEmpty())
            return {false, {}, {}, QStringLiteral("'path' is required.")};

        const QString path = resolvePath(rawPath);

        if (!QFile::exists(path))
            return {false, {}, {}, QStringLiteral("File not found: %1").arg(path)};

        QImage img(path);
        if (img.isNull())
            return {false, {}, {}, QStringLiteral("Cannot load image: %1").arg(path)};

        const QFileInfo fi(path);
        QJsonObject data;
        data[QLatin1String("width")]      = img.width();
        data[QLatin1String("height")]     = img.height();
        data[QLatin1String("depth")]      = img.depth();
        data[QLatin1String("hasAlpha")]   = img.hasAlphaChannel();
        data[QLatin1String("format")]     = fi.suffix().toLower();
        data[QLatin1String("fileSize")]   = fi.size();
        data[QLatin1String("colorCount")] = img.colorCount();

        // Detect if image is mostly uniform (e.g., a blank screenshot)
        const bool isGrayscale = img.isGrayscale();
        data[QLatin1String("isGrayscale")] = isGrayscale;

        QString text = QStringLiteral("%1: %2x%3 %4-bit %5, %6 bytes")
                           .arg(fi.fileName())
                           .arg(img.width())
                           .arg(img.height())
                           .arg(img.depth())
                           .arg(fi.suffix().toUpper())
                           .arg(fi.size());
        if (img.hasAlphaChannel())
            text += QStringLiteral(" (with alpha)");
        if (isGrayscale)
            text += QStringLiteral(" (grayscale)");

        return {true, data, text, {}};
    }

private:
    QString resolvePath(const QString &path) const
    {
        if (path.isEmpty() || QFileInfo(path).isAbsolute() || m_workspaceRoot.isEmpty())
            return path;
        return QDir(m_workspaceRoot).absoluteFilePath(path);
    }

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

    explicit GenerateDiagramTool(DiagramRenderer renderer)
        : m_renderer(std::move(renderer)) {}

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("generate_diagram");
        s.description = QStringLiteral(
            "Generate a diagram from Mermaid or PlantUML markup. Renders the "
            "diagram to SVG or PNG. Useful for architecture diagrams, flowcharts, "
            "sequence diagrams, class diagrams, and state machines.\n"
            "Example Mermaid:\n"
            "  graph LR; A-->B; B-->C;\n"
            "Returns SVG content or path to generated image file.");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 30000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("markup"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Diagram markup in Mermaid or PlantUML syntax.")}
                }},
                {QStringLiteral("format"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Markup format: mermaid or plantuml. Default: mermaid.")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("mermaid"), QStringLiteral("plantuml")}}
                }},
                {QStringLiteral("outputPath"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Optional output file path (.svg or .png).")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("markup")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString markup = args[QLatin1String("markup")].toString();
        if (markup.isEmpty())
            return {false, {}, {}, QStringLiteral("'markup' is required.")};

        const QString format = args[QLatin1String("format")].toString(QStringLiteral("mermaid"));
        const QString outPath = args[QLatin1String("outputPath")].toString();
        const QString resolvedOut = outPath.isEmpty() ? QString() : resolvePath(outPath);

        const DiagramResult result = m_renderer(markup, format, resolvedOut);

        if (!result.ok)
            return {false, {}, {}, result.error};

        QString text;
        if (!result.pngPath.isEmpty())
            text = QStringLiteral("Diagram saved to: %1").arg(result.pngPath);
        else if (!result.svgContent.isEmpty())
            text = result.svgContent;
        else
            text = QStringLiteral("Diagram generated successfully.");

        return {true, {}, text, {}};
    }

private:
    QString resolvePath(const QString &path) const
    {
        if (path.isEmpty() || QFileInfo(path).isAbsolute() || m_workspaceRoot.isEmpty())
            return path;
        return QDir(m_workspaceRoot).absoluteFilePath(path);
    }

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

    explicit TreeSitterParseTool(TreeSitterParser parser)
        : m_parser(std::move(parser)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("tree_sitter_parse");
        s.description = QStringLiteral(
            "Parse source code using Tree-sitter and return the AST "
            "(Abstract Syntax Tree) as an S-expression. Useful for:\n"
            "  - Understanding code structure\n"
            "  - Finding syntax errors\n"
            "  - Analyzing code patterns\n"
            "  - Counting language constructs\n"
            "Supports: C, C++, Python, JavaScript, TypeScript, Rust, JSON.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 15000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("code"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Source code to parse.")}
                }},
                {QStringLiteral("language"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Language of the code: c, cpp, python, javascript, typescript, rust, json.")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("c"), QStringLiteral("cpp"),
                        QStringLiteral("python"), QStringLiteral("javascript"),
                        QStringLiteral("typescript"), QStringLiteral("rust"),
                        QStringLiteral("json")}}
                }},
                {QStringLiteral("maxDepth"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("Maximum AST depth to output (default: unlimited). "
                                    "Use to limit output for large files.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("code"), QStringLiteral("language")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString code     = args[QLatin1String("code")].toString();
        const QString language = args[QLatin1String("language")].toString();

        if (code.isEmpty())
            return {false, {}, {}, QStringLiteral("'code' is required.")};
        if (language.isEmpty())
            return {false, {}, {}, QStringLiteral("'language' is required.")};

        const ParseResult result = m_parser(code, language);

        if (!result.ok)
            return {false, {}, {}, result.error};

        QString text = result.ast;
        // Truncate very large ASTs
        constexpr int kMaxAst = 50 * 1024;
        if (text.size() > kMaxAst)
            text = text.left(kMaxAst) + QStringLiteral("\n... (truncated)");

        text += QStringLiteral("\n[%1 nodes, %2 errors]")
                    .arg(result.nodeCount)
                    .arg(result.errorCount);

        QJsonObject data;
        data[QLatin1String("nodeCount")]  = result.nodeCount;
        data[QLatin1String("errorCount")] = result.errorCount;

        return {true, data, text, {}};
    }

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

    explicit PerformanceProfileTool(Profiler profiler)
        : m_profiler(std::move(profiler)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("performance_profile");
        s.description = QStringLiteral(
            "Profile application performance for hotspot detection. "
            "Runs a profiler on a target (file, process, PID) and returns "
            "a report with the top hotspots, call counts, and time distribution.\n"
            "Profile types: cpu, memory, io.");
        s.permission  = AgentToolPermission::Dangerous;
        s.timeoutMs   = 120000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("target"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Target to profile: executable path, process name, or PID.")}
                }},
                {QStringLiteral("duration"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("Duration of profiling in seconds (default: 5, max: 30).")}
                }},
                {QStringLiteral("type"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Type of profiling: cpu, memory, io. Default: cpu.")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("cpu"), QStringLiteral("memory"),
                        QStringLiteral("io")}}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("target")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString target   = args[QLatin1String("target")].toString();
        if (target.isEmpty())
            return {false, {}, {}, QStringLiteral("'target' is required.")};

        const int duration     = qBound(1, args[QLatin1String("duration")].toInt(5), 30);
        const QString type     = args[QLatin1String("type")].toString(QStringLiteral("cpu"));

        const ProfileResult result = m_profiler(target, duration, type);

        if (!result.ok)
            return {false, {}, {}, result.error};

        QJsonObject data;
        data[QLatin1String("totalSamples")] = result.totalSamples;
        data[QLatin1String("totalMs")]      = result.totalMs;
        data[QLatin1String("hotspots")]     = result.hotspots;

        return {true, data, result.report, {}};
    }

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

    explicit SymbolDocTool(DocGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("symbol_documentation");
        s.description = QStringLiteral(
            "Get documentation for a symbol at a specific position in a file. "
            "Returns the symbol's documentation, type signature, and any "
            "associated documentation URLs. Uses LSP hover information.\n"
            "Useful for understanding APIs, checking function signatures, "
            "and reading documentation without leaving the editor.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("filePath"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("File path containing the symbol.")}
                }},
                {QStringLiteral("line"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("1-based line number of the symbol.")}
                }},
                {QStringLiteral("column"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("1-based column number of the symbol.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("filePath"), QStringLiteral("line"),
                QStringLiteral("column")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString filePath = args[QLatin1String("filePath")].toString();
        const int line         = args[QLatin1String("line")].toInt();
        const int column       = args[QLatin1String("column")].toInt();

        if (filePath.isEmpty() || line <= 0 || column <= 0)
            return {false, {}, {},
                    QStringLiteral("'filePath', 'line', and 'column' are required.")};

        const DocResult result = m_getter(filePath, line, column);

        if (!result.ok)
            return {false, {}, {}, result.error};

        QString text;
        if (!result.signature.isEmpty())
            text += QStringLiteral("```\n%1\n```\n").arg(result.signature);
        if (!result.documentation.isEmpty())
            text += result.documentation;
        if (!result.docUrl.isEmpty())
            text += QStringLiteral("\nDocs: %1").arg(result.docUrl);

        if (text.isEmpty())
            text = QStringLiteral("No documentation found for symbol at %1:%2:%3")
                       .arg(filePath).arg(line).arg(column);

        return {true, {}, text, {}};
    }

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

    explicit CodeCompletionTool(CompletionGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("code_completion");
        s.description = QStringLiteral(
            "Get code completion suggestions at a specific position in a file. "
            "Returns a list of completions with labels, kinds, and documentation.\n"
            "Useful for exploring available APIs, method chaining, and "
            "understanding what's available at a given code point.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("filePath"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("File path to get completions for.")}
                }},
                {QStringLiteral("line"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("1-based line number.")}
                }},
                {QStringLiteral("column"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("1-based column number.")}
                }},
                {QStringLiteral("prefix"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Filter prefix (e.g. 'push' to filter completions). Optional.")}
                }},
                {QStringLiteral("maxResults"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("Maximum completions to return (default: 20).")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("filePath"), QStringLiteral("line"),
                QStringLiteral("column")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString filePath    = args[QLatin1String("filePath")].toString();
        const int line            = args[QLatin1String("line")].toInt();
        const int column          = args[QLatin1String("column")].toInt();
        const QString prefix      = args[QLatin1String("prefix")].toString();
        const int maxResults      = args[QLatin1String("maxResults")].toInt(20);

        if (filePath.isEmpty() || line <= 0 || column <= 0)
            return {false, {}, {},
                    QStringLiteral("'filePath', 'line', and 'column' are required.")};

        const CompletionResult result = m_getter(filePath, line, column, prefix);

        if (!result.ok)
            return {false, {}, {}, result.error};

        if (result.items.isEmpty())
            return {true, {}, QStringLiteral("No completions found."), {}};

        QString text;
        const int count = qMin(result.items.size(), maxResults);
        for (int i = 0; i < count; ++i) {
            const auto &item = result.items[i];
            text += QStringLiteral("%1. %2 [%3]")
                        .arg(i + 1).arg(item.label, item.kind);
            if (!item.detail.isEmpty())
                text += QStringLiteral(" — %1").arg(item.detail);
            text += QLatin1Char('\n');
        }

        if (result.isIncomplete)
            text += QStringLiteral("(more completions available)");

        QJsonObject data;
        data[QLatin1String("count")]        = count;
        data[QLatin1String("isIncomplete")] = result.isIncomplete;

        return {true, data, text, {}};
    }

private:
    CompletionGetter m_getter;
};
