#include "newpluginwizard.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

QString sanitizeForIdentifier(const QString &raw)
{
    QString out;
    out.reserve(raw.size());
    bool capNext = true;
    for (QChar c : raw) {
        if (c.isLetterOrNumber()) {
            out.append(capNext ? c.toUpper() : c);
            capNext = false;
        } else {
            capNext = true;
        }
    }
    if (out.isEmpty())
        out = QStringLiteral("MyPlugin");
    if (!out.at(0).isLetter())
        out.prepend(QLatin1Char('P'));
    return out;
}

QString sanitizeForTarget(const QString &raw)
{
    QString out;
    out.reserve(raw.size());
    for (QChar c : raw) {
        if (c.isLetterOrNumber())
            out.append(c.toLower());
        else if (c == QLatin1Char('.') || c == QLatin1Char('-') || c == QLatin1Char('_'))
            out.append(QLatin1Char('_'));
    }
    if (out.isEmpty())
        out = QStringLiteral("my_plugin");
    if (out.at(0).isDigit())
        out.prepend(QLatin1Char('p'));
    return out;
}

bool writeTextFile(const QString &path, const QString &content)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << content;
    return f.error() == QFile::NoError;
}

} // namespace

// ── NewPluginWizard ──────────────────────────────────────────────────────────

