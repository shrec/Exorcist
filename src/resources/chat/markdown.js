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
        var runBtn = isShellLang(lang)
            ? '<button class="code-action-btn" onclick="ChatApp._runCode(\\'' + id + '\\')" title="Run">&#x25B6; Run</button>'
            : '';

        return '<div class="interactive-result-code-block" id="' + id + '"' + fileAttr + langAttr + '>' +
            '<div class="code-block-toolbar">' +
                '<span class="code-block-lang-label" title="' + labelTitle + '">' + label + '</span>' +
                '<div class="code-block-actions">' +
                    '<button class="code-action-btn" onclick="ChatApp._applyCode(\\'' + id + '\\')" title="Apply">&#x2199; Apply</button>' +
                    '<button class="code-action-btn" onclick="ChatApp._copyCode(\\'' + id + '\\')" title="Copy">&#x2398; Copy</button>' +
                    '<button class="code-action-btn" onclick="ChatApp._insertCode(\\'' + id + '\\')" title="Insert at Cursor">&#x2913; Insert</button>' +
                    runBtn +
                '</div>' +
            '</div>' +
            '<div class="code-block-body"><pre><code class="language-' + escapeHtml(lang || 'text') + '">' +
                escapedCode +
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

    // Inline markdown: bold, italic, code, links, images
    function inlineMarkdown(text) {
        if (!text) return '';

        var s = escapeHtml(text);

        // Inline code (backticks) — must be first to prevent inner parsing
        s = s.replace(/`([^`]+)`/g, '<code>$1</code>');

        // Images: ![alt](url)
        s = s.replace(/!\[([^\]]*)\]\(([^)]+)\)/g, '<img src="$2" alt="$1" style="max-width:100%">');

        // Links: [text](url)
        s = s.replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2" title="$2">$1</a>');

        // Bold + italic: ***text***
        s = s.replace(/\*\*\*(.+?)\*\*\*/g, '<strong><em>$1</em></strong>');

        // Bold: **text** or __text__
        s = s.replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
        s = s.replace(/__(.+?)__/g, '<strong>$1</strong>');

        // Italic: *text* or _text_
        s = s.replace(/\*(.+?)\*/g, '<em>$1</em>');
        s = s.replace(/(?<!\w)_(.+?)_(?!\w)/g, '<em>$1</em>');

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
