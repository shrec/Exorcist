#include "codegraphdb.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>

#include <atomic>

// ── Connection lifecycle ─────────────────────────────────────────────────────

CodeGraphDb::~CodeGraphDb()
{
    close();
}

bool CodeGraphDb::open(const QString &dbPath)
{
    static std::atomic<int> s_counter{0};
    m_connectionName = QStringLiteral("codegraph_%1").arg(++s_counter);

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open())
        return false;

    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("PRAGMA journal_mode=WAL")))
        qWarning("CodeGraphDb: WAL mode failed: %s",
                 qPrintable(q.lastError().text()));
    if (!q.exec(QStringLiteral("PRAGMA synchronous=NORMAL")))
        qWarning("CodeGraphDb: synchronous pragma failed: %s",
                 qPrintable(q.lastError().text()));
    if (!q.exec(QStringLiteral("PRAGMA foreign_keys=ON")))
        qWarning("CodeGraphDb: foreign_keys pragma failed: %s",
                 qPrintable(q.lastError().text()));
    return true;
}

void CodeGraphDb::close()
{
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool CodeGraphDb::isOpen() const
{
    return m_db.isOpen();
}

QSqlDatabase &CodeGraphDb::database()
{
    return m_db;
}

QString CodeGraphDb::defaultPath(const QString &workspaceRoot)
{
    return workspaceRoot + QStringLiteral("/.exorcist/codegraph.db");
}

// ── Schema creation ──────────────────────────────────────────────────────────

bool CodeGraphDb::createSchema()
{
    QSqlQuery q(m_db);

    // Drop existing tables for clean rebuild
    static const char *const drops[] = {
        "DROP TABLE IF EXISTS files",
        "DROP TABLE IF EXISTS classes",
        "DROP TABLE IF EXISTS methods",
        "DROP TABLE IF EXISTS includes",
        "DROP TABLE IF EXISTS namespaces",
        "DROP TABLE IF EXISTS implementations",
        "DROP TABLE IF EXISTS features",
        "DROP TABLE IF EXISTS subsystems",
        "DROP TABLE IF EXISTS subsystem_deps",
        "DROP TABLE IF EXISTS class_refs",
        "DROP TABLE IF EXISTS file_summaries",
        "DROP TABLE IF EXISTS function_index",
        "DROP TABLE IF EXISTS edges",
        "DROP TABLE IF EXISTS qt_connections",
        "DROP TABLE IF EXISTS services",
        "DROP TABLE IF EXISTS test_cases",
        "DROP TABLE IF EXISTS cmake_targets",
    };
    for (const auto *sql : drops)
        q.exec(QString::fromLatin1(sql));

    // Try to drop FTS5 table (may not exist)
    q.exec(QStringLiteral("DROP TABLE IF EXISTS fts_index"));

    // ── Core tables ──

    if (!q.exec(QStringLiteral(
        "CREATE TABLE files ("
        "  id          INTEGER PRIMARY KEY,"
        "  path        TEXT UNIQUE NOT NULL,"
        "  dir         TEXT NOT NULL,"
        "  name        TEXT NOT NULL,"
        "  ext         TEXT NOT NULL,"
        "  lang        TEXT NOT NULL,"
        "  lines       INTEGER DEFAULT 0,"
        "  size_bytes  INTEGER DEFAULT 0,"
        "  has_qobject INTEGER DEFAULT 0,"
        "  has_signals INTEGER DEFAULT 0,"
        "  has_slots   INTEGER DEFAULT 0,"
        "  subsystem   TEXT DEFAULT ''"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE classes ("
        "  id           INTEGER PRIMARY KEY,"
        "  file_id      INTEGER REFERENCES files(id),"
        "  name         TEXT NOT NULL,"
        "  bases        TEXT,"
        "  line_num     INTEGER,"
        "  is_interface INTEGER DEFAULT 0,"
        "  is_struct    INTEGER DEFAULT 0,"
        "  has_impl     INTEGER DEFAULT 0,"
        "  lang         TEXT DEFAULT 'cpp'"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE methods ("
        "  id              INTEGER PRIMARY KEY,"
        "  file_id         INTEGER REFERENCES files(id),"
        "  class_id        INTEGER REFERENCES classes(id),"
        "  name            TEXT NOT NULL,"
        "  params          TEXT,"
        "  line_num        INTEGER,"
        "  is_pure_virtual INTEGER DEFAULT 0,"
        "  is_signal       INTEGER DEFAULT 0,"
        "  is_slot         INTEGER DEFAULT 0,"
        "  has_body        INTEGER DEFAULT 0,"
        "  lang            TEXT DEFAULT 'cpp'"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE includes ("
        "  id        INTEGER PRIMARY KEY,"
        "  file_id   INTEGER REFERENCES files(id),"
        "  included  TEXT NOT NULL,"
        "  is_system INTEGER DEFAULT 0"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE namespaces ("
        "  id      INTEGER PRIMARY KEY,"
        "  file_id INTEGER REFERENCES files(id),"
        "  name    TEXT NOT NULL"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE implementations ("
        "  id           INTEGER PRIMARY KEY,"
        "  header_id    INTEGER REFERENCES files(id),"
        "  source_id    INTEGER REFERENCES files(id),"
        "  class_name   TEXT,"
        "  method_count INTEGER DEFAULT 0"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE features ("
        "  id          INTEGER PRIMARY KEY,"
        "  name        TEXT UNIQUE NOT NULL,"
        "  status      TEXT DEFAULT 'unknown',"
        "  header_file TEXT,"
        "  source_file TEXT,"
        "  class_name  TEXT,"
        "  notes       TEXT"
        ")")))
        return false;

    // ── Subsystem tables ──

    if (!q.exec(QStringLiteral(
        "CREATE TABLE subsystems ("
        "  id              INTEGER PRIMARY KEY,"
        "  name            TEXT UNIQUE NOT NULL,"
        "  dir_path        TEXT NOT NULL,"
        "  file_count      INTEGER DEFAULT 0,"
        "  line_count      INTEGER DEFAULT 0,"
        "  class_count     INTEGER DEFAULT 0,"
        "  h_only_count    INTEGER DEFAULT 0,"
        "  interface_count INTEGER DEFAULT 0"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE subsystem_deps ("
        "  id             INTEGER PRIMARY KEY,"
        "  from_subsystem TEXT NOT NULL,"
        "  to_subsystem   TEXT NOT NULL,"
        "  edge_count     INTEGER DEFAULT 1,"
        "  UNIQUE(from_subsystem, to_subsystem)"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE class_refs ("
        "  id         INTEGER PRIMARY KEY,"
        "  from_class TEXT NOT NULL,"
        "  to_class   TEXT NOT NULL,"
        "  ref_type   TEXT DEFAULT 'include',"
        "  UNIQUE(from_class, to_class, ref_type)"
        ")")))
        return false;

    // ── Enhanced tables (from Python code graph) ──

    if (!q.exec(QStringLiteral(
        "CREATE TABLE file_summaries ("
        "  file_id     INTEGER PRIMARY KEY REFERENCES files(id),"
        "  summary     TEXT NOT NULL DEFAULT '',"
        "  category    TEXT DEFAULT '',"
        "  key_classes TEXT DEFAULT '',"
        "  key_exports TEXT DEFAULT ''"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE function_index ("
        "  id             INTEGER PRIMARY KEY,"
        "  file_id        INTEGER REFERENCES files(id),"
        "  name           TEXT NOT NULL,"
        "  qualified_name TEXT DEFAULT '',"
        "  params         TEXT DEFAULT '',"
        "  return_type    TEXT DEFAULT '',"
        "  start_line     INTEGER NOT NULL,"
        "  end_line       INTEGER NOT NULL,"
        "  line_count     INTEGER DEFAULT 0,"
        "  is_method      INTEGER DEFAULT 0,"
        "  class_name     TEXT DEFAULT ''"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE edges ("
        "  id          INTEGER PRIMARY KEY,"
        "  source_file INTEGER REFERENCES files(id),"
        "  target_file INTEGER,"
        "  source_name TEXT DEFAULT '',"
        "  target_name TEXT DEFAULT '',"
        "  edge_type   TEXT NOT NULL,"
        "  line_num    INTEGER DEFAULT 0,"
        "  metadata    TEXT DEFAULT ''"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE qt_connections ("
        "  id          INTEGER PRIMARY KEY,"
        "  file_id     INTEGER REFERENCES files(id),"
        "  sender      TEXT NOT NULL,"
        "  signal_name TEXT NOT NULL,"
        "  receiver    TEXT NOT NULL,"
        "  slot_name   TEXT NOT NULL,"
        "  line_num    INTEGER DEFAULT 0"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE services ("
        "  id          INTEGER PRIMARY KEY,"
        "  file_id     INTEGER REFERENCES files(id),"
        "  service_key TEXT NOT NULL,"
        "  class_name  TEXT DEFAULT '',"
        "  line_num    INTEGER DEFAULT 0,"
        "  reg_type    TEXT DEFAULT 'register'"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE test_cases ("
        "  id          INTEGER PRIMARY KEY,"
        "  file_id     INTEGER REFERENCES files(id),"
        "  test_class  TEXT NOT NULL,"
        "  test_method TEXT NOT NULL,"
        "  line_num    INTEGER DEFAULT 0"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE cmake_targets ("
        "  id          INTEGER PRIMARY KEY,"
        "  file_id     INTEGER REFERENCES files(id),"
        "  target_name TEXT NOT NULL,"
        "  target_type TEXT NOT NULL,"
        "  line_num    INTEGER DEFAULT 0"
        ")")))
        return false;

    // ── Indexes ──

    static const char *const indexes[] = {
        "CREATE INDEX idx_files_path       ON files(path)",
        "CREATE INDEX idx_files_dir        ON files(dir)",
        "CREATE INDEX idx_files_subsystem  ON files(subsystem)",
        "CREATE INDEX idx_classes_name     ON classes(name)",
        "CREATE INDEX idx_classes_file     ON classes(file_id)",
        "CREATE INDEX idx_methods_class    ON methods(class_id)",
        "CREATE INDEX idx_methods_name     ON methods(name)",
        "CREATE INDEX idx_includes_file    ON includes(file_id)",
        "CREATE INDEX idx_includes_incl    ON includes(included)",
        "CREATE INDEX idx_subdeps_from     ON subsystem_deps(from_subsystem)",
        "CREATE INDEX idx_classrefs_from   ON class_refs(from_class)",
        "CREATE INDEX idx_funcidx_file     ON function_index(file_id)",
        "CREATE INDEX idx_funcidx_name     ON function_index(name)",
        "CREATE INDEX idx_funcidx_class    ON function_index(class_name)",
        "CREATE INDEX idx_edges_source     ON edges(source_file)",
        "CREATE INDEX idx_edges_target     ON edges(target_file)",
        "CREATE INDEX idx_edges_type       ON edges(edge_type)",
        "CREATE INDEX idx_qtconn_file      ON qt_connections(file_id)",
        "CREATE INDEX idx_services_key     ON services(service_key)",
        "CREATE INDEX idx_testcases_file   ON test_cases(file_id)",
        "CREATE INDEX idx_cmake_file       ON cmake_targets(file_id)",
        "CREATE INDEX idx_filesumm_cat     ON file_summaries(category)",
    };
    for (const auto *sql : indexes)
        q.exec(QString::fromLatin1(sql));

    // ── FTS5 virtual table (optional) ──

    m_hasFts5 = q.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE fts_index USING fts5("
        "  name, qualified_name, file_path, kind, summary,"
        "  tokenize='unicode61'"
        ")"));

    return true;
}