NewPluginWizard::NewPluginWizard(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Plugin"));
    setMinimumSize(560, 460);
    resize(640, 500);

    setStyleSheet(QStringLiteral(
        "NewPluginWizard { background: #1e1e1e; }"
        "QLabel { color: #cccccc; font-size: 13px; }"
        "QLabel#titleLabel { font-size: 18px; font-weight: 300; }"
        "QLabel#hintLabel { color: #888; font-size: 12px; }"
        "QLineEdit { background: #3c3c3c; border: 1px solid #3e3e42; "
        "  color: #cccccc; padding: 6px; font-size: 13px; selection-background-color: #094771; }"
        "QLineEdit:focus { border-color: #007acc; }"
        "QComboBox { background: #3c3c3c; border: 1px solid #3e3e42; "
        "  color: #cccccc; padding: 5px 8px; font-size: 13px; min-height: 28px; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: #252526; color: #cccccc; "
        "  selection-background-color: #094771; border: 1px solid #3e3e42; }"
        "QPushButton { background: #0e639c; color: white; border: none; "
        "  padding: 8px 20px; font-size: 13px; border-radius: 2px; }"
        "QPushButton:hover { background: #1177bb; }"
        "QPushButton:disabled { background: #3e3e42; color: #888; }"
        "QPushButton#browseBtn { background: #3c3c3c; color: #cccccc; padding: 6px 12px; }"
        "QPushButton#browseBtn:hover { background: #505050; }"
        "QPushButton#cancelBtn { background: transparent; color: #cccccc; "
        "  border: 1px solid #3e3e42; }"
        "QPushButton#cancelBtn:hover { background: #2a2d2e; }"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    auto *title = new QLabel(tr("Create a new plugin"), this);
    title->setObjectName(QStringLiteral("titleLabel"));
    mainLayout->addWidget(title);

    auto *subtitle = new QLabel(tr("Scaffolds CMakeLists.txt, plugin.json, "
                                   "source files and README.md."),
                                this);
    subtitle->setObjectName(QStringLiteral("hintLabel"));
    mainLayout->addWidget(subtitle);

    auto *form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_idEdit = new QLineEdit(this);
    m_idEdit->setPlaceholderText(QStringLiteral("org.example.myplugin"));
    form->addRow(tr("Plugin ID:"), m_idEdit);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral("My Plugin"));
    form->addRow(tr("Name:"), m_nameEdit);

    m_descEdit = new QLineEdit(this);
    m_descEdit->setPlaceholderText(tr("Short description"));
    form->addRow(tr("Description:"), m_descEdit);

    m_authorEdit = new QLineEdit(this);
    m_authorEdit->setPlaceholderText(tr("Author"));
    form->addRow(tr("Author:"), m_authorEdit);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("View Contributor (panel/dock)"),
                         int(ViewContributor));
    m_typeCombo->addItem(tr("Command Plugin (menu/toolbar)"),
                         int(CommandPlugin));
    m_typeCombo->addItem(tr("Language Plugin (LSP integration)"),
                         int(LanguagePlugin));
    m_typeCombo->addItem(tr("Agent Tool Plugin (AI tools)"),
                         int(AgentToolPlugin));
    m_typeCombo->addItem(tr("AI Provider Plugin (chat completion)"),
                         int(AgentProvider));
    form->addRow(tr("Type:"), m_typeCombo);

    auto *folderRow = new QHBoxLayout;
    folderRow->setSpacing(6);
    m_folderEdit = new QLineEdit(this);
    m_folderEdit->setPlaceholderText(tr("Output folder"));
    QString defaultParent = QStandardPaths::writableLocation(
                                QStandardPaths::DocumentsLocation);
    if (defaultParent.isEmpty())
        defaultParent = QDir::homePath();
    m_folderEdit->setText(defaultParent);
    m_browseBtn = new QPushButton(QStringLiteral("..."), this);
    m_browseBtn->setObjectName(QStringLiteral("browseBtn"));
    m_browseBtn->setFixedWidth(36);
    folderRow->addWidget(m_folderEdit, 1);
    folderRow->addWidget(m_browseBtn);
    form->addRow(tr("Output folder:"), folderRow);

    mainLayout->addLayout(form);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setObjectName(QStringLiteral("hintLabel"));
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setText(tr("A subfolder named after the target will be "
                            "created inside the output folder."));
    mainLayout->addWidget(m_hintLabel);

    mainLayout->addStretch(1);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setObjectName(QStringLiteral("cancelBtn"));
    m_createBtn = new QPushButton(tr("Create"), this);
    m_createBtn->setDefault(true);
    buttonRow->addWidget(m_cancelBtn);
    buttonRow->addWidget(m_createBtn);
    mainLayout->addLayout(buttonRow);

    connect(m_browseBtn, &QPushButton::clicked,
            this, &NewPluginWizard::onBrowseFolder);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_createBtn, &QPushButton::clicked,
            this, &NewPluginWizard::onCreate);
    connect(m_idEdit, &QLineEdit::textChanged,
            this, &NewPluginWizard::updateCreateButton);
    connect(m_nameEdit, &QLineEdit::textChanged,
            this, &NewPluginWizard::updateCreateButton);
    connect(m_folderEdit, &QLineEdit::textChanged,
            this, &NewPluginWizard::updateCreateButton);

    updateCreateButton();
}

void NewPluginWizard::onBrowseFolder()
{
    const QString start = m_folderEdit->text().isEmpty()
                              ? QDir::homePath()
                              : m_folderEdit->text();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Choose Output Folder"), start);
    if (!dir.isEmpty())
        m_folderEdit->setText(dir);
}

void NewPluginWizard::updateCreateButton()
{
    QString err;
    m_createBtn->setEnabled(validate(&err));
}

bool NewPluginWizard::validate(QString *errorOut) const
{
    auto fail = [&](const QString &msg) {
        if (errorOut) *errorOut = msg;
        return false;
    };
    const QString id = m_idEdit ? m_idEdit->text().trimmed() : QString();
    const QString name = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
    const QString folder = m_folderEdit ? m_folderEdit->text().trimmed() : QString();

    if (id.isEmpty())
        return fail(tr("Plugin ID is required."));

    static const QRegularExpression idRe(
        QStringLiteral("^[a-zA-Z][a-zA-Z0-9_]*(\\.[a-zA-Z][a-zA-Z0-9_]*)+$"));
    if (!idRe.match(id).hasMatch())
        return fail(tr("Plugin ID must be reverse-DNS, e.g. org.example.myplugin."));

    if (name.isEmpty())
        return fail(tr("Plugin name is required."));
    if (folder.isEmpty())
        return fail(tr("Output folder is required."));
    if (!QFileInfo(folder).isDir())
        return fail(tr("Output folder does not exist."));
    return true;
}

