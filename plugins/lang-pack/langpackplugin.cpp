#include "langpackplugin.h"

#include "editor/languages/langbuilders.h"
#include "editor/syntaxhighlighter.h"
#include "plugininterface.h"

LangPackPlugin::LangPackPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo LangPackPlugin::info() const
{
    PluginInfo i;
    i.id          = QStringLiteral("org.exorcist.lang-pack");
    i.name        = QStringLiteral("Language Pack");
    i.version     = QStringLiteral("1.0.0");
    i.description = QStringLiteral("Regex syntax highlighting for 40+ languages");
    return i;
}

bool LangPackPlugin::initializePlugin()
{
    using C = SyntaxHighlighter::LanguageContribution;

    // ── C / C++ ──────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"cpp","cxx","cc","c","h","hpp","hxx","inl","mm"}, {}, {}, true, &buildCpp});

    // ── C# ───────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"cs"}, {}, {}, true, &buildCsharp});

    // ── Python ───────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"py","pyw"}, {}, {"python"}, true, &buildPython});

    // ── JavaScript ───────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"js","mjs","cjs","jsx"}, {}, {"node"}, true, &buildJavaScript});

    // ── TypeScript ───────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"ts","tsx"}, {}, {}, true, &buildTypeScript});

    // ── JSON ─────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"json","jsonc"}, {}, {}, false, &buildJson});

    // ── XML / HTML ───────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"xml","html","htm","svg","xsl","xslt","xaml","plist",
         "ui","resx","csproj","vbproj","fsproj","props",
         "targets","config","manifest"}, {}, {}, true, &buildXml});

    // ── CSS / SCSS / LESS ────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"css","scss","sass","less"}, {}, {}, true, &buildCss});

    // ── PHP ──────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"php","php3","php4","phtml"}, {}, {}, true, &buildPhp});

    // ── Java ─────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"java"}, {}, {}, true, &buildJava});

    // ── Kotlin ───────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"kt","kts"}, {}, {}, true, &buildKotlin});

    // ── Scala ────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"scala","sc"}, {}, {}, true, &buildScala});

    // ── Dart ─────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"dart"}, {}, {}, true, &buildDart});

    // ── Rust ─────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"rs"}, {}, {}, true, &buildRust});

    // ── Go ───────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"go"}, {}, {}, true, &buildGo});

    // ── Zig ──────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"zig","zon"}, {}, {}, false, &buildZig});

    // ── Swift ────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"swift"}, {}, {}, true, &buildSwift});

    // ── Lua ──────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"lua"}, {}, {"lua"}, true, &buildLua});

    // ── Ruby ─────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"rb","rake","gemspec"},
        {"gemfile","rakefile","guardfile"},
        {"ruby"}, true, &buildRuby});

    // ── Perl ─────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"pl","pm","t","pod"}, {}, {"perl"}, false, &buildPerl});

    // ── R ────────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"r","rmd"}, {}, {}, false, &buildR});

    // ── Elixir ───────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"ex","exs","heex","leex"}, {}, {"elixir"}, false, &buildElixir});

    // ── Haskell ──────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"hs","lhs"}, {}, {}, true, &buildHaskell});

    // ── Shell ────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"sh","bash","zsh","ksh","fish"},
        {".bashrc",".zshrc",".profile",".bash_profile"},
        {"bash","/sh"}, false, &buildShell});

    // ── PowerShell ───────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"ps1","psm1","psd1"}, {}, {}, true, &buildPowerShell});

    // ── Batch ────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"bat","cmd"}, {}, {}, false, &buildBatch});

    // ── YAML ─────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"yaml","yml"}, {}, {}, false, &buildYaml});

    // ── TOML ─────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"toml"}, {}, {}, false, &buildToml});

    // ── INI / config ─────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"ini","conf","cfg","properties"},
        {".gitconfig",".editorconfig"},
        {}, false, &buildIni});

    // ── ENV ──────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"env"}, {".env"}, {}, false, &buildEnv});

    // ── SQL ──────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"sql","ddl","dml"}, {}, {}, false, &buildSql});

    // ── CMake ────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"cmake"}, {"cmakelists.txt"}, {}, false, &buildCmake});

    // ── Makefile ─────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"mk"}, {"makefile","gnumakefile"}, {}, false, &buildMakefile});

    // ── Dockerfile ───────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"dockerfile"}, {"dockerfile"}, {}, false, &buildDockerfile});

    // ── QMake ────────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"pro","pri","prf"}, {}, {}, false, &buildQmake});

    // ── Visual Studio solution ────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"sln"}, {}, {}, false, &buildSln});

    // ── GLSL / HLSL ──────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"glsl","vert","frag","geom","comp","tesc","tese","hlsl","fx"},
        {}, {}, true, &buildGlsl});

    // ── Markdown ─────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"md","markdown"}, {}, {}, false, &buildMarkdown});

    // ── GraphQL ──────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"graphql","gql"}, {}, {}, true, &buildGraphql});

    // ── Protobuf ─────────────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"proto","proto3"}, {}, {}, true, &buildProtobuf});

    // ── Terraform / HCL ──────────────────────────────────────────────────────
    SyntaxHighlighter::registerLanguage(C{
        {"tf","tfvars","hcl"}, {}, {}, true, &buildTerraform});

    return true;
}

void LangPackPlugin::shutdownPlugin()
{
    // Language registrations are process-scoped static data — no cleanup needed.
}
