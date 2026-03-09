#include "syntaxhighlighter.h"
#include "languages/langbuilders.h"

#include <QFileInfo>
#include <QHash>

// ── Language entry ───────────────────────────────────────────────────────────

struct LangEntry
{
    LangBuildFn build;
    bool        hasBlockComments;
};

// ── Extension-based lookup table ─────────────────────────────────────────────

static const QHash<QString, LangEntry> &extensionMap()
{
    static const QHash<QString, LangEntry> m = {
        // C / C++
        {"cpp", {buildCpp, true}}, {"cxx", {buildCpp, true}},
        {"cc",  {buildCpp, true}}, {"c",   {buildCpp, true}},
        {"h",   {buildCpp, true}}, {"hpp", {buildCpp, true}},
        {"hxx", {buildCpp, true}}, {"inl", {buildCpp, true}},
        {"mm",  {buildCpp, true}},
        // C#
        {"cs", {buildCsharp, true}},
        // Python
        {"py",  {buildPython, true}}, {"pyw", {buildPython, true}},
        // JavaScript
        {"js",  {buildJavaScript, true}}, {"mjs", {buildJavaScript, true}},
        {"cjs", {buildJavaScript, true}}, {"jsx", {buildJavaScript, true}},
        // TypeScript
        {"ts",  {buildTypeScript, true}}, {"tsx", {buildTypeScript, true}},
        // JSON
        {"json", {buildJson, false}}, {"jsonc", {buildJson, false}},
        // CMake (by extension)
        {"cmake", {buildCmake, false}},
        // Markdown
        {"md", {buildMarkdown, false}}, {"markdown", {buildMarkdown, false}},
        // Java
        {"java", {buildJava, true}},
        // Dart
        {"dart", {buildDart, true}},
        // XML / HTML
        {"xml",  {buildXml, true}}, {"html",  {buildXml, true}},
        {"htm",  {buildXml, true}}, {"svg",   {buildXml, true}},
        {"xsl",  {buildXml, true}}, {"xslt",  {buildXml, true}},
        {"xaml", {buildXml, true}}, {"plist", {buildXml, true}},
        {"ui",   {buildXml, true}}, {"resx",  {buildXml, true}},
        {"csproj", {buildXml, true}}, {"vbproj",  {buildXml, true}},
        {"fsproj", {buildXml, true}}, {"props",   {buildXml, true}},
        {"targets",{buildXml, true}}, {"config",  {buildXml, true}},
        {"manifest", {buildXml, true}},
        // Solution
        {"sln", {buildSln, false}},
        // QMake
        {"pro", {buildQmake, false}}, {"pri", {buildQmake, false}},
        {"prf", {buildQmake, false}},
        // SQL
        {"sql", {buildSql, false}}, {"ddl", {buildSql, false}},
        {"dml", {buildSql, false}},
        // PHP
        {"php",  {buildPhp, true}}, {"php3",  {buildPhp, true}},
        {"php4", {buildPhp, true}}, {"phtml", {buildPhp, true}},
        // Rust
        {"rs", {buildRust, true}},
        // Go
        {"go", {buildGo, true}},
        // Kotlin
        {"kt", {buildKotlin, true}}, {"kts", {buildKotlin, true}},
        // Swift
        {"swift", {buildSwift, true}},
        // Shell
        {"sh",   {buildShell, false}}, {"bash", {buildShell, false}},
        {"zsh",  {buildShell, false}}, {"ksh",  {buildShell, false}},
        {"fish", {buildShell, false}},
        // PowerShell
        {"ps1",  {buildPowerShell, true}}, {"psm1", {buildPowerShell, true}},
        {"psd1", {buildPowerShell, true}},
        // YAML
        {"yaml", {buildYaml, false}}, {"yml", {buildYaml, false}},
        // TOML
        {"toml", {buildToml, false}},
        // INI / config (note: .env is separate — see below)
        {"ini",  {buildIni, false}}, {"conf",  {buildIni, false}},
        {"cfg",  {buildIni, false}}, {"properties", {buildIni, false}},
        // Environment files
        {"env", {buildEnv, false}},
        // Lua
        {"lua", {buildLua, true}},
        // Ruby
        {"rb", {buildRuby, true}}, {"rake", {buildRuby, true}},
        {"gemspec", {buildRuby, true}},
        // GLSL / HLSL
        {"glsl", {buildGlsl, true}}, {"vert", {buildGlsl, true}},
        {"frag", {buildGlsl, true}}, {"geom", {buildGlsl, true}},
        {"comp", {buildGlsl, true}}, {"tesc", {buildGlsl, true}},
        {"tese", {buildGlsl, true}}, {"hlsl", {buildGlsl, true}},
        {"fx",   {buildGlsl, true}},
        // Scala
        {"scala", {buildScala, true}}, {"sc", {buildScala, true}},
        // Batch
        {"bat", {buildBatch, false}}, {"cmd", {buildBatch, false}},
        // GraphQL
        {"graphql", {buildGraphql, true}}, {"gql", {buildGraphql, true}},
        // Terraform / HCL
        {"tf", {buildTerraform, true}}, {"tfvars", {buildTerraform, true}},
        {"hcl", {buildTerraform, true}},
        // CSS / SCSS / LESS
        {"css",  {buildCss, true}}, {"scss", {buildCss, true}},
        {"sass", {buildCss, true}}, {"less", {buildCss, true}},
        // Perl
        {"pl", {buildPerl, false}}, {"pm", {buildPerl, false}},
        {"t",  {buildPerl, false}}, {"pod", {buildPerl, false}},
        // R
        {"r", {buildR, false}}, {"rmd", {buildR, false}},
        // Haskell
        {"hs", {buildHaskell, true}}, {"lhs", {buildHaskell, true}},
        // Elixir
        {"ex",   {buildElixir, false}}, {"exs",  {buildElixir, false}},
        {"heex", {buildElixir, false}}, {"leex", {buildElixir, false}},
        // Zig
        {"zig", {buildZig, false}}, {"zon", {buildZig, false}},
        // Protobuf
        {"proto", {buildProtobuf, true}}, {"proto3", {buildProtobuf, true}},
        // Makefile by extension
        {"mk", {buildMakefile, false}},
        // Dockerfile by extension
        {"dockerfile", {buildDockerfile, false}},
    };
    return m;
}