NewPluginWizard::PluginKind NewPluginWizard::currentKind() const
{
    return static_cast<PluginKind>(m_typeCombo->currentData().toInt());
}

QString NewPluginWizard::classNameFromId(const QString &id) const
{
    const QStringList parts = id.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return QStringLiteral("MyPlugin");
    QString last = parts.last();
    QString cls = sanitizeForIdentifier(last);
    if (!cls.endsWith(QStringLiteral("Plugin"), Qt::CaseInsensitive))
        cls.append(QStringLiteral("Plugin"));
    return cls;
}

QString NewPluginWizard::targetNameFromId(const QString &id) const
{
    const QStringList parts = id.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    const QString base = parts.isEmpty() ? id : parts.last();
    QString t = sanitizeForTarget(base);
    if (!t.endsWith(QStringLiteral("_plugin")))
        t.append(QStringLiteral("_plugin"));
    return t;
}

QString NewPluginWizard::kindActivationEvent(PluginKind k)
{
    switch (k) {
    case ViewContributor: return QStringLiteral("onView:default");
    case CommandPlugin:   return QStringLiteral("onCommand:*");
    case LanguagePlugin:  return QStringLiteral("onLanguage:plaintext");
    case AgentToolPlugin: return QStringLiteral("onAgent:tool");
    case AgentProvider:   return QStringLiteral("onAgent:provider");
    }
    return QStringLiteral("*");
}

QString NewPluginWizard::kindCategory(PluginKind k)
{
    switch (k) {
    case ViewContributor: return QStringLiteral("view");
    case CommandPlugin:   return QStringLiteral("command");
    case LanguagePlugin:  return QStringLiteral("language");
    case AgentToolPlugin: return QStringLiteral("agent-tool");
    case AgentProvider:   return QStringLiteral("agent-provider");
    }
    return QStringLiteral("misc");
}

void NewPluginWizard::onCreate()
{
    QString err;
    if (!validate(&err)) {
        QMessageBox::warning(this, tr("Invalid input"), err);
        return;
    }

    const QString id     = m_idEdit->text().trimmed();
    const QString folder = m_folderEdit->text().trimmed();
    const QString target = targetNameFromId(id);
    const QString className = classNameFromId(id);

    QDir parent(folder);
    if (parent.exists(target)) {
        const auto answer = QMessageBox::question(
            this, tr("Folder exists"),
            tr("A folder named '%1' already exists in the output location. "
               "Files inside will be overwritten. Continue?").arg(target));
        if (answer != QMessageBox::Yes)
            return;
    } else {
        if (!parent.mkpath(target)) {
            QMessageBox::critical(this, tr("Create failed"),
                                  tr("Could not create folder '%1' inside '%2'.")
                                      .arg(target, folder));
            return;
        }
    }

    const QString pluginDir = parent.absoluteFilePath(target);

    writeCMakeLists(pluginDir, target);
    writePluginJson(pluginDir);
    writeReadme(pluginDir, target);
    QString primary;
    writeSourceFiles(pluginDir, className, &primary);

    m_createdPath    = pluginDir;
    m_primarySource  = primary;

    QMessageBox::information(
        this, tr("Plugin created"),
        tr("Plugin scaffolded at:\n%1\n\n"
           "Add the folder to plugins/CMakeLists.txt to build it:\n"
           "    add_subdirectory(%2)").arg(pluginDir, target));

    accept();
}

// ── File generators ──────────────────────────────────────────────────────────

