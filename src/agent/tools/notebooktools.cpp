#include "notebooktools.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// ── NotebookContextTool ──────────────────────────────────────────────────────

ToolSpec NotebookContextTool::spec() const
{
    ToolSpec s;
    s.name = QStringLiteral("notebook_context");
    s.description = QStringLiteral(
        "Get the contents of notebook cells as context. "
        "Pass the notebook file path and optionally specific cell indices.");
    s.permission = AgentToolPermission::ReadOnly;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("Path to .ipynb file")}
            }},
            {QStringLiteral("cellIndices"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}},
                {QStringLiteral("description"), QStringLiteral("Optional: specific cell indices (0-based). If empty, returns all cells.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("filePath")}}
    };
    return s;
}

ToolExecResult NotebookContextTool::invoke(const QJsonObject &args)
{
    const QString path = args.value(QStringLiteral("filePath")).toString();
    auto doc = loadNotebookFromFile(path);
    if (!doc.isValid())
        return {false, {}, {}, QStringLiteral("Failed to load notebook: %1").arg(path)};

    const QJsonArray indices = args.value(QStringLiteral("cellIndices")).toArray();
    QString context;
    if (indices.isEmpty()) {
        context = m_mgr->fullContext(doc);
    } else {
        QStringList parts;
        for (const auto &idx : indices)
            parts << m_mgr->cellContext(doc, idx.toInt());
        context = parts.join(QStringLiteral("\n\n"));
    }

    return {true, {}, context, {}};
}

NotebookDocument NotebookContextTool::loadNotebookFromFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    NotebookDocument doc;
    doc.filePath = path;

    const QJsonObject kernelspec = root.value(QStringLiteral("metadata")).toObject()
                                       .value(QStringLiteral("kernelspec")).toObject();
    doc.kernelName = kernelspec.value(QStringLiteral("name")).toString();

    const QJsonArray cells = root.value(QStringLiteral("cells")).toArray();
    for (int i = 0; i < cells.size(); ++i) {
        const QJsonObject cellObj = cells[i].toObject();
        NotebookCell cell;
        cell.index = i;

        const QString cellType = cellObj.value(QStringLiteral("cell_type")).toString();
        if (cellType == QStringLiteral("code"))
            cell.type = NotebookCell::CellType::Code;
        else if (cellType == QStringLiteral("markdown"))
            cell.type = NotebookCell::CellType::Markdown;
        else
            cell.type = NotebookCell::CellType::Raw;

        // Source can be string or array of strings
        const QJsonValue src = cellObj.value(QStringLiteral("source"));
        if (src.isArray()) {
            QStringList lines;
            for (const auto &line : src.toArray()) lines << line.toString();
            cell.source = lines.join(QString());
        } else {
            cell.source = src.toString();
        }

        // Outputs
        const QJsonArray outputs = cellObj.value(QStringLiteral("outputs")).toArray();
        for (const auto &out : outputs) {
            const QJsonObject outObj = out.toObject();
            const QString outType = outObj.value(QStringLiteral("output_type")).toString();
            if (outType == QStringLiteral("stream")) {
                const QJsonValue text = outObj.value(QStringLiteral("text"));
                if (text.isArray()) {
                    for (const auto &t : text.toArray()) cell.outputs << t.toString();
                } else {
                    cell.outputs << text.toString();
                }
            } else if (outType == QStringLiteral("execute_result") ||
                       outType == QStringLiteral("display_data")) {
                const QJsonObject data = outObj.value(QStringLiteral("data")).toObject();
                if (data.contains(QStringLiteral("text/plain"))) {
                    const QJsonValue txt = data.value(QStringLiteral("text/plain"));
                    if (txt.isArray()) {
                        for (const auto &t : txt.toArray()) cell.outputs << t.toString();
                    } else {
                        cell.outputs << txt.toString();
                    }
                }
            }
        }

        cell.executed = cellObj.contains(QStringLiteral("execution_count")) &&
                        !cellObj.value(QStringLiteral("execution_count")).isNull();
        cell.executionCount = cellObj.value(QStringLiteral("execution_count")).toInt();

        doc.cells.append(cell);
    }
    return doc;
}