// ── Filename-based lookup ────────────────────────────────────────────────────

static const LangEntry *findByName(const QString &name)
{
    static const LangEntry cmake      = {buildCmake, false};
    static const LangEntry makefile   = {buildMakefile, false};
    static const LangEntry dockerfile = {buildDockerfile, false};
    static const LangEntry ruby       = {buildRuby, true};
    static const LangEntry shell      = {buildShell, false};
    static const LangEntry env        = {buildEnv, false};
    static const LangEntry ini        = {buildIni, false};

    if (name == QLatin1String("cmakelists.txt"))                           return &cmake;
    if (name == QLatin1String("makefile") ||
        name == QLatin1String("gnumakefile"))                              return &makefile;
    if (name == QLatin1String("dockerfile") ||
        name.startsWith(QLatin1String("dockerfile.")))                     return &dockerfile;
    if (name == QLatin1String("gemfile") ||
        name == QLatin1String("rakefile") ||
        name == QLatin1String("guardfile"))                                return &ruby;
    if (name == QLatin1String(".bashrc") ||
        name == QLatin1String(".zshrc") ||
        name == QLatin1String(".profile") ||
        name == QLatin1String(".bash_profile"))                            return &shell;
    if (name == QLatin1String(".env"))                                      return &env;
    if (name == QLatin1String(".gitconfig") ||
        name == QLatin1String(".editorconfig"))                            return &ini;
    return nullptr;
}

// ── Shebang-based language detection ─────────────────────────────────────────

static const LangEntry *findByShebang(QTextDocument *doc)
{
    if (!doc || doc->blockCount() < 1)
        return nullptr;
    const QString first = doc->firstBlock().text();
    if (!first.startsWith(QLatin1String("#!")))
        return nullptr;

    static const LangEntry python = {buildPython, true};
    static const LangEntry js     = {buildJavaScript, true};
    static const LangEntry shell  = {buildShell, false};
    static const LangEntry ruby   = {buildRuby, true};
    static const LangEntry perl   = {buildPerl, false};
    static const LangEntry lua    = {buildLua, true};
    static const LangEntry elixir = {buildElixir, false};

    if (first.contains(QLatin1String("python"))) return &python;
    if (first.contains(QLatin1String("node")))   return &js;
    if (first.contains(QLatin1String("bash")) ||
        first.contains(QLatin1String("/sh")))    return &shell;
    if (first.contains(QLatin1String("ruby")))   return &ruby;
    if (first.contains(QLatin1String("perl")))   return &perl;
    if (first.contains(QLatin1String("lua")))    return &lua;
    if (first.contains(QLatin1String("elixir"))) return &elixir;
    return nullptr;
}

// ── SyntaxHighlighter ────────────────────────────────────────────────────────

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *doc)
    : QSyntaxHighlighter(doc)
{
}

SyntaxHighlighter *SyntaxHighlighter::create(const QString &filePath, QTextDocument *doc)
{
    const QString ext  = QFileInfo(filePath).suffix().toLower();
    const QString name = QFileInfo(filePath).fileName().toLower();

    // 1) Try exact filename match
    const LangEntry *entry = findByName(name);

    // 2) Try file extension
    if (!entry) {
        auto it = extensionMap().constFind(ext);
        if (it != extensionMap().constEnd())
            entry = &it.value();
    }

    // 3) Try shebang line
    if (!entry)
        entry = findByShebang(doc);

    if (!entry)
        return nullptr;

    auto *h = new SyntaxHighlighter(doc);
    entry->build(h->m_rules, h->m_blockCommentStart,
                 h->m_blockCommentEnd, h->m_blockCommentFmt);
    h->m_hasBlockComments = entry->hasBlockComments;
    return h;
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    // ── Single-line rules ────────────────────────────────────────────────
    for (const Rule &rule : std::as_const(m_rules)) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }

    // ── Multi-line block comments (state 0 = normal, 1 = inside block) ──
    if (!m_hasBlockComments)
        return;

    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = static_cast<int>(
            m_blockCommentStart.match(text).capturedStart());

    while (startIndex >= 0) {
        const QRegularExpressionMatch endMatch =
            m_blockCommentEnd.match(text, startIndex + 2);

        int endIndex = static_cast<int>(endMatch.capturedStart());
        int commentLen;
        if (endIndex < 0) {
            setCurrentBlockState(1);
            commentLen = text.length() - startIndex;
        } else {
            commentLen = endIndex - startIndex + static_cast<int>(endMatch.capturedLength());
        }
        setFormat(startIndex, commentLen, m_blockCommentFmt);

        const QRegularExpressionMatch nextStart =
            m_blockCommentStart.match(text, startIndex + commentLen);
        startIndex = static_cast<int>(nextStart.capturedStart());
    }
}