void NewPluginWizard::writeCMakeLists(const QString &dir, const QString &target) const
{
    const QString pluginCpp = QStringLiteral("plugin.cpp");
    const QString pluginH   = QStringLiteral("plugin.h");
    const PluginKind kind = currentKind();

    QString extraLibs;
    QString extraComment;
    switch (kind) {
    case ViewContributor:
        extraLibs    = QStringLiteral("Qt6::Widgets");
        extraComment = QStringLiteral("# View contributor: links Qt Widgets for QWidget-based panels.");
        break;
    case CommandPlugin:
        extraLibs    = QStringLiteral("Qt6::Widgets");
        extraComment = QStringLiteral("# Command plugin: registers commands with the host.");
        break;
    case LanguagePlugin:
        extraLibs    = QStringLiteral("Qt6::Network Qt6::Widgets");
        extraComment = QStringLiteral("# Language plugin: typically needs Qt::Network for LSP transport.");
        break;
    case AgentToolPlugin:
        extraLibs    = QStringLiteral("Qt6::Core");
        extraComment = QStringLiteral("# Agent tool plugin: registers AI tools, no UI required.");
        break;
    case AgentProvider:
        extraLibs    = QStringLiteral("Qt6::Network");
        extraComment = QStringLiteral("# AI provider plugin: typically needs Qt::Network for HTTP calls.");
        break;
    }

    QString tmpl;
    QTextStream ts(&tmpl);
    ts << "# Auto-generated by Exorcist NewPluginWizard.\n";
    ts << "# Plugin target: " << target << "\n";
    ts << "\n";
    ts << "cmake_minimum_required(VERSION 3.20)\n";
    ts << "\n";
    ts << "find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)\n";
    ts << "\n";
    ts << "set(CMAKE_AUTOMOC ON)\n";
    ts << "set(CMAKE_CXX_STANDARD 17)\n";
    ts << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
    ts << "\n";
    ts << "add_library(" << target << " SHARED\n";
    ts << "    " << pluginH << "\n";
    ts << "    " << pluginCpp << "\n";
    ts << ")\n";
    ts << "\n";
    ts << extraComment << "\n";
    ts << "target_link_libraries(" << target << " PRIVATE " << extraLibs << ")\n";
    ts << "\n";
    ts << "# Include the Exorcist SDK headers. When developing inside the\n";
    ts << "# Exorcist tree, ${CMAKE_SOURCE_DIR}/src already provides them.\n";
    ts << "if(DEFINED EXORCIST_SDK_INCLUDE_DIR)\n";
    ts << "    target_include_directories(" << target << " PRIVATE \"${EXORCIST_SDK_INCLUDE_DIR}\")\n";
    ts << "elseif(EXISTS \"${CMAKE_SOURCE_DIR}/src/plugininterface.h\")\n";
    ts << "    target_include_directories(" << target << " PRIVATE \"${CMAKE_SOURCE_DIR}/src\")\n";
    ts << "endif()\n";
    ts << "\n";
    ts << "set_target_properties(" << target << " PROPERTIES\n";
    ts << "    OUTPUT_NAME  \"" << target << "\"\n";
    ts << "    LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/src/plugins\"\n";
    ts << "    RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/src/plugins\"\n";
    ts << ")\n";
    ts << "\n";
    ts << "# Copy plugin.json next to the DLL so PluginManager can load it.\n";
    ts << "add_custom_command(TARGET " << target << " POST_BUILD\n";
    ts << "    COMMAND ${CMAKE_COMMAND} -E copy_if_different\n";
    ts << "        \"${CMAKE_CURRENT_SOURCE_DIR}/plugin.json\"\n";
    ts << "        \"$<TARGET_FILE_DIR:" << target << ">/" << target << ".json\"\n";
    ts << ")\n";

    writeTextFile(QDir(dir).absoluteFilePath(QStringLiteral("CMakeLists.txt")), tmpl);
}