// ── ReadCellOutputTool ───────────────────────────────────────────────────────

ToolSpec ReadCellOutputTool::spec() const
{
    ToolSpec s;
    s.name = QStringLiteral("read_cell_output");
    s.description = QStringLiteral(
        "Read the output of a specific cell in a Jupyter notebook.");
    s.permission = AgentToolPermission::ReadOnly;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("Path to .ipynb file")}
            }},
            {QStringLiteral("cellIndex"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"), QStringLiteral("Cell index (0-based)")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("filePath"), QStringLiteral("cellIndex")}}
    };
    return s;
}

ToolExecResult ReadCellOutputTool::invoke(const QJsonObject &args)
{
    Q_UNUSED(m_mgr)
    const QString path = args.value(QStringLiteral("filePath")).toString();
    const int cellIdx = args.value(QStringLiteral("cellIndex")).toInt();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {false, {}, {}, QStringLiteral("Cannot open notebook: %1").arg(path)};

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonArray cells = root.value(QStringLiteral("cells")).toArray();

    if (cellIdx < 0 || cellIdx >= cells.size())
        return {false, {}, {}, QStringLiteral("Cell index %1 out of range").arg(cellIdx)};

    const QJsonObject cellObj = cells[cellIdx].toObject();
    const QJsonArray outputs = cellObj.value(QStringLiteral("outputs")).toArray();

    if (outputs.isEmpty())
        return {true, {}, QStringLiteral("Cell %1 has no output.").arg(cellIdx), {}};

    QStringList outLines;
    for (const auto &out : outputs) {
        const QJsonObject o = out.toObject();
        const QString type = o.value(QStringLiteral("output_type")).toString();
        if (type == QStringLiteral("stream")) {
            const QJsonValue text = o.value(QStringLiteral("text"));
            if (text.isArray())
                for (const auto &t : text.toArray()) outLines << t.toString();
            else
                outLines << text.toString();
        } else if (type == QStringLiteral("error")) {
            outLines << o.value(QStringLiteral("ename")).toString()
                     + QStringLiteral(": ")
                     + o.value(QStringLiteral("evalue")).toString();
        }
    }

    return {true, {}, outLines.join(QString()), {}};
}

// ── CreateNotebookTool ───────────────────────────────────────────────────────

ToolSpec CreateNotebookTool::spec() const
{
    ToolSpec s;
    s.name = QStringLiteral("create_notebook");
    s.description = QStringLiteral(
        "Create a new Jupyter notebook (.ipynb) file with specified cells.");
    s.permission = AgentToolPermission::SafeMutate;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("Path for the new .ipynb file")}
            }},
            {QStringLiteral("kernel"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("Kernel name (default: python3)")}
            }},
            {QStringLiteral("cells"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("object")},
                    {QStringLiteral("properties"), QJsonObject{
                        {QStringLiteral("type"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                        {QStringLiteral("source"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                    }}
                }},
                {QStringLiteral("description"), QStringLiteral("Array of {type: 'code'|'markdown', source: string}")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("filePath")}}
    };
    return s;
}

