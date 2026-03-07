#include "symbolindex.h"

#include <QFileInfo>
#include <QRegularExpression>

SymbolIndex::SymbolIndex(QObject *parent)
    : QObject(parent)
{
}

void SymbolIndex::indexFile(const QString &filePath, const QString &content)
{
    QList<WorkspaceSymbol> symbols;

    const QString ext = QFileInfo(filePath).suffix().toLower();
    const bool isCpp = (ext == "cpp" || ext == "cxx" || ext == "cc"
                     || ext == "c" || ext == "h" || ext == "hpp" || ext == "hxx");
    const bool isJs  = (ext == "js" || ext == "ts" || ext == "jsx" || ext == "tsx");
    const bool isPy  = (ext == "py");
    const bool isJava = (ext == "java");
    const bool isCSharp = (ext == "cs");

    const QStringList lines = content.split('\n');

    if (isCpp || isCSharp) {
        // Match: class Foo, struct Bar, enum Baz, namespace Qux, void func(
        static const QRegularExpression rxClass(
            QStringLiteral(R"(^\s*(?:class|struct)\s+(\w+))"));
        static const QRegularExpression rxEnum(
            QStringLiteral(R"(^\s*enum\s+(?:class\s+)?(\w+))"));
        static const QRegularExpression rxNamespace(
            QStringLiteral(R"(^\s*namespace\s+(\w+))"));
        static const QRegularExpression rxFunc(
            QStringLiteral(R"(^\s*(?:[\w:*&<>]+\s+)+(\w+)\s*\()"));

        for (int i = 0; i < lines.size(); ++i) {
            const QString &line = lines[i];
            auto m = rxClass.match(line);
            if (m.hasMatch()) {
                WorkspaceSymbol sym;
                sym.name = m.captured(1);
                sym.filePath = filePath;
                sym.line = i + 1;
                sym.kind = line.contains("struct") ? WorkspaceSymbol::Struct
                                                   : WorkspaceSymbol::Class;
                symbols.append(sym);
                continue;
            }
            m = rxEnum.match(line);
            if (m.hasMatch()) {
                symbols.append({m.captured(1), filePath, i + 1, WorkspaceSymbol::Enum});
                continue;
            }
            m = rxNamespace.match(line);
            if (m.hasMatch()) {
                symbols.append({m.captured(1), filePath, i + 1, WorkspaceSymbol::Namespace});
                continue;
            }
            m = rxFunc.match(line);
            if (m.hasMatch() && !line.contains("if") && !line.contains("while")
                && !line.contains("for") && !line.contains("switch")) {
                symbols.append({m.captured(1), filePath, i + 1, WorkspaceSymbol::Function});
            }
        }
    } else if (isPy) {
        static const QRegularExpression rxPyClass(
            QStringLiteral(R"(^class\s+(\w+))"));
        static const QRegularExpression rxPyFunc(
            QStringLiteral(R"(^(?:\s*)def\s+(\w+))"));

        for (int i = 0; i < lines.size(); ++i) {
            auto m = rxPyClass.match(lines[i]);
            if (m.hasMatch()) {
                symbols.append({m.captured(1), filePath, i + 1, WorkspaceSymbol::Class});
                continue;
            }
            m = rxPyFunc.match(lines[i]);
            if (m.hasMatch()) {
                symbols.append({m.captured(1), filePath, i + 1, WorkspaceSymbol::Function});
            }
        }
    } else if (isJs) {
        static const QRegularExpression rxJsClass(
            QStringLiteral(R"(^\s*(?:export\s+)?class\s+(\w+))"));
        static const QRegularExpression rxJsFunc(
            QStringLiteral(R"(^\s*(?:export\s+)?(?:async\s+)?function\s+(\w+))"));
        static const QRegularExpression rxJsConst(
            QStringLiteral(R"(^\s*(?:export\s+)?(?:const|let|var)\s+(\w+)\s*=)"));

        for (int i = 0; i < lines.size(); ++i) {
            auto m = rxJsClass.match(lines[i]);
            if (m.hasMatch()) {
                symbols.append({m.captured(1), filePath, i + 1, WorkspaceSymbol::Class});
                continue;
            }
            m = rxJsFunc.match(lines[i]);
            if (m.hasMatch()) {
                symbols.append({m.captured(1), filePath, i + 1, WorkspaceSymbol::Function});
                continue;
            }
            m = rxJsConst.match(lines[i]);
            if (m.hasMatch()) {
                symbols.append({m.captured(1), filePath, i + 1, WorkspaceSymbol::Variable});
            }
        }
    } else if (isJava) {
        static const QRegularExpression rxJavaClass(
            QStringLiteral(R"(^\s*(?:public\s+|private\s+|protected\s+)?(?:abstract\s+)?class\s+(\w+))"));
        static const QRegularExpression rxJavaMethod(
            QStringLiteral(R"(^\s*(?:public|private|protected)\s+(?:static\s+)?[\w<>\[\]]+\s+(\w+)\s*\()"));

        for (int i = 0; i < lines.size(); ++i) {
            auto m = rxJavaClass.match(lines[i]);
            if (m.hasMatch()) {
                symbols.append({m.captured(1), filePath, i + 1, WorkspaceSymbol::Class});
                continue;
            }
            m = rxJavaMethod.match(lines[i]);
            if (m.hasMatch()) {
                symbols.append({m.captured(1), filePath, i + 1, WorkspaceSymbol::Method});
            }
        }
    }

    m_fileSymbols[filePath] = symbols;
    emit indexUpdated();
}

void SymbolIndex::removeFile(const QString &filePath)
{
    m_fileSymbols.remove(filePath);
    emit indexUpdated();
}

void SymbolIndex::clear()
{
    m_fileSymbols.clear();
    emit indexUpdated();
}

QList<WorkspaceSymbol> SymbolIndex::search(const QString &query, int maxResults) const
{
    QList<WorkspaceSymbol> results;
    const QString lower = query.toLower();

    for (auto it = m_fileSymbols.cbegin(); it != m_fileSymbols.cend(); ++it) {
        for (const auto &sym : it.value()) {
            if (sym.name.toLower().contains(lower)) {
                results.append(sym);
                if (results.size() >= maxResults)
                    return results;
            }
        }
    }
    return results;
}

QList<WorkspaceSymbol> SymbolIndex::symbolsInFile(const QString &filePath) const
{
    return m_fileSymbols.value(filePath);
}