void NewPluginWizard::writePluginJson(const QString &dir) const
{
    const QString id          = m_idEdit->text().trimmed();
    const QString name        = m_nameEdit->text().trimmed();
    const QString description = m_descEdit->text().trimmed();
    const QString author      = m_authorEdit->text().trimmed();
    const PluginKind kind     = currentKind();

    QJsonObject manifest;
    manifest[QStringLiteral("id")]          = id;
    manifest[QStringLiteral("name")]        = name;
    manifest[QStringLiteral("version")]     = QStringLiteral("0.1.0");
    manifest[QStringLiteral("description")] = description;
    manifest[QStringLiteral("author")]      = author;
    manifest[QStringLiteral("license")]     = QStringLiteral("MIT");
    manifest[QStringLiteral("layer")]       = QStringLiteral("cpp-sdk");
    manifest[QStringLiteral("category")]    = kindCategory(kind);
    manifest[QStringLiteral("apiVersion")]  = QStringLiteral("1.0");

    QJsonArray activation;
    activation.append(kindActivationEvent(kind));
    manifest[QStringLiteral("activationEvents")] = activation;

    QJsonArray permissions;
    switch (kind) {
    case ViewContributor:
    case CommandPlugin:
        permissions.append(QStringLiteral("ui.contribute"));
        break;
    case LanguagePlugin:
        permissions.append(QStringLiteral("ui.contribute"));
        permissions.append(QStringLiteral("network.connect"));
        permissions.append(QStringLiteral("process.spawn"));
        break;
    case AgentToolPlugin:
        permissions.append(QStringLiteral("agent.tools"));
        break;
    case AgentProvider:
        permissions.append(QStringLiteral("agent.providers"));
        permissions.append(QStringLiteral("network.connect"));
        break;
    }
    manifest[QStringLiteral("permissions")] = permissions;

    QJsonObject contributions;
    switch (kind) {
    case ViewContributor: {
        QJsonArray views;
        QJsonObject view;
        view[QStringLiteral("id")]       = QStringLiteral("default");
        view[QStringLiteral("title")]    = name.isEmpty() ? QStringLiteral("Plugin View") : name;
        view[QStringLiteral("location")] = QStringLiteral("sidebar-right");
        views.append(view);
        contributions[QStringLiteral("views")] = views;
        break;
    }
    case CommandPlugin: {
        QJsonArray commands;
        QJsonObject cmd;
        cmd[QStringLiteral("id")]       = id + QStringLiteral(".helloWorld");
        cmd[QStringLiteral("title")]    = QStringLiteral("Hello from %1").arg(name);
        cmd[QStringLiteral("category")] = name;
        commands.append(cmd);
        contributions[QStringLiteral("commands")] = commands;
        break;
    }
    case LanguagePlugin: {
        QJsonArray languages;
        QJsonObject lang;
        lang[QStringLiteral("id")]         = QStringLiteral("plaintext");
        lang[QStringLiteral("aliases")]    = QJsonArray{QStringLiteral("Plain Text")};
        lang[QStringLiteral("extensions")] = QJsonArray{QStringLiteral(".txt")};
        languages.append(lang);
        contributions[QStringLiteral("languages")] = languages;
        break;
    }
    case AgentToolPlugin: {
        QJsonArray tools;
        QJsonObject t;
        t[QStringLiteral("id")]          = id + QStringLiteral(".sample");
        t[QStringLiteral("description")] = QStringLiteral("Sample agent tool");
        tools.append(t);
        contributions[QStringLiteral("agentTools")] = tools;
        break;
    }
    case AgentProvider: {
        QJsonArray providers;
        QJsonObject p;
        p[QStringLiteral("id")]   = id + QStringLiteral(".provider");
        p[QStringLiteral("name")] = name;
        providers.append(p);
        contributions[QStringLiteral("agentProviders")] = providers;
        break;
    }
    }
    manifest[QStringLiteral("contributions")] = contributions;

    const QString jsonPath = QDir(dir).absoluteFilePath(QStringLiteral("plugin.json"));
    writeTextFile(jsonPath, QString::fromUtf8(
        QJsonDocument(manifest).toJson(QJsonDocument::Indented)));
}

