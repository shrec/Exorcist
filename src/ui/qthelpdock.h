#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTextBrowser;
class QSplitter;
class QLabel;
QT_END_NAMESPACE

#if __has_include(<QtHelp/QHelpEngineCore>)
#  define EXORCIST_HAS_QT_HELP 1
#else
#  define EXORCIST_HAS_QT_HELP 0
#endif

#if EXORCIST_HAS_QT_HELP
class QHelpEngineCore;
#endif

/// Qt Help dock — opens Qt documentation when user presses F1 over a Qt
/// class/function name in the editor.
///
/// Reads .qch help files via Qt's QHelpEngineCore (Qt6::Help). Auto-detects
/// a Qt-installed .qhc collection under typical install paths. Falls back
/// gracefully when Qt6::Help is unavailable at compile time, or when no
/// collection file is found at runtime.
class QtHelpDock : public QWidget
{
    Q_OBJECT
public:
    explicit QtHelpDock(QWidget *parent = nullptr);
    ~QtHelpDock() override;

    /// Look up a keyword (e.g. a class name or function name) and populate
    /// the topic list. If exactly one topic matches, also load it directly.
    void lookupKeyword(const QString &keyword);

    /// True when a Qt help collection (.qhc) was successfully loaded.
    bool hasCollection() const;

private slots:
    void onSearchTextEdited(const QString &text);
    void onTopicActivated(QListWidgetItem *item);

private:
    /// Probe well-known locations for a Qt .qhc collection file.
    /// Returns the first existing path or QString() if none found.
    static QString findCollectionFile();

    /// Display HTML content for the given help URL.
    void loadDocumentForUrl(const QUrl &url);

    /// Update the status label (used to surface errors to the user).
    void setStatus(const QString &text);

    QLineEdit    *m_searchEdit  = nullptr;
    QListWidget  *m_topicList   = nullptr;
    QTextBrowser *m_textBrowser = nullptr;
    QSplitter    *m_splitter    = nullptr;
    QLabel       *m_statusLabel = nullptr;

#if EXORCIST_HAS_QT_HELP
    QHelpEngineCore *m_engine = nullptr;
#endif
};