ToolExecResult CreateNotebookTool::invoke(const QJsonObject &args)
{
    const QString path = args.value(QStringLiteral("filePath")).toString();
    const QString kernel = args.value(QStringLiteral("kernel")).toString(QStringLiteral("python3"));
    const QJsonArray cellArgs = args.value(QStringLiteral("cells")).toArray();

    // Build the notebook JSON
    QJsonArray cells;
    for (const auto &c : cellArgs) {
        const QJsonObject co = c.toObject();
        QJsonObject cell;
        cell[QStringLiteral("cell_type")] = co.value(QStringLiteral("type")).toString(QStringLiteral("code"));
        cell[QStringLiteral("source")] = QJsonArray{co.value(QStringLiteral("source")).toString()};
        cell[QStringLiteral("metadata")] = QJsonObject{};
        if (co.value(QStringLiteral("type")).toString() == QStringLiteral("code")) {
            cell[QStringLiteral("outputs")] = QJsonArray{};
            cell[QStringLiteral("execution_count")] = QJsonValue();
        }
        cells.append(cell);
    }

    if (cells.isEmpty()) {
        // Create with one empty code cell
        QJsonObject cell;
        cell[QStringLiteral("cell_type")] = QStringLiteral("code");
        cell[QStringLiteral("source")] = QJsonArray{QStringLiteral("")};
        cell[QStringLiteral("metadata")] = QJsonObject{};
        cell[QStringLiteral("outputs")] = QJsonArray{};
        cell[QStringLiteral("execution_count")] = QJsonValue();
        cells.append(cell);
    }

    QJsonObject notebook;
    notebook[QStringLiteral("nbformat")] = 4;
    notebook[QStringLiteral("nbformat_minor")] = 5;
    notebook[QStringLiteral("cells")] = cells;
    notebook[QStringLiteral("metadata")] = QJsonObject{
        {QStringLiteral("kernelspec"), QJsonObject{
            {QStringLiteral("name"), kernel},
            {QStringLiteral("display_name"), kernel},
            {QStringLiteral("language"), QStringLiteral("python")}
        }},
        {QStringLiteral("language_info"), QJsonObject{
            {QStringLiteral("name"), QStringLiteral("python")}
        }}
    };

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {false, {}, {}, QStringLiteral("Cannot create file: %1").arg(path)};

    file.write(QJsonDocument(notebook).toJson(QJsonDocument::Indented));
    return {true, {}, QStringLiteral("Created notebook: %1 with %2 cells")
                .arg(path).arg(cells.size()), {}};
}

// ── EditNotebookCellsTool ────────────────────────────────────────────────────

ToolSpec EditNotebookCellsTool::spec() const
{
    ToolSpec s;
    s.name = QStringLiteral("edit_notebook_cells");
    s.description = QStringLiteral(
        "Add, modify, or delete cells in a Jupyter notebook (.ipynb).");
    s.permission = AgentToolPermission::SafeMutate;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("Path to .ipynb file")}
            }},
            {QStringLiteral("operations"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("object")},
                    {QStringLiteral("properties"), QJsonObject{
                        {QStringLiteral("action"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                        {QStringLiteral("index"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}},
                        {QStringLiteral("cellType"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                        {QStringLiteral("source"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
                    }}
                }},
                {QStringLiteral("description"), QStringLiteral("Array of {action: 'add'|'modify'|'delete', index, cellType, source}")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("filePath"), QStringLiteral("operations")}}
    };
    return s;
}

ToolExecResult EditNotebookCellsTool::invoke(const QJsonObject &args)
{
    const QString path = args.value(QStringLiteral("filePath")).toString();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {false, {}, {}, QStringLiteral("Cannot open: %1").arg(path)};

    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    QJsonArray cells = root.value(QStringLiteral("cells")).toArray();
    const QJsonArray ops = args.value(QStringLiteral("operations")).toArray();

    int added = 0, modified = 0, deleted = 0;
    // Process in reverse for deletes
    QList<QJsonObject> opList;
    for (const auto &op : ops)
        opList.append(op.toObject());

    for (const auto &op : opList) {
        const QString action = op.value(QStringLiteral("action")).toString();
        const int idx = op.value(QStringLiteral("index")).toInt();

        if (action == QStringLiteral("add")) {
            QJsonObject cell;
            cell[QStringLiteral("cell_type")] = op.value(QStringLiteral("cellType")).toString(QStringLiteral("code"));
            cell[QStringLiteral("source")] = QJsonArray{op.value(QStringLiteral("source")).toString()};
            cell[QStringLiteral("metadata")] = QJsonObject{};
            if (op.value(QStringLiteral("cellType")).toString(QStringLiteral("code")) == QStringLiteral("code")) {
                cell[QStringLiteral("outputs")] = QJsonArray{};
                cell[QStringLiteral("execution_count")] = QJsonValue();
            }
            if (idx >= 0 && idx <= cells.size())
                cells.insert(idx, cell);
            else
                cells.append(cell);
            ++added;
        } else if (action == QStringLiteral("modify")) {
            if (idx >= 0 && idx < cells.size()) {
                QJsonObject cell = cells[idx].toObject();
                cell[QStringLiteral("source")] = QJsonArray{op.value(QStringLiteral("source")).toString()};
                if (op.contains(QStringLiteral("cellType")))
                    cell[QStringLiteral("cell_type")] = op.value(QStringLiteral("cellType")).toString();
                cells[idx] = cell;
                ++modified;
            }
        } else if (action == QStringLiteral("delete")) {
            if (idx >= 0 && idx < cells.size()) {
                cells.removeAt(idx);
                ++deleted;
            }
        }
    }

    root[QStringLiteral("cells")] = cells;

    if (!file.open(QIODevice::WriteOnly))
        return {false, {}, {}, QStringLiteral("Cannot write: %1").arg(path)};
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));

    return {true, {},
            QStringLiteral("Edited %1: added %2, modified %3, deleted %4 cells")
                .arg(path).arg(added).arg(modified).arg(deleted),
            {}};
}