void NewPluginWizard::writeReadme(const QString &dir, const QString &target) const
{
    const QString name    = m_nameEdit->text().trimmed();
    const QString id      = m_idEdit->text().trimmed();
    const QString descr   = m_descEdit->text().trimmed();
    const QString author  = m_authorEdit->text().trimmed();
    const PluginKind kind = currentKind();

    QString readme;
    QTextStream rs(&readme);
    rs << "# " << name << "\n\n";
    rs << descr << "\n\n";
    rs << "**Plugin ID:** `" << id << "`  \n";
    rs << "**Type:** " << kindCategory(kind) << "  \n";
    if (!author.isEmpty())
        rs << "**Author:** " << author << "  \n";
    rs << "\n";
    rs << "## Build\n\n";
    rs << "From the Exorcist repo root:\n\n";
    rs << "```bash\n";
    rs << "# 1. Add the folder to plugins/CMakeLists.txt:\n";
    rs << "#    add_subdirectory(" << target << ")\n";
    rs << "# 2. Build:\n";
    rs << "cmake --build build-llvm --config Debug --target " << target << "\n";
    rs << "```\n\n";
    rs << "The compiled DLL plus `" << target << ".json` manifest will be placed "
       << "in `build-llvm/src/plugins/` and loaded automatically on next IDE start.\n\n";
    rs << "## Layout\n\n";
    rs << "- `CMakeLists.txt` - build script\n";
    rs << "- `plugin.json`    - manifest (id, contributions, permissions)\n";
    rs << "- `plugin.h`       - plugin class declaration\n";
    rs << "- `plugin.cpp`     - plugin class implementation\n\n";
    rs << "## Next steps\n\n";
    switch (kind) {
    case ViewContributor:
        rs << "- Implement `createView()` to return your dock panel widget.\n";
        rs << "- Edit `plugin.json` to add additional view contributions.\n";
        break;
    case CommandPlugin:
        rs << "- Register more commands in `initialize()` via the host's command service.\n";
        rs << "- Add the new command IDs to `plugin.json`.\n";
        break;
    case LanguagePlugin:
        rs << "- Wire up an LSP transport (process or socket).\n";
        rs << "- Update `plugin.json` to declare your languages, file extensions, and grammars.\n";
        break;
    case AgentToolPlugin:
        rs << "- Implement one or more `ITool` subclasses inside `createTools()`.\n";
        rs << "- Restrict `contexts()` if your tools should only load in some workspaces.\n";
        break;
    case AgentProvider:
        rs << "- Implement the streaming `chat()` method against your provider's HTTP API.\n";
        rs << "- Surface the provider's models in `models()`.\n";
        break;
    }

    writeTextFile(QDir(dir).absoluteFilePath(QStringLiteral("README.md")), readme);
}

