#pragma once
/// Test helper: registers all built-in language contributions so that
/// SyntaxHighlighter::create() works in unit tests without the lang-pack plugin.
/// Include this in test files that test SyntaxHighlighter / HighlighterFactory.
#include "editor/languages/langbuilders.h"
#include "editor/syntaxhighlighter.h"

inline void registerAllTestLanguages()
{
    using C = SyntaxHighlighter::LanguageContribution;
    SyntaxHighlighter::registerLanguage(C{{"cpp","cxx","cc","c","h","hpp","hxx","inl","mm"},{},{},true,&buildCpp});
    SyntaxHighlighter::registerLanguage(C{{"cs"},{},{},true,&buildCsharp});
    SyntaxHighlighter::registerLanguage(C{{"py","pyw"},{},{"python"},true,&buildPython});
    SyntaxHighlighter::registerLanguage(C{{"js","mjs","cjs","jsx"},{},{"node"},true,&buildJavaScript});
    SyntaxHighlighter::registerLanguage(C{{"ts","tsx"},{},{},true,&buildTypeScript});
    SyntaxHighlighter::registerLanguage(C{{"json","jsonc"},{},{},false,&buildJson});
    SyntaxHighlighter::registerLanguage(C{{"xml","html","htm","svg","xsl","xslt","xaml","plist","ui","resx","csproj","vbproj","fsproj","props","targets","config","manifest"},{},{},true,&buildXml});
    SyntaxHighlighter::registerLanguage(C{{"css","scss","sass","less"},{},{},true,&buildCss});
    SyntaxHighlighter::registerLanguage(C{{"php","php3","php4","phtml"},{},{},true,&buildPhp});
    SyntaxHighlighter::registerLanguage(C{{"java"},{},{},true,&buildJava});
    SyntaxHighlighter::registerLanguage(C{{"kt","kts"},{},{},true,&buildKotlin});
    SyntaxHighlighter::registerLanguage(C{{"scala","sc"},{},{},true,&buildScala});
    SyntaxHighlighter::registerLanguage(C{{"dart"},{},{},true,&buildDart});
    SyntaxHighlighter::registerLanguage(C{{"rs"},{},{},true,&buildRust});
    SyntaxHighlighter::registerLanguage(C{{"go"},{},{},true,&buildGo});
    SyntaxHighlighter::registerLanguage(C{{"zig","zon"},{},{},false,&buildZig});
    SyntaxHighlighter::registerLanguage(C{{"swift"},{},{},true,&buildSwift});
    SyntaxHighlighter::registerLanguage(C{{"lua"},{},{"lua"},true,&buildLua});
    SyntaxHighlighter::registerLanguage(C{{"rb","rake","gemspec"},{"gemfile","rakefile","guardfile"},{"ruby"},true,&buildRuby});
    SyntaxHighlighter::registerLanguage(C{{"pl","pm","t","pod"},{},{"perl"},false,&buildPerl});
    SyntaxHighlighter::registerLanguage(C{{"r","rmd"},{},{},false,&buildR});
    SyntaxHighlighter::registerLanguage(C{{"ex","exs","heex","leex"},{},{"elixir"},false,&buildElixir});
    SyntaxHighlighter::registerLanguage(C{{"hs","lhs"},{},{},true,&buildHaskell});
    SyntaxHighlighter::registerLanguage(C{{"sh","bash","zsh","ksh","fish"},{".bashrc",".zshrc",".profile",".bash_profile"},{"bash","/sh"},false,&buildShell});
    SyntaxHighlighter::registerLanguage(C{{"ps1","psm1","psd1"},{},{},true,&buildPowerShell});
    SyntaxHighlighter::registerLanguage(C{{"bat","cmd"},{},{},false,&buildBatch});
    SyntaxHighlighter::registerLanguage(C{{"yaml","yml"},{},{},false,&buildYaml});
    SyntaxHighlighter::registerLanguage(C{{"toml"},{},{},false,&buildToml});
    SyntaxHighlighter::registerLanguage(C{{"ini","conf","cfg","properties"},{".gitconfig",".editorconfig"},{},false,&buildIni});
    SyntaxHighlighter::registerLanguage(C{{"env"},{".env"},{},false,&buildEnv});
    SyntaxHighlighter::registerLanguage(C{{"sql","ddl","dml"},{},{},false,&buildSql});
    SyntaxHighlighter::registerLanguage(C{{"cmake"},{"cmakelists.txt"},{},false,&buildCmake});
    SyntaxHighlighter::registerLanguage(C{{"mk"},{"makefile","gnumakefile"},{},false,&buildMakefile});
    SyntaxHighlighter::registerLanguage(C{{"dockerfile"},{"dockerfile"},{},false,&buildDockerfile});
    SyntaxHighlighter::registerLanguage(C{{"pro","pri","prf"},{},{},false,&buildQmake});
    SyntaxHighlighter::registerLanguage(C{{"sln"},{},{},false,&buildSln});
    SyntaxHighlighter::registerLanguage(C{{"glsl","vert","frag","geom","comp","tesc","tese","hlsl","fx"},{},{},true,&buildGlsl});
    SyntaxHighlighter::registerLanguage(C{{"md","markdown"},{},{},false,&buildMarkdown});
    SyntaxHighlighter::registerLanguage(C{{"graphql","gql"},{},{},true,&buildGraphql});
    SyntaxHighlighter::registerLanguage(C{{"proto","proto3"},{},{},true,&buildProtobuf});
    SyntaxHighlighter::registerLanguage(C{{"tf","tfvars","hcl"},{},{},true,&buildTerraform});
}
