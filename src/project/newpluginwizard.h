#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

/// New Plugin scaffolding wizard.
///
/// Generates a complete plugin skeleton (CMakeLists.txt, plugin.json,
/// plugin.cpp, plugin.h, README.md) in a chosen folder. Five plugin
/// types are supported:
///
///   - View Contributor   : panel/dock plugin (IViewContributor)
///   - Command Plugin     : registers commands in menu/toolbar
///   - Language Plugin    : LSP integration template
///   - Agent Tool Plugin  : registers AI tools (IAgentToolPlugin)
///   - AI Provider Plugin : chat completion provider (IAgentPlugin)
///
/// Generated code is syntactically valid C++17 and compiles standalone
/// when Qt6 + Exorcist SDK headers are available.
class NewPluginWizard : public QDialog
{
    Q_OBJECT

public:
    enum PluginKind {
        ViewContributor = 0,
        CommandPlugin   = 1,
        LanguagePlugin  = 2,
        AgentToolPlugin = 3,
        AgentProvider   = 4,
    };

    explicit NewPluginWizard(QWidget *parent = nullptr);

    /// The path to the generated plugin folder (valid after accept()).
    QString createdPluginPath() const { return m_createdPath; }

    /// Path to the primary source file the user might want to open
    /// (e.g. the generated plugin.cpp). Valid after accept().
    QString primarySourceFile() const { return m_primarySource; }

private slots:
    void onBrowseFolder();
    void onCreate();
    void updateCreateButton();

private:
    bool validate(QString *errorOut) const;

    void writeCMakeLists(const QString &dir, const QString &targetName) const;
    void writePluginJson(const QString &dir) const;
    void writeReadme(const QString &dir, const QString &targetName) const;
    void writeSourceFiles(const QString &dir,
                          const QString &className,
                          QString *primarySourceOut) const;

    QString classNameFromId(const QString &id) const;
    QString targetNameFromId(const QString &id) const;
    PluginKind currentKind() const;
    static QString kindActivationEvent(PluginKind k);
    static QString kindCategory(PluginKind k);

    QLineEdit   *m_idEdit       = nullptr;
    QLineEdit   *m_nameEdit     = nullptr;
    QLineEdit   *m_descEdit     = nullptr;
    QLineEdit   *m_authorEdit   = nullptr;
    QComboBox   *m_typeCombo    = nullptr;
    QLineEdit   *m_folderEdit   = nullptr;
    QPushButton *m_browseBtn    = nullptr;
    QPushButton *m_createBtn    = nullptr;
    QPushButton *m_cancelBtn    = nullptr;
    QLabel      *m_hintLabel    = nullptr;

    QString m_createdPath;
    QString m_primarySource;
};