void NewPluginWizard::writeSourceFiles(const QString &dir,
                                       const QString &className,
                                       QString *primarySourceOut) const
{
    const QString id    = m_idEdit->text().trimmed();
    const QString name  = m_nameEdit->text().trimmed();
    const QString descr = m_descEdit->text().trimmed();
    const QString author = m_authorEdit->text().trimmed();
    const PluginKind kind = currentKind();

    QString header;
    QTextStream hs(&header);
    hs << "// " << name << " - " << descr << "\n";
    hs << "// Generated by Exorcist NewPluginWizard.\n";
    hs << "#pragma once\n\n";
    hs << "#include <QObject>\n";
    hs << "#include \"plugininterface.h\"\n";
    switch (kind) {
    case ViewContributor:
        hs << "#include \"plugin/iviewcontributor.h\"\n";
        hs << "#include <QWidget>\n";
        break;
    case CommandPlugin:
        // No additional headers needed beyond IPlugin.
        break;
    case LanguagePlugin:
        hs << "#include \"plugin/ilanguagecontributor.h\"\n";
        break;
    case AgentToolPlugin:
        hs << "#include \"agent/iagentplugin.h\"\n";
        hs << "#include <vector>\n";
        hs << "#include <memory>\n";
        break;
    case AgentProvider:
        hs << "#include \"agent/iagentplugin.h\"\n";
        hs << "#include <QList>\n";
        break;
    }
    hs << "\n";

    QString interfaces = QStringLiteral("public QObject, public IPlugin");
    QString interfaceMacroLines;
    {
        QTextStream ms(&interfaceMacroLines);
        ms << "    Q_INTERFACES(IPlugin)";
        switch (kind) {
        case ViewContributor:
            interfaces += QStringLiteral(", public IViewContributor");
            ms << " Q_INTERFACES(IViewContributor)";
            break;
        case CommandPlugin:
        case LanguagePlugin:
            // IPlugin only.
            break;
        case AgentToolPlugin:
            interfaces += QStringLiteral(", public IAgentToolPlugin");
            ms << " Q_INTERFACES(IAgentToolPlugin)";
            break;
        case AgentProvider:
            interfaces += QStringLiteral(", public IAgentPlugin");
            ms << " Q_INTERFACES(IAgentPlugin)";
            break;
        }
    }

    hs << "class " << className << " : " << interfaces << "\n";
    hs << "{\n";
    hs << "    Q_OBJECT\n";
    hs << "    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE \"plugin.json\")\n";
    hs << interfaceMacroLines << "\n";
    hs << "public:\n";
    hs << "    " << className << "() = default;\n";
    hs << "    ~" << className << "() override = default;\n\n";
    hs << "    PluginInfo info() const override;\n";
    hs << "    bool initialize(IHostServices *host) override;\n";
    hs << "    void shutdown() override;\n\n";

    switch (kind) {
    case ViewContributor:
        hs << "    // IViewContributor\n";
        hs << "    QWidget *createView(const QString &viewId, QWidget *parent) override;\n";
        break;
    case CommandPlugin:
        hs << "    // Command plugin entry points are wired in initialize().\n";
        break;
    case LanguagePlugin:
        hs << "    // Language plugins typically wire LSP in initialize().\n";
        break;
    case AgentToolPlugin:
        hs << "    // IAgentToolPlugin\n";
        hs << "    QString toolPluginName() const override { return QStringLiteral(\""
           << name << "\"); }\n";
        hs << "    std::vector<std::unique_ptr<ITool>> createTools() override;\n";
        break;
    case AgentProvider:
        hs << "    // IAgentPlugin\n";
        hs << "    QList<IAgentProvider *> createProviders(QObject *parent) override;\n";
        break;
    }

    hs << "\nprivate:\n";
    hs << "    IHostServices *m_host = nullptr;\n";
    hs << "};\n";

    const QString headerPath = QDir(dir).absoluteFilePath(QStringLiteral("plugin.h"));
    writeTextFile(headerPath, header);

    // ── plugin.cpp ───────────────────────────────────────────────────────────
    QString src;
    QTextStream cs(&src);
    cs << "// " << name << " - implementation\n";
    cs << "// Generated by Exorcist NewPluginWizard.\n";
    cs << "#include \"plugin.h\"\n\n";
    cs << "#include \"sdk/ihostservices.h\"\n";
    if (kind == ViewContributor) {
        cs << "#include <QLabel>\n";
        cs << "#include <QVBoxLayout>\n";
    }
    cs << "\n";

    cs << "PluginInfo " << className << "::info() const\n";
    cs << "{\n";
    cs << "    PluginInfo i;\n";
    cs << "    i.id          = QStringLiteral(\"" << id << "\");\n";
    cs << "    i.name        = QStringLiteral(\"" << name << "\");\n";
    cs << "    i.version     = QStringLiteral(\"0.1.0\");\n";
    cs << "    i.description = QStringLiteral(\"" << descr << "\");\n";
    cs << "    i.author      = QStringLiteral(\"" << author << "\");\n";
    cs << "    i.apiVersion  = QStringLiteral(\"1.0\");\n";
    cs << "    return i;\n";
    cs << "}\n\n";

    cs << "bool " << className << "::initialize(IHostServices *host)\n";
    cs << "{\n";
    cs << "    m_host = host;\n";
    switch (kind) {
    case ViewContributor:
        cs << "    // The host calls createView() lazily for each declared view.\n";
        cs << "    // Use `host` to access services (notifications, output, etc.).\n";
        break;
    case CommandPlugin:
        cs << "    // Register commands with the host's command service.\n";
        cs << "    // Example (pseudo-code):\n";
        cs << "    // if (auto *cmds = host ? host->commands() : nullptr) {\n";
        cs << "    //     cmds->registerCommand(QStringLiteral(\""
           << id << ".helloWorld\"),\n";
        cs << "    //         [host] { /* implement command */ });\n";
        cs << "    // }\n";
        break;
    case LanguagePlugin:
        cs << "    // Hook your LSP server here. Typical pattern:\n";
        cs << "    //   - spawn the language server process\n";
        cs << "    //   - wire ProcessLspTransport to LspClient\n";
        cs << "    //   - register diagnostics + completion via the host\n";
        break;
    case AgentToolPlugin:
        cs << "    // Tools are returned from createTools(). The host will\n";
        cs << "    // collect and register them via the agent platform.\n";
        break;
    case AgentProvider:
        cs << "    // Providers are returned from createProviders(). The host\n";
        cs << "    // wires each into the agent orchestrator.\n";
        break;
    }
    cs << "    return true;\n";
    cs << "}\n\n";

    cs << "void " << className << "::shutdown()\n";
    cs << "{\n";
    cs << "    m_host = nullptr;\n";
    cs << "}\n";

    if (kind == ViewContributor) {
        cs << "\n";
        cs << "QWidget *" << className << "::createView(const QString &viewId, QWidget *parent)\n";
        cs << "{\n";
        cs << "    Q_UNUSED(viewId);\n";
        cs << "    auto *root = new QWidget(parent);\n";
        cs << "    auto *layout = new QVBoxLayout(root);\n";
        cs << "    layout->setContentsMargins(8, 8, 8, 8);\n";
        cs << "    auto *label = new QLabel(QStringLiteral(\"" << name
           << " - replace me\"), root);\n";
        cs << "    layout->addWidget(label);\n";
        cs << "    layout->addStretch(1);\n";
        cs << "    return root;\n";
        cs << "}\n";
    } else if (kind == AgentToolPlugin) {
        cs << "\n";
        cs << "std::vector<std::unique_ptr<ITool>> " << className << "::createTools()\n";
        cs << "{\n";
        cs << "    std::vector<std::unique_ptr<ITool>> tools;\n";
        cs << "    // tools.emplace_back(std::make_unique<MyTool>());\n";
        cs << "    return tools;\n";
        cs << "}\n";
    } else if (kind == AgentProvider) {
        cs << "\n";
        cs << "QList<IAgentProvider *> " << className << "::createProviders(QObject *parent)\n";
        cs << "{\n";
        cs << "    Q_UNUSED(parent);\n";
        cs << "    QList<IAgentProvider *> providers;\n";
        cs << "    // providers.append(new MyProvider(parent));\n";
        cs << "    return providers;\n";
        cs << "}\n";
    }

    const QString srcPath = QDir(dir).absoluteFilePath(QStringLiteral("plugin.cpp"));
    writeTextFile(srcPath, src);

    if (primarySourceOut)
        *primarySourceOut = srcPath;
}