// ── GetNotebookSummaryTool ───────────────────────────────────────────────────

ToolSpec GetNotebookSummaryTool::spec() const
{
    ToolSpec s;
    s.name = QStringLiteral("get_notebook_summary");
    s.description = QStringLiteral(
        "Get the structure of a Jupyter notebook: cell count, types, "
        "execution status, and output mime types.");
    s.permission = AgentToolPermission::ReadOnly;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("Path to .ipynb file")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("filePath")}}
    };
    return s;
}

ToolExecResult GetNotebookSummaryTool::invoke(const QJsonObject &args)
{
    const QString path = args.value(QStringLiteral("filePath")).toString();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {false, {}, {}, QStringLiteral("Cannot open: %1").arg(path)};

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonArray cells = root.value(QStringLiteral("cells")).toArray();
    const QJsonObject metadata = root.value(QStringLiteral("metadata")).toObject();
    const QString kernel = metadata.value(QStringLiteral("kernelspec")).toObject()
                               .value(QStringLiteral("name")).toString();

    QStringList lines;
    lines << QStringLiteral("Notebook: %1").arg(path);
    lines << QStringLiteral("Kernel: %1").arg(kernel);
    lines << QStringLiteral("Total cells: %1").arg(cells.size());
    lines << QString();

    int codeCells = 0, markdownCells = 0, rawCells = 0;
    for (int i = 0; i < cells.size(); ++i) {
        const QJsonObject cell = cells[i].toObject();
        const QString type = cell.value(QStringLiteral("cell_type")).toString();

        if (type == QStringLiteral("code")) ++codeCells;
        else if (type == QStringLiteral("markdown")) ++markdownCells;
        else ++rawCells;

        const QString execStr = cell.value(QStringLiteral("execution_count")).isNull()
                                    ? QStringLiteral("not executed")
                                    : QStringLiteral("executed [%1]")
                                          .arg(cell.value(QStringLiteral("execution_count")).toInt());

        const QJsonArray outputs = cell.value(QStringLiteral("outputs")).toArray();
        QStringList mimeTypes;
        for (const auto &out : outputs) {
            const QJsonObject o = out.toObject();
            const QString otype = o.value(QStringLiteral("output_type")).toString();
            if (otype == QStringLiteral("stream"))
                mimeTypes << QStringLiteral("text/plain");
            else if (o.contains(QStringLiteral("data"))) {
                const QJsonObject data = o.value(QStringLiteral("data")).toObject();
                mimeTypes << data.keys();
            }
        }

        // First line preview
        const QJsonValue src = cell.value(QStringLiteral("source"));
        QString preview;
        if (src.isArray() && !src.toArray().isEmpty())
            preview = src.toArray().first().toString().trimmed();
        else
            preview = src.toString().section(QLatin1Char('\n'), 0, 0).trimmed();
        if (preview.size() > 60) preview = preview.left(60) + QStringLiteral("...");

        lines << QStringLiteral("Cell %1 [%2] %3%4: %5")
                     .arg(i).arg(type).arg(execStr)
                     .arg(mimeTypes.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(mimeTypes.join(QStringLiteral(", "))))
                     .arg(preview);
    }

    lines << QString();
    lines << QStringLiteral("Code: %1, Markdown: %2, Raw: %3")
                 .arg(codeCells).arg(markdownCells).arg(rawCells);

    return {true, {}, lines.join(QLatin1Char('\n')), {}};
}
