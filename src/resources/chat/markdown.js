// ── Markdown Renderer ────────────────────────────────────────────────────────
//
// Lightweight markdown-to-HTML renderer for Exorcist chat panel.
// Supports: paragraphs, headings, bold, italic, inline code, code blocks,
// links, images, lists, blockquotes, tables, horizontal rules.
//
// Code blocks get action buttons (Copy, Insert) wired to C++ bridge.

var MarkdownRenderer = (function() {
    'use strict';

    // Escape HTML entities
    function escapeHtml(s) {
        return s
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;');
    }

    // ── Syntax Highlighting ──────────────────────────────────────────────────
    // Lightweight regex-based highlighter producing <span class="hl-*"> tokens.
    // Covers keywords, strings, comments, numbers, decorators, types for
    // C/C++, JS/TS, Python, Rust, Go, Java, C#, Bash, JSON, HTML/CSS, SQL.

    var LANG_RULES = (function() {
        // Common token patterns reused across languages
        var cLineComment = '\\/\\/[^\\n]*';
        var cBlockComment = '\\/\\*[\\s\\S]*?\\*\\/';
        var hashComment  = '#[^\\n]*';
        var dblString = '"(?:[^"\\\\]|\\\\.)*"';
        var sglString = "'(?:[^'\\\\]|\\\\.)*'";
        var backtickStr = '`(?:[^`\\\\]|\\\\.)*`';
        var number = '\\b(?:0[xXbBoO][\\da-fA-F_]+|\\d[\\d_]*\\.?[\\d_]*(?:[eE][+-]?\\d+)?)[fFlLuU]*\\b';
        var decorator = '@\\w+';

        function kw(words) {
            return '\\b(?:' + words + ')\\b';
        }

        var cKw = 'auto|break|case|const|continue|default|do|else|enum|extern|for|goto|if|inline|register|restrict|return|sizeof|static|struct|switch|typedef|union|volatile|while';
        var cppKw = cKw + '|alignas|alignof|and|and_eq|asm|bitand|bitor|catch|class|compl|concept|consteval|constexpr|constinit|co_await|co_return|co_yield|decltype|delete|dynamic_cast|explicit|export|final|friend|module|mutable|namespace|new|noexcept|not|not_eq|operator|or|or_eq|override|private|protected|public|reinterpret_cast|requires|static_assert|static_cast|template|this|thread_local|throw|try|typeid|typename|using|virtual|xor|xor_eq';
        var cppTypes = 'void|bool|char|char8_t|char16_t|char32_t|wchar_t|short|int|long|float|double|signed|unsigned|int8_t|int16_t|int32_t|int64_t|uint8_t|uint16_t|uint32_t|uint64_t|size_t|ptrdiff_t|nullptr_t|string|vector|map|set|array|unique_ptr|shared_ptr|weak_ptr|optional|variant|tuple|pair|nullptr|true|false|NULL|std|QString|QObject|QWidget|QList|QMap|QSet|QHash|QVector|QStringList|QVariant|QByteArray';
        var cppDirective = '#\\s*(?:include|define|undef|ifdef|ifndef|if|elif|else|endif|pragma|error|warning)[^\\n]*';

        var jsKw = 'abstract|arguments|async|await|break|case|catch|class|const|continue|debugger|default|delete|do|else|enum|eval|export|extends|finally|for|from|function|get|if|implements|import|in|instanceof|interface|let|module|new|of|package|private|protected|public|return|set|static|super|switch|this|throw|try|typeof|var|void|while|with|yield';
        var jsTypes = 'true|false|null|undefined|NaN|Infinity|Array|Boolean|Date|Error|Function|JSON|Map|Math|Number|Object|Promise|Proxy|RegExp|Set|String|Symbol|WeakMap|WeakSet|console|window|document|globalThis';

        var pyKw = 'False|None|True|and|as|assert|async|await|break|class|continue|def|del|elif|else|except|finally|for|from|global|if|import|in|is|lambda|nonlocal|not|or|pass|raise|return|try|while|with|yield';
        var pyTypes = 'int|float|str|bool|list|dict|tuple|set|frozenset|bytes|bytearray|memoryview|range|complex|type|object|Exception|print|len|super|self|cls|__init__|__name__|__main__';

        var rustKw = 'as|async|await|break|const|continue|crate|dyn|else|enum|extern|fn|for|if|impl|in|let|loop|match|mod|move|mut|pub|ref|return|self|Self|static|struct|super|trait|type|unsafe|use|where|while|yield';
        var rustTypes = 'bool|char|f32|f64|i8|i16|i32|i64|i128|isize|str|u8|u16|u32|u64|u128|usize|String|Vec|Box|Rc|Arc|Option|Result|Some|None|Ok|Err|true|false|HashMap|HashSet|BTreeMap|println|eprintln|format|panic|todo|unimplemented|unreachable';

        var goKw = 'break|case|chan|const|continue|default|defer|else|fallthrough|for|func|go|goto|if|import|interface|map|package|range|return|select|struct|switch|type|var';
        var goTypes = 'bool|byte|complex64|complex128|error|float32|float64|int|int8|int16|int32|int64|rune|string|uint|uint8|uint16|uint32|uint64|uintptr|true|false|nil|iota|append|cap|close|copy|delete|len|make|new|panic|print|println|recover';

        var javaKw = 'abstract|assert|break|case|catch|class|const|continue|default|do|else|enum|extends|final|finally|for|goto|if|implements|import|instanceof|interface|native|new|package|private|protected|public|return|static|strictfp|super|switch|synchronized|this|throw|throws|transient|try|volatile|while';
        var javaTypes = 'boolean|byte|char|double|float|int|long|short|void|null|true|false|String|Integer|Long|Double|Float|Boolean|Byte|Short|Character|Object|Class|System|List|Map|Set|ArrayList|HashMap|HashSet|Iterator|Optional|Stream';

        var csKw = 'abstract|as|base|break|case|catch|checked|class|const|continue|default|delegate|do|else|enum|event|explicit|extern|finally|fixed|for|foreach|goto|if|implicit|in|interface|internal|is|lock|namespace|new|operator|out|override|params|private|protected|public|readonly|ref|return|sealed|sizeof|stackalloc|static|struct|switch|this|throw|try|typeof|unchecked|unsafe|using|virtual|volatile|while|yield|async|await|var|dynamic|nameof|when|where';
        var csTypes = 'bool|byte|char|decimal|double|float|int|long|object|sbyte|short|string|uint|ulong|ushort|void|null|true|false|List|Dictionary|Task|Action|Func|IEnumerable|Console|String|Math|Array|Exception|Nullable';

        var sqlKw = 'SELECT|FROM|WHERE|INSERT|INTO|UPDATE|SET|DELETE|CREATE|TABLE|DROP|ALTER|ADD|INDEX|JOIN|INNER|LEFT|RIGHT|OUTER|FULL|CROSS|ON|AND|OR|NOT|IN|IS|NULL|LIKE|BETWEEN|EXISTS|HAVING|GROUP|BY|ORDER|ASC|DESC|LIMIT|OFFSET|UNION|ALL|DISTINCT|AS|CASE|WHEN|THEN|ELSE|END|VALUES|PRIMARY|KEY|FOREIGN|REFERENCES|CONSTRAINT|UNIQUE|CHECK|DEFAULT|COUNT|SUM|AVG|MIN|MAX|CAST|COALESCE|IF|BEGIN|COMMIT|ROLLBACK|GRANT|REVOKE|VIEW|PROCEDURE|FUNCTION|TRIGGER|CURSOR|DECLARE|FETCH|EXEC|EXECUTE|USE|DATABASE|SCHEMA|TRUNCATE';

        var bashKw = 'if|then|else|elif|fi|for|while|do|done|case|esac|in|function|return|local|export|source|alias|unalias|declare|typeset|readonly|shift|break|continue|exit|trap|eval|exec|set|unset|echo|printf|read|test|true|false';

        function buildRules(comments, strings, keywords, types, extra) {
            var parts = [];
            // Order matters: comments first, then strings, then other tokens
            if (comments) parts.push('(?<cm>' + comments + ')');
            if (strings) parts.push('(?<st>' + strings + ')');
            if (extra && extra.directive) parts.push('(?<di>' + extra.directive + ')');
            if (extra && extra.decorator) parts.push('(?<dc>' + extra.decorator + ')');
            if (types) parts.push('(?<ty>' + kw(types) + ')');
            if (keywords) parts.push('(?<kw>' + kw(keywords) + ')');
            parts.push('(?<nu>' + number + ')');
            if (extra && extra.fn) parts.push(extra.fn);
            return new RegExp(parts.join('|'), 'gm');
        }

        var fnPattern = '(?<fn>\\b[a-zA-Z_]\\w*(?=\\s*\\())';

        return {
            'c': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString, cKw, cppTypes, {directive: cppDirective, fn: fnPattern}),
            'cpp': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString, cppKw, cppTypes, {directive: cppDirective, fn: fnPattern}),
            'h': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString, cppKw, cppTypes, {directive: cppDirective, fn: fnPattern}),
            'hpp': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString, cppKw, cppTypes, {directive: cppDirective, fn: fnPattern}),
            'javascript': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString + '|' + backtickStr, jsKw, jsTypes, {fn: fnPattern}),
            'js': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString + '|' + backtickStr, jsKw, jsTypes, {fn: fnPattern}),
            'jsx': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString + '|' + backtickStr, jsKw, jsTypes, {fn: fnPattern}),
            'typescript': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString + '|' + backtickStr, jsKw + '|type|declare|readonly|keyof|infer|extends|implements|as|is|satisfies|asserts', jsTypes + '|any|never|unknown|void|number|string|boolean|bigint|symbol|Record|Partial|Required|Pick|Omit|Readonly|Exclude|Extract|ReturnType|Parameters|Awaited', {fn: fnPattern, decorator: decorator}),
            'ts': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString + '|' + backtickStr, jsKw + '|type|declare|readonly|keyof|infer|extends|implements|as|is|satisfies|asserts', jsTypes + '|any|never|unknown|void|number|string|boolean|bigint|symbol|Record|Partial|Required|Pick|Omit|Readonly|Exclude|Extract|ReturnType|Parameters|Awaited', {fn: fnPattern, decorator: decorator}),
            'tsx': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString + '|' + backtickStr, jsKw + '|type|declare|readonly|keyof|infer|extends|implements|as|is|satisfies|asserts', jsTypes + '|any|never|unknown|void|number|string|boolean|bigint|symbol|Record|Partial|Required|Pick|Omit|Readonly|Exclude|Extract|ReturnType|Parameters|Awaited', {fn: fnPattern, decorator: decorator}),
            'python': buildRules(hashComment, dblString + '|' + sglString + '|' + '"""[\\s\\S]*?"""|' + "'''[\\s\\S]*?'''", pyKw, pyTypes, {decorator: decorator, fn: fnPattern}),
            'py': buildRules(hashComment, dblString + '|' + sglString + '|' + '"""[\\s\\S]*?"""|' + "'''[\\s\\S]*?'''", pyKw, pyTypes, {decorator: decorator, fn: fnPattern}),
            'rust': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString, rustKw, rustTypes, {fn: fnPattern}),
            'rs': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString, rustKw, rustTypes, {fn: fnPattern}),
            'go': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString + '|' + backtickStr, goKw, goTypes, {fn: fnPattern}),
            'java': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString, javaKw, javaTypes, {decorator: decorator, fn: fnPattern}),
            'csharp': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString, csKw, csTypes, {decorator: decorator, fn: fnPattern}),
            'cs': buildRules(cLineComment + '|' + cBlockComment, dblString + '|' + sglString, csKw, csTypes, {decorator: decorator, fn: fnPattern}),
            'json': buildRules(null, dblString, null, 'true|false|null', {}),
            'sql': buildRules('--[^\\n]*|' + cBlockComment, sglString, sqlKw, null, {}),
            'bash': buildRules(hashComment, dblString + '|' + sglString, bashKw, null, {fn: fnPattern}),
            'sh': buildRules(hashComment, dblString + '|' + sglString, bashKw, null, {fn: fnPattern}),
            'shell': buildRules(hashComment, dblString + '|' + sglString, bashKw, null, {fn: fnPattern}),
            'zsh': buildRules(hashComment, dblString + '|' + sglString, bashKw, null, {fn: fnPattern}),
            'powershell': buildRules(hashComment + '|<#[\\s\\S]*?#>', dblString + '|' + sglString, 'if|else|elseif|foreach|for|while|do|switch|break|continue|return|function|filter|param|begin|process|end|throw|try|catch|finally|trap|exit|ForEach-Object|Where-Object|Select-Object|Sort-Object|Group-Object|Measure-Object', 'true|false|null|Write-Host|Write-Output|Get-Content|Set-Content|Get-ChildItem|Get-Item|New-Item|Remove-Item|Test-Path', {fn: fnPattern}),
            'pwsh': buildRules(hashComment + '|<#[\\s\\S]*?#>', dblString + '|' + sglString, 'if|else|elseif|foreach|for|while|do|switch|break|continue|return|function|filter|param|begin|process|end|throw|try|catch|finally|trap|exit|ForEach-Object|Where-Object|Select-Object|Sort-Object|Group-Object|Measure-Object', 'true|false|null|Write-Host|Write-Output|Get-Content|Set-Content|Get-ChildItem|Get-Item|New-Item|Remove-Item|Test-Path', {fn: fnPattern}),
            'cmake': buildRules(hashComment, dblString, 'if|elseif|else|endif|foreach|endforeach|while|endwhile|function|endfunction|macro|endmacro|return|break|continue|set|unset|option|add_executable|add_library|target_link_libraries|target_include_directories|find_package|include|add_subdirectory|message|install|configure_file|add_test|project|cmake_minimum_required', null, {fn: fnPattern}),
            'yaml': buildRules(hashComment, dblString + '|' + sglString, null, 'true|false|null|yes|no|on|off', {}),
            'yml': buildRules(hashComment, dblString + '|' + sglString, null, 'true|false|null|yes|no|on|off', {}),
            'xml': null,
            'html': null,
            'css': null,
            'markdown': null,
            'md': null
        };
    })();

    // Apply syntax highlighting to already-escaped HTML code
    function highlightCode(escapedCode, lang) {
        if (!lang) return escapedCode;
        var l = lang.toLowerCase();

        // XML / HTML — special tag-based highlighting
        if (l === 'xml' || l === 'html' || l === 'svg' || l === 'htm') {
            return highlightXml(escapedCode);
        }
        // CSS
        if (l === 'css' || l === 'scss' || l === 'less') {
            return highlightCss(escapedCode);
        }

        var re = LANG_RULES[l];
        if (!re) return escapedCode;

        // Reset regex state
        re.lastIndex = 0;

        return escapedCode.replace(re, function(match) {
            // Determine which named group matched
            var cls = '';
            // We check arguments — named groups are in the last arg (object)
            var groups = arguments[arguments.length - 1];
            if (typeof groups === 'object' && groups !== null) {
                if (groups.cm !== undefined) cls = 'hl-comment';
                else if (groups.st !== undefined) cls = 'hl-string';
                else if (groups.di !== undefined) cls = 'hl-directive';
                else if (groups.dc !== undefined) cls = 'hl-decorator';
                else if (groups.ty !== undefined) cls = 'hl-type';
                else if (groups.kw !== undefined) cls = 'hl-keyword';
                else if (groups.nu !== undefined) cls = 'hl-number';
                else if (groups.fn !== undefined) cls = 'hl-function';
            }
            if (!cls) return match;
            return '<span class="' + cls + '">' + match + '</span>';
        });
    }

    // XML/HTML highlighting
    function highlightXml(code) {
        return code
            .replace(/(&lt;\/?)([\w\-:]+)/g, function(m, bracket, tag) {
                return '<span class="hl-keyword">' + bracket + '</span><span class="hl-type">' + tag + '</span>';
            })
            .replace(/([\w\-:]+)(=)(&quot;[^&]*?&quot;|&#39;[^&]*?&#39;)/g, function(m, attr, eq, val) {
                return '<span class="hl-function">' + attr + '</span>' + eq + '<span class="hl-string">' + val + '</span>';
            })
            .replace(/(&lt;!--[\s\S]*?--&gt;)/g, '<span class="hl-comment">$1</span>');
    }

    // CSS highlighting
    function highlightCss(code) {
        return code
            .replace(/(\/\*[\s\S]*?\*\/)/g, '<span class="hl-comment">$1</span>')
            .replace(/([\w\-]+)(\s*:\s*)/g, '<span class="hl-function">$1</span>$2')
            .replace(/(#[0-9a-fA-F]{3,8})\b/g, '<span class="hl-number">$1</span>')
            .replace(/(\d+\.?\d*)(px|em|rem|%|vh|vw|s|ms|deg|fr|ch|ex|vmin|vmax)\b/g, '<span class="hl-number">$1$2</span>')
            .replace(/\b(\d+\.?\d*)\b/g, '<span class="hl-number">$1</span>');
    }

    // Render a fenced code block — VS Code interactive-result-code-block structure
    function isShellLang(lang) {
        if (!lang) return false;
        var l = lang.toLowerCase();
        return (l === 'sh' || l === 'bash' || l === 'shell' || l === 'zsh' ||
                l === 'pwsh' || l === 'powershell' || l === 'bat' || l === 'cmd');
    }

    function basename(path) {
        if (!path) return '';
        var p = path.replace(/\\/g, '/');
        var parts = p.split('/');
        return parts[parts.length - 1] || path;
    }

    function renderCodeBlock(lang, code, blockIndex, filePath) {
        var label = filePath ? basename(filePath) : (lang ? escapeHtml(lang) : 'text');
        var labelTitle = filePath ? escapeHtml(filePath) : '';
        var escapedCode = escapeHtml(code);
        var id = 'code-block-' + blockIndex;
        var fileAttr = filePath ? (' data-file="' + escapeHtml(filePath) + '"') : '';
        var langAttr = lang ? (' data-lang="' + escapeHtml(lang) + '"') : '';

        // SVG icons for toolbar buttons
        var applyIcon = '<svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor"><path d="M13.23 1h-1.46L3.52 9.25l-.16.22L1 13.59 2.41 15l4.12-2.36.22-.16L15 4.23V2.77L13.23 1zM2.41 13.59l1.51-3 1.45 1.45-2.96 1.55zm3.83-2.06L4.47 9.76l8-8 1.77 1.77-8 8z"/></svg>';
        var copyIcon = '<svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor"><path d="M4 4l1-1h5.414L14 6.586V14l-1 1H5l-1-1V4zm9 3l-3-3H5v10h8V7zM3 1L2 2v10h1V2h6.414l-1-1H3z"/></svg>';
        var insertIcon = '<svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor"><path d="M8 1v6H2v1h6v6h1V8h6V7H9V1H8z"/></svg>';
        var runIcon = '<svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor"><path d="M3 2l10 6-10 6V2z"/></svg>';

        var runBtn = isShellLang(lang)
            ? '<span class="toolbar-sep"></span>' +
              '<button class="code-action-btn" onclick="ChatApp._runCode(\'' + id + '\')" title="Run in Terminal">' + runIcon + '</button>'
            : '';

        return '<div class="interactive-result-code-block" id="' + id + '"' + fileAttr + langAttr + '>' +
            '<div class="code-block-toolbar">' +
                '<span class="code-block-lang-label" title="' + labelTitle + '">' + label + '</span>' +
                '<div class="code-block-actions">' +
                    '<button class="code-action-btn" onclick="ChatApp._applyCode(\'' + id + '\')" title="Apply to File">' + applyIcon + '</button>' +
                    '<button class="code-action-btn" onclick="ChatApp._copyCode(\'' + id + '\')" title="Copy">' + copyIcon + '</button>' +
                    '<button class="code-action-btn" onclick="ChatApp._insertCode(\'' + id + '\')" title="Insert at Cursor">' + insertIcon + '</button>' +
                    runBtn +
                '</div>' +
            '</div>' +
            '<div class="code-block-body"><pre><code class="language-' + escapeHtml(lang || 'text') + '">' +
                highlightCode(escapedCode, lang) +
            '</code></pre></div>' +
        '</div>';
    }

    // Parse markdown text into HTML
    function render(text, streaming) {
        if (!text) return '';

        var lines = text.split('\n');
        var html = '';
        var inCodeBlock = false;
        var codeBlockLang = '';
        var codeBlockContent = '';
        var codeBlockIndex = 0;
        var inList = false;
        var listType = '';
        var inBlockquote = false;
        var inTable = false;
        var tableRows = [];
        var codeBlockFile = '';

        for (var i = 0; i < lines.length; i++) {
            var line = lines[i];

            // ── Fenced code blocks ───────────────────────────────────────
            if (!inCodeBlock && /^```(\w*)/.test(line)) {
                // Close any open structures
                if (inList) { html += '</' + listType + '>'; inList = false; }
                if (inBlockquote) { html += '</blockquote>'; inBlockquote = false; }
                closeTable();

                inCodeBlock = true;
                codeBlockLang = RegExp.$1 || '';
                codeBlockContent = '';
                codeBlockFile = '';
                continue;
            }

            if (inCodeBlock) {
                if (line === '```') {
                    html += renderCodeBlock(codeBlockLang, codeBlockContent, codeBlockIndex++, codeBlockFile);
                    inCodeBlock = false;
                } else {
                    if (!codeBlockContent) {
                        if (/^[^\\s].*\\.[A-Za-z0-9]+$/.test(line.trim())) {
                            codeBlockFile = line.trim();
                            continue;
                        }
                    }
                    if (codeBlockContent) codeBlockContent += '\n';
                    codeBlockContent += line;
                }
                continue;
            }

            // ── Table detection ──────────────────────────────────────────
            if (/^\|(.+)\|$/.test(line)) {
                if (inList) { html += '</' + listType + '>'; inList = false; }
                if (inBlockquote) { html += '</blockquote>'; inBlockquote = false; }

                // Skip separator rows (|---|---|)
                if (/^\|[\s\-:|]+\|$/.test(line)) continue;

                if (!inTable) {
                    inTable = true;
                    tableRows = [];
                }
                var cells = line.split('|').filter(function(c, idx, arr) {
                    return idx > 0 && idx < arr.length - 1;
                }).map(function(c) { return c.trim(); });
                tableRows.push(cells);
                continue;
            } else if (inTable) {
                closeTable();
            }

            function closeTable() {
                if (!inTable) return;
                inTable = false;
                if (tableRows.length === 0) return;
                html += '<table>';
                // First row as header
                html += '<thead><tr>';
                tableRows[0].forEach(function(c) {
                    html += '<th>' + inlineMarkdown(c) + '</th>';
                });
                html += '</tr></thead>';
                if (tableRows.length > 1) {
                    html += '<tbody>';
                    for (var r = 1; r < tableRows.length; r++) {
                        html += '<tr>';
                        tableRows[r].forEach(function(c) {
                            html += '<td>' + inlineMarkdown(c) + '</td>';
                        });
                        html += '</tr>';
                    }
                    html += '</tbody>';
                }
                html += '</table>';
                tableRows = [];
            }

            // ── Horizontal rule ──────────────────────────────────────────
            if (/^(-{3,}|\*{3,}|_{3,})\s*$/.test(line)) {
                if (inList) { html += '</' + listType + '>'; inList = false; }
                if (inBlockquote) { html += '</blockquote>'; inBlockquote = false; }
                html += '<hr>';
                continue;
            }

            // ── Headings ─────────────────────────────────────────────────
            var headingMatch = /^(#{1,6})\s+(.+)/.exec(line);
            if (headingMatch) {
                if (inList) { html += '</' + listType + '>'; inList = false; }
                if (inBlockquote) { html += '</blockquote>'; inBlockquote = false; }
                var level = headingMatch[1].length;
                html += '<h' + level + '>' + inlineMarkdown(headingMatch[2]) + '</h' + level + '>';
                continue;
            }

            // ── Blockquotes ──────────────────────────────────────────────
            if (/^>\s?(.*)/.test(line)) {
                if (inList) { html += '</' + listType + '>'; inList = false; }
                if (!inBlockquote) {
                    inBlockquote = true;
                    html += '<blockquote>';
                }
                html += inlineMarkdown(RegExp.$1) + '<br>';
                continue;
            } else if (inBlockquote) {
                html += '</blockquote>';
                inBlockquote = false;
            }

            // ── Unordered list ───────────────────────────────────────────
            var ulMatch = /^(\s*)[*\-+]\s+(.+)/.exec(line);
            if (ulMatch) {
                if (!inList || listType !== 'ul') {
                    if (inList) html += '</' + listType + '>';
                    html += '<ul>';
                    inList = true;
                    listType = 'ul';
                }
                html += '<li>' + inlineMarkdown(ulMatch[2]) + '</li>';
                continue;
            }

            // ── Ordered list ─────────────────────────────────────────────
            var olMatch = /^(\s*)\d+\.\s+(.+)/.exec(line);
            if (olMatch) {
                if (!inList || listType !== 'ol') {
                    if (inList) html += '</' + listType + '>';
                    html += '<ol>';
                    inList = true;
                    listType = 'ol';
                }
                html += '<li>' + inlineMarkdown(olMatch[2]) + '</li>';
                continue;
            }

            // Close list if we hit a non-list line
            if (inList && line.trim() === '') {
                html += '</' + listType + '>';
                inList = false;
                continue;
            }

            // ── Empty line ───────────────────────────────────────────────
            if (line.trim() === '') {
                continue;
            }

            // ── Paragraph ────────────────────────────────────────────────
            if (inList) { html += '</' + listType + '>'; inList = false; }
            html += '<p>' + inlineMarkdown(line) + '</p>';
        }

        // Close any open structures
        if (inCodeBlock) {
            // Streaming: code block still being typed
            html += renderCodeBlock(codeBlockLang, codeBlockContent, codeBlockIndex++, codeBlockFile);
        }
        if (inList) html += '</' + listType + '>';
        if (inBlockquote) html += '</blockquote>';
        if (inTable) {
            // Flush remaining table
            inTable = false;
            if (tableRows.length > 0) {
                html += '<table><thead><tr>';
                tableRows[0].forEach(function(c) {
                    html += '<th>' + inlineMarkdown(c) + '</th>';
                });
                html += '</tr></thead>';
                if (tableRows.length > 1) {
                    html += '<tbody>';
                    for (var r = 1; r < tableRows.length; r++) {
                        html += '<tr>';
                        tableRows[r].forEach(function(c) {
                            html += '<td>' + inlineMarkdown(c) + '</td>';
                        });
                        html += '</tr>';
                    }
                    html += '</tbody>';
                }
                html += '</table>';
            }
        }

        // Add VS Code–style animated ellipsis streaming indicator
        if (streaming) {
            html += '<span class="chat-animated-ellipsis"></span>';
        }

        return html;
    }

    // Inline markdown: bold, italic, code, links, images, inline anchors
    function inlineMarkdown(text) {
        if (!text) return '';

        var s = escapeHtml(text);

        // Inline code / inline anchors — process backtick content
        s = s.replace(/`([^`]+)`/g, function(match, content) {
            // Check if this looks like a file path (has extension with slash/backslash, or common extensions)
            var isFilePath = /[\/\\]/.test(content) && /\.\w{1,10}$/.test(content);
            var isLineRef = /\.\w{1,10}(#L\d+|:\d+)/.test(content);
            if (isFilePath || isLineRef) {
                // File icon SVG
                var fileIcon = '<span class="anchor-icon"><svg viewBox="0 0 16 16"><path d="M13.85 4.44l-3.28-3.3-.71-.14H2.5l-.5.5v13l.5.5h11l.5-.5V4.8l-.15-.36zM13 5h-3V2l3 3zM3 14V2h6v4h4v8H3z" fill="currentColor"/></svg></span>';
                var cleanPath = content.replace(/#L\d+.*$/, '').replace(/:\d+.*$/, '');
                var displayName = cleanPath.split(/[\/\\]/).pop();
                return '<span class="chat-inline-anchor" onclick="ChatApp._fileClick(\'' + content + '\')">' +
                    fileIcon + '<span class="anchor-label">' + displayName + '</span></span>';
            }
            return '<code>' + content + '</code>';
        });

        // Images: ![alt](url)
        s = s.replace(/!\[([^\]]*)\]\(([^)]+)\)/g, '<img src="$2" alt="$1" style="max-width:100%">');

        // Links: [text](url)
        s = s.replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2" title="$2" class="external-link">$1 &#x2197;</a>');

        // Bold + italic: ***text***
        s = s.replace(/\*\*\*(.+?)\*\*\*/g, '<strong><em>$1</em></strong>');

        // Bold: **text** or __text__
        s = s.replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
        s = s.replace(/__(.+?)__/g, '<strong>$1</strong>');

        // Italic: *text* or _text_
        s = s.replace(/\*(.+?)\*/g, '<em>$1</em>');
        s = s.replace(/(^|\\W)_(.+?)_(?=\\W|$)/g, '$1<em>$2</em>');

        // Strikethrough: ~~text~~
        s = s.replace(/~~(.+?)~~/g, '<del>$1</del>');

        return s;
    }

    return {
        render: render,
        escapeHtml: escapeHtml,
        inlineMarkdown: inlineMarkdown
    };
})();
