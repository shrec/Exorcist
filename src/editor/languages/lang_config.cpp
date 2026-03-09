#include "langcommon.h"

void buildCmake(QVector<SyntaxHighlighter::Rule> &rules,
                QRegularExpression &blockStart,
                QRegularExpression &blockEnd,
                QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"add_executable", "add_library", "add_subdirectory",
                 "cmake_minimum_required", "find_package", "if", "elseif",
                 "else", "endif", "foreach", "endforeach", "function",
                 "endfunction", "macro", "endmacro", "include", "message",
                 "option", "project", "set", "string", "target_compile_definitions",
                 "target_compile_options", "target_include_directories",
                 "target_link_libraries", "target_sources", "while",
                 "endwhile", "return", "list", "file", "get_filename_component",
                 "get_target_property", "add_custom_command",
                 "add_custom_target", "install", "configure_file",
                 "find_path", "find_library", "find_program"},
                clr::keyword);

    addRule(rules, R"(\$\{[^}]*\})", clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")", clr::string_);
    addRule(rules, R"(#[^\n]*)", clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildMakefile(QVector<SyntaxHighlighter::Rule> &rules,
                   QRegularExpression &blockStart,
                   QRegularExpression &blockEnd,
                   QTextCharFormat &blockFmt)
{
    addRule(rules, R"(^[A-Za-z_.][A-Za-z0-9_.\-/ ]*\s*:(?!=))", clr::keyword);
    addRule(rules, R"(\$[\(\{][A-Za-z_][A-Za-z0-9_]*[\)\}])",  clr::special_);
    addRule(rules, R"(\$[@<^%?*+|])",                           clr::special_);
    addKeywords(rules,
                {"include", "define", "endef", "ifdef", "ifndef",
                 "ifeq", "ifneq", "else", "endif", "export", "unexport",
                 "override", "private", "vpath", "undefine"},
                clr::preproc_);
    addRule(rules, R"(\$\((?:call|subst|patsubst|strip|findstring|filter|sort|word|wordlist|firstword|lastword|dir|notdir|suffix|basename|addsuffix|addprefix|join|wildcard|realpath|abspath|foreach|if|or|and|origin|flavor|value|eval|file|info|warning|error)\b)",
            clr::function_);
    addRule(rules, R"(#[^\n]*)", clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildDockerfile(QVector<SyntaxHighlighter::Rule> &rules,
                     QRegularExpression &blockStart,
                     QRegularExpression &blockEnd,
                     QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"FROM", "RUN", "CMD", "LABEL", "EXPOSE", "ENV", "ADD",
                 "COPY", "ENTRYPOINT", "VOLUME", "USER", "WORKDIR",
                 "ARG", "ONBUILD", "STOPSIGNAL", "HEALTHCHECK", "SHELL"},
                clr::keyword);
    addRule(rules, R"(\$\{?[A-Za-z_][A-Za-z0-9_]*\}?)", clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",              clr::string_);
    addRule(rules, R"('[^']*')",                        clr::string_);
    addRule(rules, R"(#[^\n]*)",                        clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildQmake(QVector<SyntaxHighlighter::Rule> &rules,
                QRegularExpression &blockStart,
                QRegularExpression &blockEnd,
                QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"SOURCES", "HEADERS", "FORMS", "RESOURCES",
                 "QT", "CONFIG", "LIBS", "INCLUDEPATH", "DEFINES",
                 "TARGET", "TEMPLATE", "DESTDIR", "OBJECTS_DIR",
                 "MOC_DIR", "UI_DIR", "RC_ICONS", "RC_FILE",
                 "VERSION", "QMAKE_CXXFLAGS", "QMAKE_LFLAGS",
                 "TRANSLATIONS", "QMAKE_INFO_PLIST",
                 "SUBDIRS", "DEPENDPATH", "VPATH",
                 "QMAKE_TARGET_BUNDLE_PREFIX", "QMAKE_APPLE_DEVICE_ARCHS"},
                clr::keyword);
    addRule(rules, R"(\b(win32|unix|macx|linux|android|ios|wasm|msvc|gcc|clang|debug|release)\b(?=\s*[:{(]))",
            clr::type_);
    addRule(rules, R"(\$\$[\w\[\]{}()]+|\$\$\([^)]*\))", clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")", clr::string_);
    addRule(rules, R"([+\-*]?=)", clr::preproc_);
    addRule(rules, R"(\\$)",      clr::preproc_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)", clr::number_);
    addRule(rules, R"(#[^\n]*)", clr::comment_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildSln(QVector<SyntaxHighlighter::Rule> &rules,
              QRegularExpression &blockStart,
              QRegularExpression &blockEnd,
              QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"Project", "EndProject", "Global", "EndGlobal",
                 "GlobalSection", "EndGlobalSection",
                 "ProjectSection", "EndProjectSection"},
                clr::keyword);
    addRule(rules, R"(\{[0-9A-Fa-f\-]{36}\})", clr::special_);
    addRule(rules, R"(\([A-Za-z][A-Za-z0-9]*\))", clr::type_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")", clr::string_);
    addRule(rules, R"(^#[^\n]*)", clr::comment_);
    addRule(rules, R"(\b\d+\.\d+(?:\.\d+)*\b)", clr::number_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildGlsl(QVector<SyntaxHighlighter::Rule> &rules,
               QRegularExpression &blockStart,
               QRegularExpression &blockEnd,
               QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"attribute", "break", "bvec2", "bvec3", "bvec4", "case",
                 "centroid", "coherent", "const", "continue", "default",
                 "discard", "do", "double", "dvec2", "dvec3", "dvec4",
                 "else", "flat", "float", "for", "highp", "if", "in",
                 "inout", "int", "invariant", "ivec2", "ivec3", "ivec4",
                 "layout", "lowp", "mat2", "mat3", "mat4", "mat2x2",
                 "mat2x3", "mat2x4", "mat3x2", "mat3x3", "mat3x4",
                 "mat4x2", "mat4x3", "mat4x4", "mediump", "noperspective",
                 "out", "patch", "precise", "precision", "readonly",
                 "restrict", "return", "sample", "sampler2D", "sampler3D",
                 "samplerCube", "samplerCubeShadow", "sampler2DShadow",
                 "smooth", "struct", "subroutine", "switch", "true",
                 "false", "uint", "uniform", "uvec2", "uvec3", "uvec4",
                 "varying", "vec2", "vec3", "vec4", "void", "volatile",
                 "while", "writeonly"},
                clr::keyword);
    addKeywords(rules,
                {"gl_Position", "gl_FragCoord", "gl_FragDepth",
                 "gl_VertexID", "gl_InstanceID", "gl_PointSize",
                 "gl_ClipDistance", "gl_FrontFacing", "gl_PointCoord",
                 "gl_Layer", "gl_ViewportIndex"},
                clr::special_);
    addRule(rules, R"(^\s*#\s*\w+.*)", clr::preproc_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",              clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?[uUfF]?\b)", clr::number_);
    addRule(rules, R"(//[^\n]*)",                       clr::comment_);
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

void buildMarkdown(QVector<SyntaxHighlighter::Rule> &rules,
                   QRegularExpression &blockStart,
                   QRegularExpression &blockEnd,
                   QTextCharFormat &blockFmt)
{
    addRule(rules, R"(^#{1,6}\s.*$)", clr::keyword);
    addRule(rules, R"(\*\*[^*]+\*\*|__[^_]+__)", clr::type_);
    addRule(rules, R"(\*[^*]+\*|_[^_]+_)", clr::function_);
    addRule(rules, R"(`[^`]+`)", clr::string_);
    addRule(rules, R"(\[[^\]]*\]\([^)]*\))", clr::special_);
    addRule(rules, R"(^>\s.*$)", clr::comment_);
    addRule(rules, R"(^[-*_]{3,}\s*$)", clr::preproc_);
    addRule(rules, R"(^\s*[-*+]\s)", clr::preproc_);
    addRule(rules, R"(^\s*[0-9]+\.\s)", clr::preproc_);

    Q_UNUSED(blockStart);
    Q_UNUSED(blockEnd);
    Q_UNUSED(blockFmt);
}

void buildGraphql(QVector<SyntaxHighlighter::Rule> &rules,
                  QRegularExpression &blockStart,
                  QRegularExpression &blockEnd,
                  QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"directive", "enum", "extend", "fragment", "implements",
                 "input", "interface", "mutation", "on", "query",
                 "repeatable", "scalar", "schema", "subscription",
                 "type", "union"},
                clr::keyword);
    addKeywords(rules,
                {"Boolean", "Float", "ID", "Int", "String",
                 "true", "false", "null"},
                clr::type_);
    addRule(rules, R"(@[A-Za-z_][A-Za-z0-9_]*)",      clr::preproc_);
    addRule(rules, R"(\$[A-Za-z_][A-Za-z0-9_]*)",     clr::special_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",             clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)",       clr::number_);
    addRule(rules, R"(#[^\n]*)",                       clr::comment_);
    blockStart = QRegularExpression(R"(""")");
    blockEnd   = QRegularExpression(R"(""")");
    blockFmt   = clr::string_;
}

void buildProtobuf(QVector<SyntaxHighlighter::Rule> &rules,
                   QRegularExpression &blockStart,
                   QRegularExpression &blockEnd,
                   QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"syntax", "package", "import", "option", "message", "enum",
                 "service", "rpc", "returns", "stream", "oneof", "map",
                 "reserved", "extensions", "extend", "optional", "required",
                 "repeated", "public", "weak", "true", "false", "to", "max"},
                clr::keyword);
    addKeywords(rules,
                {"double", "float", "int32", "int64", "uint32", "uint64",
                 "sint32", "sint64", "fixed32", "fixed64",
                 "sfixed32", "sfixed64", "bool", "string", "bytes",
                 "google", "protobuf", "Any", "Timestamp", "Duration"},
                clr::type_);
    addRule(rules, R"(\b[A-Z][a-zA-Z0-9_]*)", clr::type_);
    addRule(rules, R"("(?:\\.|[^"\\])*")", clr::string_);
    addRule(rules, R"('(?:\\.|[^'\\])*')", clr::string_);
    addRule(rules, R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b)", clr::number_);
    addRule(rules, R"(\b0[xX][0-9a-fA-F]+\b)", clr::number_);
    addRule(rules, R"(=\s*\d+)", clr::number_);
    addRule(rules, R"(//[^\n]*)", clr::comment_);

    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}

void buildTerraform(QVector<SyntaxHighlighter::Rule> &rules,
                    QRegularExpression &blockStart,
                    QRegularExpression &blockEnd,
                    QTextCharFormat &blockFmt)
{
    addKeywords(rules,
                {"resource", "data", "provider", "variable", "output",
                 "module", "terraform", "locals", "dynamic", "for_each",
                 "count", "depends_on", "lifecycle", "provisioner",
                 "connection", "backend", "required_providers",
                 "required_version"},
                clr::keyword);
    addKeywords(rules, {"true", "false", "null"}, clr::type_);
    addRule(rules, R"(\$\{[^}]*\})",              clr::special_);
    addRule(rules, R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*\()", clr::function_);
    addRule(rules, R"(<<-?\s*[A-Z_]+)",          clr::preproc_);
    addRule(rules, R"("(?:[^"\\]|\\.)*")",        clr::string_);
    addRule(rules, R"(\b[0-9]+(?:\.[0-9]+)?\b)",  clr::number_);
    addRule(rules, R"(#[^\n]*)",                  clr::comment_);
    blockStart = QRegularExpression(R"(/\*)");
    blockEnd   = QRegularExpression(R"(\*/)");
    blockFmt   = clr::comment_;
}
