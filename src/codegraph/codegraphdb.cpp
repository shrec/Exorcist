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
        "DROP TABLE IF EXISTS call_graph",
        "DROP TABLE IF EXISTS xml_bindings",
        "DROP TABLE IF EXISTS config_issues",
        "DROP TABLE IF EXISTS runtime_entrypoints",
        "DROP TABLE IF EXISTS symbol_aliases",
        "DROP TABLE IF EXISTS reachability",
        "DROP TABLE IF EXISTS hotspot_scores",
        "DROP TABLE IF EXISTS semantic_tags",
        "DROP TABLE IF EXISTS function_summary",
        "DROP TABLE IF EXISTS symbol_slices",
        "DROP TABLE IF EXISTS optimization_patterns",
        "DROP TABLE IF EXISTS git_delta",
        "DROP TABLE IF EXISTS analysis_scores",
        "DROP TABLE IF EXISTS ai_tasks",
        "DROP VIEW  IF EXISTS v_bottleneck_queue",
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

    // ── Analysis tables (mirrors Python steps 13-18) ──

    if (!q.exec(QStringLiteral(
        "CREATE TABLE call_graph ("
        "  id             INTEGER PRIMARY KEY,"
        "  caller_file_id INTEGER REFERENCES files(id),"
        "  caller_func    TEXT NOT NULL,"
        "  callee_func    TEXT NOT NULL,"
        "  callee_file_id INTEGER REFERENCES files(id),"
        "  line_num       INTEGER DEFAULT 0,"
        "  UNIQUE(caller_file_id, caller_func, callee_func)"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE xml_bindings ("
        "  id             INTEGER PRIMARY KEY,"
        "  xml_file_id    INTEGER REFERENCES files(id),"
        "  element        TEXT NOT NULL,"
        "  attribute      TEXT NOT NULL,"
        "  value          TEXT NOT NULL,"
        "  bound_symbol   TEXT DEFAULT '',"
        "  bound_file_id  INTEGER REFERENCES files(id),"
        "  binding_type   TEXT DEFAULT 'unresolved'"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE config_issues ("
        "  id          INTEGER PRIMARY KEY,"
        "  xml_file_id INTEGER REFERENCES files(id),"
        "  issue_type  TEXT NOT NULL,"
        "  element     TEXT DEFAULT '',"
        "  attribute   TEXT DEFAULT '',"
        "  value       TEXT DEFAULT '',"
        "  message     TEXT DEFAULT '',"
        "  severity    TEXT DEFAULT 'warning'"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE runtime_entrypoints ("
        "  id          INTEGER PRIMARY KEY,"
        "  file_id     INTEGER REFERENCES files(id),"
        "  loader_type TEXT NOT NULL,"
        "  target      TEXT NOT NULL,"
        "  line_num    INTEGER DEFAULT 0"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE symbol_aliases ("
        "  id           INTEGER PRIMARY KEY,"
        "  symbol_a     TEXT NOT NULL,"
        "  file_a       INTEGER REFERENCES files(id),"
        "  symbol_b     TEXT NOT NULL,"
        "  file_b       INTEGER REFERENCES files(id),"
        "  edit_distance INTEGER DEFAULT 0,"
        "  alias_type   TEXT DEFAULT 'alias',"
        "  UNIQUE(symbol_a, symbol_b)"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE reachability ("
        "  id            INTEGER PRIMARY KEY,"
        "  file_id       INTEGER REFERENCES files(id),"
        "  symbol        TEXT NOT NULL,"
        "  symbol_type   TEXT DEFAULT 'function',"
        "  is_reachable  INTEGER DEFAULT 0,"
        "  reachable_via TEXT DEFAULT '',"
        "  dead_reason   TEXT DEFAULT ''"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE hotspot_scores ("
        "  id                INTEGER PRIMARY KEY,"
        "  file_id           INTEGER UNIQUE REFERENCES files(id),"
        "  coupling_in       INTEGER DEFAULT 0,"
        "  coupling_out      INTEGER DEFAULT 0,"
        "  has_tests         INTEGER DEFAULT 0,"
        "  crash_indicators  INTEGER DEFAULT 0,"
        "  hotspot_score     REAL DEFAULT 0.0,"
        "  risk_factors      TEXT DEFAULT ''"
        ")")))
        return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE semantic_tags ("
        "  id          INTEGER PRIMARY KEY,"
        "  file_id     INTEGER REFERENCES files(id),"
        "  entity_type TEXT NOT NULL,"
        "  entity_name TEXT NOT NULL,"
        "  line_num    INTEGER DEFAULT 0,"
        "  tag         TEXT NOT NULL,"
        "  tag_value   TEXT DEFAULT '',"
        "  confidence  INTEGER DEFAULT 100,"
        "  source      TEXT DEFAULT 'heuristic',"
        "  evidence    TEXT DEFAULT '',"
        "  UNIQUE(file_id, entity_type, entity_name, tag, tag_value)"
        ")")))
        return false;

    // ── AI-context optimisation tables ──

    if (!q.exec(QStringLiteral(
        "CREATE TABLE function_summary ("
        "  id                INTEGER PRIMARY KEY,"
        "  file_id           INTEGER REFERENCES files(id),"
        "  symbol            TEXT NOT NULL,"
        "  qualified_symbol  TEXT DEFAULT '',"
        "  summary           TEXT DEFAULT '',"
        "  category          TEXT DEFAULT '',"
        "  batchable         INTEGER DEFAULT 0,"
        "  gpu_candidate     INTEGER DEFAULT 0,"
        "  ct_sensitive      INTEGER DEFAULT 0,"
        "  recently_modified INTEGER DEFAULT 0,"
        "  body_hash         TEXT DEFAULT '',"
        "  last_updated      TEXT DEFAULT '',"
        "  UNIQUE(file_id, symbol)"
        ")"))) return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE symbol_slices ("
        "  id                   INTEGER PRIMARY KEY,"
        "  file_id              INTEGER REFERENCES files(id),"
        "  symbol               TEXT NOT NULL,"
        "  signature            TEXT DEFAULT '',"
        "  critical_lines       TEXT DEFAULT '',"
        "  slice_token_estimate INTEGER DEFAULT 0,"
        "  full_token_estimate  INTEGER DEFAULT 0,"
        "  UNIQUE(file_id, symbol)"
        ")"))) return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE optimization_patterns ("
        "  id              INTEGER PRIMARY KEY,"
        "  pattern_name    TEXT UNIQUE NOT NULL,"
        "  description     TEXT DEFAULT '',"
        "  gain            TEXT DEFAULT 'unknown',"
        "  risk            TEXT DEFAULT 'low',"
        "  applicable_when TEXT DEFAULT '',"
        "  example_symbol  TEXT DEFAULT ''"
        ")"))) return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE git_delta ("
        "  id            INTEGER PRIMARY KEY,"
        "  file_id       INTEGER REFERENCES files(id),"
        "  symbol        TEXT DEFAULT '',"
        "  commit_hash   TEXT DEFAULT '',"
        "  changed_at    TEXT DEFAULT '',"
        "  diff_snippet  TEXT DEFAULT '',"
        "  lines_added   INTEGER DEFAULT 0,"
        "  lines_removed INTEGER DEFAULT 0"
        ")"))) return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE analysis_scores ("
        "  id                 INTEGER PRIMARY KEY,"
        "  file_id            INTEGER REFERENCES files(id),"
        "  symbol_name        TEXT NOT NULL,"
        "  file_path          TEXT NOT NULL,"
        "  hotness_score      INTEGER DEFAULT 0,"
        "  complexity_score   INTEGER DEFAULT 0,"
        "  fanin_score        INTEGER DEFAULT 0,"
        "  fanout_score       INTEGER DEFAULT 0,"
        "  optimization_score INTEGER DEFAULT 0,"
        "  gpu_score          INTEGER DEFAULT 0,"
        "  ct_risk_score      INTEGER DEFAULT 0,"
        "  audit_gap_score    INTEGER DEFAULT 0,"
        "  overall_priority   INTEGER DEFAULT 0,"
        "  reasons            TEXT DEFAULT '',"
        "  UNIQUE(file_id, symbol_name)"
        ")"))) return false;

    if (!q.exec(QStringLiteral(
        "CREATE TABLE ai_tasks ("
        "  id          INTEGER PRIMARY KEY,"
        "  file_id     INTEGER REFERENCES files(id),"
        "  task_type   TEXT NOT NULL,"
        "  symbol_name TEXT NOT NULL,"
        "  file_path   TEXT NOT NULL,"
        "  prompt      TEXT NOT NULL,"
        "  status      TEXT DEFAULT 'pending',"
        "  priority    INTEGER DEFAULT 0,"
        "  created_at  TEXT DEFAULT ''"
        ")"))) return false;

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
        "CREATE INDEX idx_callgraph_caller  ON call_graph(caller_file_id)",
        "CREATE INDEX idx_callgraph_callee  ON call_graph(callee_file_id)",
        "CREATE INDEX idx_callgraph_cfunc   ON call_graph(caller_func)",
        "CREATE INDEX idx_xmlbind_file      ON xml_bindings(xml_file_id)",
        "CREATE INDEX idx_xmlbind_type      ON xml_bindings(binding_type)",
        "CREATE INDEX idx_confissue_file    ON config_issues(xml_file_id)",
        "CREATE INDEX idx_rtentry_file      ON runtime_entrypoints(file_id)",
        "CREATE INDEX idx_aliases_syma      ON symbol_aliases(symbol_a)",
        "CREATE INDEX idx_aliases_type      ON symbol_aliases(alias_type)",
        "CREATE INDEX idx_aliases_dist      ON symbol_aliases(edit_distance)",
        "CREATE INDEX idx_reach_file        ON reachability(file_id)",
        "CREATE INDEX idx_reach_reachable   ON reachability(is_reachable)",
        "CREATE INDEX idx_hotspot_file      ON hotspot_scores(file_id)",
        "CREATE INDEX idx_hotspot_score     ON hotspot_scores(hotspot_score DESC)",
        "CREATE INDEX idx_semantic_file     ON semantic_tags(file_id)",
        "CREATE INDEX idx_semantic_tag      ON semantic_tags(tag)",
        "CREATE INDEX idx_semantic_entity   ON semantic_tags(entity_type, entity_name)",
        "CREATE INDEX idx_func_summary_file ON function_summary(file_id)",
        "CREATE INDEX idx_func_summary_sym  ON function_summary(symbol)",
        "CREATE INDEX idx_sym_slices_file   ON symbol_slices(file_id)",
        "CREATE INDEX idx_sym_slices_sym    ON symbol_slices(symbol)",
        "CREATE INDEX idx_git_delta_file    ON git_delta(file_id)",
        "CREATE INDEX idx_analysis_pri      ON analysis_scores(overall_priority DESC)",
        "CREATE INDEX idx_analysis_file     ON analysis_scores(file_id)",
        "CREATE INDEX idx_analysis_gpu      ON analysis_scores(gpu_score DESC)",
        "CREATE INDEX idx_analysis_ct       ON analysis_scores(ct_risk_score DESC)",
        "CREATE INDEX idx_ai_tasks_status   ON ai_tasks(status)",
        "CREATE INDEX idx_ai_tasks_priority ON ai_tasks(priority DESC)",
    };
    for (const auto *sql : indexes)
        q.exec(QString::fromLatin1(sql));

    // ── FTS5 virtual table (optional) ──

    m_hasFts5 = q.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE fts_index USING fts5("
        "  name, qualified_name, file_path, kind, summary,"
        "  tokenize='unicode61'"
        ")"));

    // v_bottleneck_queue view (best-effort; depends on analysis_scores)
    q.exec(QStringLiteral(
        "CREATE VIEW IF NOT EXISTS v_bottleneck_queue AS "
        "SELECT a.symbol_name, a.file_path, "
        "a.hotness_score, a.complexity_score, a.fanin_score, a.fanout_score, "
        "a.gpu_score, a.ct_risk_score, a.audit_gap_score, a.overall_priority, "
        "a.reasons, COALESCE(fs.summary,'') AS summary "
        "FROM analysis_scores a "
        "LEFT JOIN files f ON a.file_id = f.id "
        "LEFT JOIN file_summaries fs ON fs.file_id = f.id "
        "ORDER BY a.overall_priority DESC, a.hotness_score DESC"));

    return true;
}
