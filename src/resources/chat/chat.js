// ═══════════════════════════════════════════════════════════════════════════════
// Exorcist Chat Rendering Engine — VS Code Parity
//
// Full HTML chat panel: header, transcript, input, popups.
// Called from C++ via ChatJSBridge.
// ═══════════════════════════════════════════════════════════════════════════════

var ChatApp = (function() {
    'use strict';

    // ── State ────────────────────────────────────────────────────────────────
    var turns = [];
    var isStreaming = false;
    var userScrolledAway = false;
    var scrollRAF = null;
    var currentMode = 0;        // 0=Ask, 1=Edit, 2=Agent
    var currentModelId = '';
    var models = [];            // [{id, name, premium, thinking, vision, tools}]
    var slashCommands = [];
    var inputHistory = [];
    var historyIndex = -1;
    var thinkingEnabled = false;

    // ── SVG icons ────────────────────────────────────────────────────────────
    var SPARKLE_SVG = '<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M8 1.5l1.28 3.96L13.25 6.5l-3.97 1.04L8 11.5l-1.28-3.96L2.75 6.5l3.97-1.04L8 1.5z" fill="currentColor"/></svg>';
    var COPILOT_AVATAR = '<div class="avatar avatar-ai">' + SPARKLE_SVG + '</div>';
    var USER_AVATAR = '<div class="avatar avatar-user">U</div>';

    // ── DOM refs ─────────────────────────────────────────────────────────────
    var welcomeEl       = document.getElementById('welcome');
    var transcriptEl    = document.getElementById('transcript');
    var suggestionsEl   = document.getElementById('welcome-suggestions');
    var toolCountEl     = document.getElementById('welcome-tool-count');
    var welcomeTitleEl  = document.querySelector('.welcome-title');
    var welcomeSubtitleEl = document.getElementById('welcomeSubtitle');
    var welcomeActionEl = document.getElementById('welcomeAction');

    // Header
    var sessionTitleEl  = document.getElementById('sessionTitle');
    var historyBtn      = document.getElementById('historyBtn');
    var newSessionBtn   = document.getElementById('newSessionBtn');
    var settingsBtn     = document.getElementById('settingsBtn');

    // Changes bar
    var changesBarEl    = document.getElementById('changesBar');
    var changesLabelEl  = document.getElementById('changesLabel');
    var keepAllBtn      = document.getElementById('keepAllBtn');
    var undoAllBtn      = document.getElementById('undoAllBtn');

    // Input
    var inputPartEl     = document.getElementById('inputPart');
    var inputEl         = document.getElementById('chatInput');
    var inputWrapperEl  = document.getElementById('inputWrapper');
    var sendBtnEl       = document.getElementById('sendBtn');
    var cancelBtnEl     = document.getElementById('cancelBtn');
    var streamingBarEl  = document.getElementById('streamingBar');
    var attachChipBarEl = document.getElementById('attachChipBar');
    var attachBtnEl     = document.getElementById('attachBtn');

    // Mode
    var modeBtns        = document.querySelectorAll('.mode-btn');

    // Model
    var modelBtnEl      = document.getElementById('modelBtn');
    var modelLabelEl    = document.getElementById('modelLabel');
    var modelPopupEl    = document.getElementById('modelPopup');

    // Thinking
    var thinkingBtnEl   = document.getElementById('thinkingBtn');

    // Tool count / ctx
    var inputToolCountEl = document.getElementById('inputToolCount');

    // Slash popup
    var slashPopupEl    = document.getElementById('slashPopup');
    var mentionPopupEl  = document.getElementById('mentionPopup');

    // ── Auto-scroll ──────────────────────────────────────────────────────────
    function isNearBottom() {
        return transcriptEl.scrollHeight - transcriptEl.scrollTop - transcriptEl.clientHeight < 40;
    }

    function scrollToBottom() {
        transcriptEl.scrollTop = transcriptEl.scrollHeight;
        userScrolledAway = false;
    }

    function maybeAutoScroll() {
        if (isStreaming && !userScrolledAway) {
            scrollToBottom();
        }
    }

    transcriptEl.addEventListener('scroll', function() {
        if (isStreaming) {
            userScrolledAway = !isNearBottom();
        }
    });

    // ── Clipboard copy bridge ────────────────────────────────────────────────
    // Ultralight doesn't have native clipboard access so we intercept
    // copy events and send the selected text to C++ via the bridge.
    document.addEventListener('copy', function(e) {
        var sel = window.getSelection();
        var text = sel ? sel.toString() : '';
        if (text && window.exorcist) {
            window.exorcist.sendToHost('copyText', { text: text });
        }
    });

    function scheduleScroll() {
        if (!scrollRAF) {
            scrollRAF = requestAnimationFrame(function() {
                scrollRAF = null;
                maybeAutoScroll();
            });
        }
    }

    // ── Auto-resize textarea ─────────────────────────────────────────────────
    function autoResizeInput() {
        if (!inputEl) return;
        inputEl.style.height = 'auto';
        var h = Math.min(Math.max(inputEl.scrollHeight, 32), 180);
        inputEl.style.height = h + 'px';
    }

    // ── Slash command popup ──────────────────────────────────────────────────
    function updateSlashPopup() {
        if (!slashPopupEl || !inputEl) return;
        var text = inputEl.value;
        if (!text.startsWith('/') || text.indexOf(' ') !== -1) {
            slashPopupEl.style.display = 'none';
            return;
        }
        var filter = text.toLowerCase();
        var matches = slashCommands.filter(function(c) {
            return c.name.toLowerCase().indexOf(filter) === 0;
        });
        if (matches.length === 0) {
            slashPopupEl.style.display = 'none';
            return;
        }
        var html = '';
        for (var i = 0; i < matches.length; i++) {
            html += '<div class="autocomplete-item" data-cmd="' + esc(matches[i].name) + '">' +
                '<span class="cmd-name">' + esc(matches[i].name) + '</span>' +
                '<span class="cmd-desc">' + esc(matches[i].desc || '') + '</span>' +
            '</div>';
        }
        slashPopupEl.innerHTML = html;
        slashPopupEl.style.display = 'block';

        // Click handler for items
        var items = slashPopupEl.querySelectorAll('.autocomplete-item');
        for (var j = 0; j < items.length; j++) {
            items[j].onclick = (function(cmd) {
                return function() {
                    inputEl.value = cmd + ' ';
                    slashPopupEl.style.display = 'none';
                    inputEl.focus();
                    autoResizeInput();
                };
            })(items[j].getAttribute('data-cmd'));
        }
    }

    // ── Mention/variable popup ────────────────────────────────────────────────
    function findTriggerAtCursor(text, cursorPos) {
        var atPos = text.lastIndexOf('@', cursorPos - 1);
        if (atPos >= 0 && (atPos === 0 || /\s/.test(text[atPos - 1]))) {
            var filterAt = text.slice(atPos + 1, cursorPos);
            if (!/\s/.test(filterAt)) return { trigger: '@', pos: atPos, filter: filterAt };
        }
        var hashPos = text.lastIndexOf('#', cursorPos - 1);
        if (hashPos >= 0 && (hashPos === 0 || /\s/.test(text[hashPos - 1]))) {
            var filterHash = text.slice(hashPos + 1, cursorPos);
            if (!/\s/.test(filterHash)) return { trigger: '#', pos: hashPos, filter: filterHash };
        }
        return null;
    }

    function requestMentionItems() {
        if (!inputEl || !mentionPopupEl) return;
        var text = inputEl.value;
        var cursorPos = inputEl.selectionStart || 0;
        var t = findTriggerAtCursor(text, cursorPos);
        if (!t) {
            mentionPopupEl.style.display = 'none';
            return;
        }
        if (window.exorcist) {
            window.exorcist.sendToHost('mentionQuery', {
                trigger: t.trigger,
                filter: t.filter || ''
            });
        }
    }

    function insertMention(insertText) {
        if (!inputEl) return;
        var text = inputEl.value;
        var cursorPos = inputEl.selectionStart || 0;
        var t = findTriggerAtCursor(text, cursorPos);
        if (!t) {
            inputEl.value = text + insertText + ' ';
            autoResizeInput();
            return;
        }
        var before = text.slice(0, t.pos);
        var after = text.slice(cursorPos);
        inputEl.value = before + insertText + ' ' + after;
        var newPos = (before + insertText + ' ').length;
        inputEl.selectionStart = inputEl.selectionEnd = newPos;
        autoResizeInput();
    }

    // ── Model popup ──────────────────────────────────────────────────────────
    function showModelPopup() {
        if (!modelPopupEl || models.length === 0) return;
        var html = '';
        for (var i = 0; i < models.length; i++) {
            var m = models[i];
            var sel = m.id === currentModelId ? ' selected' : '';
            var label = m.displayName || m.name || m.id;
            var badges = '';
            if (m.premium) badges += '<span class="model-badge premium">PRO</span>';
            if (m.thinking) badges += '<span class="model-badge thinking">Think</span>';
            if (m.vision) badges += '<span class="model-badge vision">Vision</span>';
            if (m.tools) badges += '<span class="model-badge tools">Tools</span>';
            html += '<div class="model-item' + sel + '" data-id="' + esc(m.id) + '">' +
                '<span class="model-name">' + esc(label) + '</span>' +
                (badges ? '<span class="model-badges">' + badges + '</span>' : '') +
            '</div>';
        }
        modelPopupEl.innerHTML = html;
        modelPopupEl.style.display = 'block';

        var items = modelPopupEl.querySelectorAll('.model-item');
        for (var j = 0; j < items.length; j++) {
            items[j].onclick = (function(id) {
                return function() {
                    modelPopupEl.style.display = 'none';
                    if (window.exorcist) window.exorcist.sendToHost('modelSelected', { modelId: id });
                };
            })(items[j].getAttribute('data-id'));
        }
    }

    function hideAllPopups() {
        if (slashPopupEl) slashPopupEl.style.display = 'none';
        if (mentionPopupEl) mentionPopupEl.style.display = 'none';
        if (modelPopupEl) modelPopupEl.style.display = 'none';
    }

    // ── Escape helper ────────────────────────────────────────────────────────
    function esc(s) {
        if (!s) return '';
        return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
                .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
    }
    // Maps tool name → category SVG icon for card headers
    function toolCategoryIcon(toolName) {
        var name = (toolName || '').toLowerCase();
        // File operations — pencil icon
        if (name === 'read_file' || name === 'create_file' || name === 'edit_file' ||
            name === 'replace_string_in_file' || name === 'delete_file' ||
            name === 'multi_replace_string_in_file' || name === 'edit_notebook_file') {
            return '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M13.23 1h-1.46L3.52 9.25l-.16.22L1 13.59 2.41 15l4.12-2.36.22-.16L15 4.23V2.77L13.23 1zM2.41 13.59l1.51-3 1.45 1.45-2.96 1.55zm3.83-2.06L4.47 9.76l8-8 1.77 1.77-8 8z"/></svg>';
        }
        // Terminal — terminal icon
        if (name === 'run_in_terminal' || name === 'run_command') {
            return '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M0 3v10h16V3H0zm15 9H1V4h14v8zM8 8h4v1H8V8zM2 8l3-2v1L3 8.5 5 10v1L2 9V8z"/></svg>';
        }
        // Search — magnifier icon
        if (name === 'grep_search' || name === 'semantic_search' ||
            name === 'file_search' || name === 'codebase_search') {
            return '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M15.25 15.02l-4.57-4.57a5.49 5.49 0 1 0-.71.7l4.57 4.58.71-.71zM6.5 11a4.5 4.5 0 1 1 0-9 4.5 4.5 0 0 1 0 9z"/></svg>';
        }
        // Directory — list icon
        if (name === 'list_dir') {
            return '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M2 3h12v1H2V3zm0 3h12v1H2V6zm0 3h10v1H2V9zm0 3h8v1H2v-1z"/></svg>';
        }
        // Subagent — people icon
        if (name === 'run_subagent' || name === 'subagent') {
            return '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M8 1a2 2 0 1 1 0 4 2 2 0 0 1 0-4zm5 3a1.5 1.5 0 1 1 0 3 1.5 1.5 0 0 1 0-3zM3 4a1.5 1.5 0 1 1 0 3 1.5 1.5 0 0 1 0-3zm5 4c2.21 0 4 1.12 4 2.5V12H4v-1.5C4 9.12 5.79 8 8 8zm5 1c1.1 0 2 .67 2 1.5V12h-2v-1.5c0-.54-.17-1.05-.5-1.5zm-10 0c-.33.45-.5.96-.5 1.5V12H1v-1.5C1 9.67 1.9 9 3 9z"/></svg>';
        }
        // Default — gear icon
        return '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor">' +
            '<path d="M9.1 4.4L8.6 2H7.4l-.5 2.4-.7.3-2-1.3-.9.8 1.3 2-.3.7L2 7.4v1.2l2.4.5.3.7-1.3 2 .8.9 2-1.3.7.3.5 2.4h1.2l.5-2.4.7-.3 2 1.3.9-.8-1.3-2 .3-.7 2.4-.5V7.4l-2.4-.5-.3-.7 1.3-2-.8-.9-2 1.3-.7-.3zM8 11a3 3 0 1 0 0-6 3 3 0 0 0 0 6zm0-1a2 2 0 1 1 0-4 2 2 0 0 1 0 4z"/></svg>';
    }

    function toolCardStateClass(state) {
        switch (state) {
            case 1: return ' is-streaming';
            case 3: return ' is-complete';
            case 4: return ' is-error';
            default: return '';
        }
    }

    // VS Code–style status icons: spinner for streaming, checkmark for complete,
    // X for error, circle for queued/pending
    function toolStatusIcon(state) {
        // Completed — checkmark
        if (state === 3) {
            return '<svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M6.27 10.87h.71l4.56-6.47-.71-.5-4.2 5.94-2.26-2.03-.64.56 2.54 2.5z"/></svg>';
        }
        // Error — X
        if (state === 4) {
            return '<svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M8 8.707l3.646 3.647.708-.707L8.707 8l3.647-3.646-.707-.708L8 7.293 4.354 3.646l-.707.708L7.293 8l-3.646 3.646.707.708L8 8.707z"/></svg>';
        }
        // Streaming — animated spinner (circle with gap)
        if (state === 1) {
            return '<svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor" class="tool-spinner-icon">' +
                '<path d="M13.917 7A6.002 6.002 0 0 0 2.083 7H1.071a7.002 7.002 0 0 1 13.858 0h-1.012z"/></svg>';
        }
        // Queued/default — small circle
        return '<svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor">' +
            '<circle cx="8" cy="8" r="3" opacity="0.4"/></svg>';
    }

    function buildCardStatusHtml(state) {
        if (state === 0) {
            return '<div class="tool-card-status">' +
                '<span class="tool-card-status-dot queued"></span>' +
                '<span class="tool-card-status-text">Queued</span></div>';
        }
        if (state === 1) {
            return '<div class="tool-card-status">' +
                '<span class="tool-card-status-dot spinning"></span>' +
                '<span class="tool-card-status-text">Processing</span></div>';
        }
        return '';
    }

    function buildCardDetailHtml(toolName, input, output) {
        if (!input && !output) return '';
        var html = '<div class="tool-card-detail hidden">';
        if (input) {
            var formatted = formatToolInput(toolName, input);
            html += '<div class="tool-detail-section">' +
                '<div class="detail-label">Input</div>' +
                '<pre class="tool-io">' + MarkdownRenderer.escapeHtml(formatted) + '</pre></div>';
        }
        if (output) {
            html += '<div class="tool-detail-section">' +
                '<div class="detail-label">Output</div>' +
                '<pre class="tool-io">' + MarkdownRenderer.escapeHtml(output) + '</pre></div>';
        }
        return html + '</div>';
    }

    function buildConfirmHtml(callId, toolName) {
        var warnIcon = '<span class="confirmation-icon">' +
            '<svg viewBox="0 0 16 16"><path d="M7.56 1h.88l6.54 12.26-.44.74H1.46L1.02 13.26 7.56 1zM8 2.28L2.28 13h11.44L8 2.28zM8.625 12v-1h-1.25v1h1.25zm-1.25-2V6h1.25v4h-1.25z"/></svg>' +
            '</span>';
        return '<div class="chat-confirmation-widget">' +
            '<div class="confirmation-header" onclick="ChatApp._toggleConfirmation(this.parentElement)">' +
                warnIcon +
                '<span class="confirmation-title">Allow \u201C' +
                    MarkdownRenderer.escapeHtml(toolName || 'tool') +
                '\u201D?</span>' +
                '<span class="confirmation-chevron">&#x25BC;</span>' +
            '</div>' +
            '<div class="confirmation-body">' +
                '<div class="confirmation-actions">' +
                    '<button class="ws-action-btn primary" onclick="ChatApp._confirmTool(\'' +
                        callId + '\', 2)">Allow Always</button>' +
                    '<button class="ws-action-btn primary" onclick="ChatApp._confirmTool(\'' +
                        callId + '\', 1)">Allow</button>' +
                    '<button class="ws-action-btn" onclick="ChatApp._confirmTool(\'' +
                        callId + '\', 0)">Deny</button>' +
                '</div>' +
            '</div></div>';
    }

    // ── Content Part Renderers ───────────────────────────────────────────────

    function renderMarkdownPart(part, streaming) {
        var text = part.markdownText || part.text || '';
        return '<div class="rendered-markdown" data-type="markdown">' +
            MarkdownRenderer.render(text, streaming) +
        '</div>';
    }

    // Rotating status messages for thinking animation
    var thinkingMessages = ['Thinking', 'Reasoning', 'Processing', 'Analyzing', 'Considering'];
    var thinkingMsgIndex = 0;

    function renderThinkingPart(part) {
        var collapsed = part.thinkingCollapsed ? ' collapsed' : '';
        var streamingClass = part._streaming ? ' streaming' : '';
        var text = MarkdownRenderer.escapeHtml(part.thinkingText || part.text || '');

        // Nested tool items
        var toolsHtml = '';
        if (part._tools && part._tools.length > 0) {
            for (var i = 0; i < part._tools.length; i++) {
                toolsHtml += renderToolInvocationPart(part._tools[i]);
            }
        }

        // Spinner icon (gear) for streaming, chevron for collapsed state
        var spinnerIcon = '<span class="thinking-spinner-icon">' +
            '<svg viewBox="0 0 16 16"><path d="M9.1 4.4L8.6 2H7.4l-.5 2.4-.7.3-2-1.3-.9.8 1.3 2-.3.7L2 7.4v1.2l2.4.5.3.7-1.3 2 .8.9 2-1.3.7.3.5 2.4h1.2l.5-2.4.7-.3 2 1.3.9-.8-1.3-2 .3-.7 2.4-.5V7.4l-2.4-.5-.3-.7 1.3-2-.8-.9-2 1.3-.7-.3zM8 11a3 3 0 1 0 0-6 3 3 0 0 0 0 6zm0-1a2 2 0 1 1 0-4 2 2 0 0 1 0 4z"/></svg>' +
            '</span>';
        var chevronIcon = '<span class="thinking-chevron">&#x25BC;</span>';
        var headerIcon = part._streaming ? spinnerIcon : chevronIcon;

        // Status message for streaming
        var statusMsg = part._streaming
            ? '<span class="thinking-status-msg">\u2014 ' +
              MarkdownRenderer.escapeHtml(thinkingMessages[thinkingMsgIndex % thinkingMessages.length]) +
              '...</span>'
            : '';

        return '<div class="chat-thinking-box' + collapsed + streamingClass + '" data-type="thinking">' +
            '<button class="thinking-header-btn" onclick="ChatApp._toggleThinking(this.parentElement)">' +
                headerIcon +
                '<span class="thinking-label">Thinking</span>' +
                statusMsg +
            '</button>' +
            '<div class="thinking-content">' +
                '<div class="thinking-text">' + text.replace(/\n/g, '<br>') + '</div>' +
                toolsHtml +
            '</div>' +
        '</div>';
    }

    // ── VS Code–style Working Section ─────────────────────────────────────
    // Rotating spinner messages (matches VS Code)
    var workingMessages = ['Processing', 'Preparing', 'Loading', 'Analyzing', 'Evaluating'];
    var workingMsgIdx = 0;

    function renderWorkingStepIcon(toolName, state) {
        // Completed — checkmark
        if (state === 3) {
            return '<span class="wk-step-icon wk-check">' +
                '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M6.27 10.87h.71l4.56-6.47-.71-.5-4.2 5.94-2.26-2.03-.64.56 2.54 2.5z"/></svg></span>';
        }
        // Error — X
        if (state === 4) {
            return '<span class="wk-step-icon wk-error">' +
                '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M8 8.707l3.646 3.647.708-.707L8.707 8l3.647-3.646-.707-.708L8 7.293 4.354 3.646l-.707.708L7.293 8l-3.646 3.646.707.708L8 8.707z"/></svg></span>';
        }
        // Streaming — category icon from the tool
        var name = (toolName || '').toLowerCase();
        if (name === 'grep_search' || name === 'semantic_search' ||
            name === 'file_search' || name === 'codebase_search') {
            return '<span class="wk-step-icon wk-active">' +
                '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M15.25 15.02l-4.57-4.57a5.49 5.49 0 1 0-.71.7l4.57 4.58.71-.71zM6.5 11a4.5 4.5 0 1 1 0-9 4.5 4.5 0 0 1 0 9z"/></svg></span>';
        }
        if (name === 'read_file') {
            return '<span class="wk-step-icon wk-active">' +
                '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M1 4.27v7.47c0 .45.3.84.74.97l6.12 1.76c.1.03.21.03.31 0l6.12-1.76c.44-.13.74-.52.74-.97V4.27c0-.45-.3-.84-.74-.97L8.17 1.54a.99.99 0 0 0-.31 0L1.74 3.3c-.44.13-.74.52-.74.97zm7 9.09l-5.5-1.58V4.86L7.5 6.3v7.06H8zm.5 0V6.3l5-1.44v6.92L8.5 13.36zM8 5.45L3.22 4.1 8 2.56l4.78 1.54L8 5.45z"/></svg></span>';
        }
        if (name === 'create_file' || name === 'edit_file' ||
            name === 'replace_string_in_file' || name === 'multi_replace_string_in_file') {
            return '<span class="wk-step-icon wk-active">' +
                '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M13.23 1h-1.46L3.52 9.25l-.16.22L1 13.59 2.41 15l4.12-2.36.22-.16L15 4.23V2.77L13.23 1zM2.41 13.59l1.51-3 1.45 1.45-2.96 1.55zm3.83-2.06L4.47 9.76l8-8 1.77 1.77-8 8z"/></svg></span>';
        }
        if (name === 'run_in_terminal' || name === 'run_command') {
            return '<span class="wk-step-icon wk-active">' +
                '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M0 3v10h16V3H0zm15 9H1V4h14v8zM8 8h4v1H8V8zM2 8l3-2v1L3 8.5 5 10v1L2 9V8z"/></svg></span>';
        }
        if (name === 'list_dir') {
            return '<span class="wk-step-icon wk-active">' +
                '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">' +
                '<path d="M2 3h12v1H2V3zm0 3h12v1H2V6zm0 3h10v1H2V9zm0 3h8v1H2v-1z"/></svg></span>';
        }
        // Default — gear
        return '<span class="wk-step-icon wk-active">' +
            '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">' +
            '<path d="M9.1 4.4L8.6 2H7.4l-.5 2.4-.7.3-2-1.3-.9.8 1.3 2-.3.7L2 7.4v1.2l2.4.5.3.7-1.3 2 .8.9 2-1.3.7.3.5 2.4h1.2l.5-2.4.7-.3 2 1.3.9-.8-1.3-2 .3-.7 2.4-.5V7.4l-2.4-.5-.3-.7 1.3-2-.8-.9-2 1.3-.7-.3zM8 11a3 3 0 1 0 0-6 3 3 0 0 0 0 6zm0-1a2 2 0 1 1 0-4 2 2 0 0 1 0 4z"/></svg></span>';
    }

    function renderWorkingStepHtml(part) {
        var toolName = toolNameOf(part);
        var toolState = toolStateOf(part);
        var callId = MarkdownRenderer.escapeHtml(toolCallIdOf(part));
        var title = '';
        if (toolState === 3 || toolState === 4) {
            title = toolPastTenseMsgOf(part) || toolTitleOf(part) || toolName || 'Tool';
        } else {
            title = toolInvocationMsgOf(part) || toolTitleOf(part) || toolName || 'Tool';
        }
        // Inline summary
        var args = parseToolInputJson(toolInputOf(part));
        var summary = summarizeToolArgs(toolName, args);
        var summaryHtml = summary
            ? ' <code class="wk-step-code">' + MarkdownRenderer.escapeHtml(summary) + '</code>'
            : '';
        // Result count for search tools
        var resultHtml = '';
        if ((toolState === 3 || toolState === 4) && toolOutputOf(part)) {
            var output = toolOutputOf(part);
            if (toolName.indexOf('search') >= 0 || toolName.indexOf('grep') >= 0) {
                var count = (output.match(/\n/g) || []).length;
                if (count > 0) resultHtml = ', ' + count + ' results';
            }
        }
        var icon = renderWorkingStepIcon(toolName, toolState);
        var stateClass = toolState === 1 ? ' wk-step-streaming' :
                         toolState === 3 ? ' wk-step-done' :
                         toolState === 4 ? ' wk-step-fail' : '';

        return '<div class="wk-step' + stateClass + '" data-call-id="' + callId + '">' +
            icon +
            '<span class="wk-step-text">' +
                MarkdownRenderer.escapeHtml(title) + summaryHtml + resultHtml +
            '</span>' +
        '</div>';
    }

    function createWorkingBoxHtml() {
        return '<div class="wk-box wk-active" data-type="workingBox" data-step-count="0">' +
            '<button class="wk-header" onclick="ChatApp._toggleWorking(this.parentElement)">' +
                '<span class="wk-header-icon">' +
                    '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">' +
                    '<path d="M7.976 10.072l4.357-7.532.894.516L7.976 12.192l-4.19-4.19.708-.708 3.482 2.778z"/>' +
                    '</svg>' +
                '</span>' +
                '<span class="wk-header-label">Working</span>' +
            '</button>' +
            '<div class="wk-body">' +
                '<div class="wk-steps"></div>' +
                '<div class="wk-spinner">' +
                    '<span class="wk-spinner-dot">&#9679;</span>' +
                    '<span class="wk-spinner-label">' +
                        workingMessages[workingMsgIdx % workingMessages.length] +
                    '</span>' +
                '</div>' +
            '</div>' +
        '</div>';
    }

    function toolNameOf(part) {
        return part.toolName || part.name || '';
    }

    function toolCallIdOf(part) {
        return part.toolCallId || part.callId || '';
    }

    function toolStateOf(part) {
        if (part.toolState !== undefined && part.toolState !== null) return part.toolState;
        if (part.state !== undefined && part.state !== null) return part.state;
        return 0;
    }

    function toolInvocationMsgOf(part) {
        return part.toolInvocationMsg || part.invocationMsg || '';
    }

    function toolPastTenseMsgOf(part) {
        return part.toolPastTenseMsg || part.pastTenseMsg || '';
    }

    function toolTitleOf(part) {
        return part.toolFriendlyTitle || part.title || '';
    }

    function toolInputOf(part) {
        return part.toolInput || part.input || '';
    }

    function toolOutputOf(part) {
        return part.toolOutput || part.output || '';
    }

    function isTerminalTool(part) {
        var name = toolNameOf(part).toLowerCase();
        return (name === 'run_in_terminal' || name === 'run_command');
    }

    function parseToolInputJson(input) {
        if (!input) return null;
        try {
            return JSON.parse(input);
        } catch (e) {
            return null;
        }
    }

    function toolStateLabel(part) {
        var s = toolStateOf(part);
        if (s === 3) return toolPastTenseMsgOf(part) || 'Ran command';
        if (s === 4) return toolPastTenseMsgOf(part) || 'Command failed';
        return toolInvocationMsgOf(part) || 'Running command';
    }

    function shortPath(fp) {
        if (!fp) return '';
        var i = Math.max(fp.lastIndexOf('/'), fp.lastIndexOf('\\'));
        return i >= 0 ? fp.substring(i + 1) : fp;
    }

    function summarizeToolArgs(name, args) {
        if (!args) return '';
        var tool = (name || '').toLowerCase();
        var pick = function(key) { return args[key] || ''; };
        if (tool === 'read_file' || tool === 'create_file' ||
            tool === 'edit_file' || tool === 'replace_string_in_file' ||
            tool === 'delete_file') {
            return shortPath(pick('filePath') || pick('path'));
        }
        if (tool === 'list_dir') {
            return shortPath(pick('path'));
        }
        if (tool === 'grep_search' || tool === 'semantic_search' ||
            tool === 'codebase_search' || tool === 'file_search') {
            return pick('query');
        }
        if (tool === 'run_in_terminal' || tool === 'run_command') {
            var cmd = pick('command');
            return cmd.length > 60 ? cmd.substring(0, 57) + '\u2026' : cmd;
        }
        if (tool === 'run_subagent' || tool === 'subagent') {
            return pick('goal') || pick('prompt') || pick('task') || pick('message');
        }
        return '';
    }

    function formatToolInput(toolName, raw) {
        var args = parseToolInputJson(raw);
        if (!args || typeof args !== 'object') {
            return raw || '';
        }
        var lines = [];
        var tool = (toolName || '').toLowerCase();
        var primary = summarizeToolArgs(toolName, args);
        if (primary) lines.push(primary);
        var keys = Object.keys(args);
        for (var i = 0; i < keys.length; i++) {
            var k = keys[i];
            if (k === 'command' || k === 'filePath' || k === 'path' ||
                k === 'query' || k === 'prompt' || k === 'goal' || k === 'task') {
                continue;
            }
            var v = args[k];
            var s = '';
            try { s = JSON.stringify(v); } catch (e) { s = String(v); }
            if (s.length > 200) s = s.slice(0, 197) + '...';
            lines.push(k + ': ' + s);
        }
        if (lines.length === 0) {
            try { return JSON.stringify(args); } catch (e) { return raw || ''; }
        }
        return lines.join('\n');
    }

    function renderTerminalToolPart(part) {
        var toolState = toolStateOf(part);
        var callId = MarkdownRenderer.escapeHtml(toolCallIdOf(part));
        var args = parseToolInputJson(toolInputOf(part)) || {};
        var cmd = args.command || '';
        var label = toolStateLabel(part);
        var stateClass = toolCardStateClass(toolState);
        var statusIcon = toolStatusIcon(toolState);

        // VS Code style: inline command with chevron
        var commandHtml = '';
        if (cmd) {
            commandHtml = '<span class="tool-card-summary-inline">' +
                '<span class="terminal-prompt">&#x276F;</span> ' +
                MarkdownRenderer.escapeHtml(cmd) + '</span>';
        }

        var output = toolOutputOf(part);
        var hasOutput = !!output;
        var outputHtml = hasOutput ?
            '<div class="tool-card-detail hidden">' +
                '<div class="tool-detail-section">' +
                    '<pre class="tool-io">' + MarkdownRenderer.escapeHtml(output) + '</pre>' +
                '</div></div>' : '';

        var hasDetail = hasOutput;
        var chevronHtml = hasDetail ?
            '<span class="tool-card-chevron">&#x25B6;</span>' : '';

        return '<div class="chat-tool-invocation-part" data-call-id="' + callId + '">' +
            '<div class="tool-card tool-card-terminal' + stateClass + '">' +
                '<div class="tool-card-header" onclick="ChatApp._toggleCardDetail(this)">' +
                    chevronHtml +
                    '<span class="tool-card-icon">' + statusIcon + '</span>' +
                    '<span class="tool-card-title">' + MarkdownRenderer.escapeHtml(label) + '</span>' +
                    commandHtml +
                '</div>' +
                outputHtml +
            '</div></div>';
    }

    function renderToolInvocationPart(part) {
        if (isTerminalTool(part)) {
            return renderTerminalToolPart(part);
        }
        var toolState = toolStateOf(part);
        var callId = MarkdownRenderer.escapeHtml(toolCallIdOf(part));
        var toolName = toolNameOf(part);
        var stateClass = toolCardStateClass(toolState);

        // VS Code style: status icon (spinner/check/error) instead of category icon
        var statusIcon = toolStatusIcon(toolState);

        // Title: past tense when complete, invocation msg otherwise
        var title = '';
        if (toolState === 3 || toolState === 4) {
            title = toolPastTenseMsgOf(part) || toolTitleOf(part) || toolName || 'Tool';
        } else {
            title = toolInvocationMsgOf(part) || toolTitleOf(part) || toolName || 'Tool';
        }

        // Summary (file path, search query, etc.)
        var args = parseToolInputJson(toolInputOf(part));
        var summary = summarizeToolArgs(toolName, args);
        var summaryInline = summary ?
            ('<span class="tool-card-summary-inline">' +
                MarkdownRenderer.escapeHtml(summary) + '</span>') : '';

        // Expandable detail
        var detailHtml = buildCardDetailHtml(toolName, toolInputOf(part), toolOutputOf(part));
        var hasDetail = detailHtml.length > 0;

        // Chevron — VS Code style expand/collapse indicator
        var chevronHtml = hasDetail ?
            '<span class="tool-card-chevron">&#x25B6;</span>' : '';

        // Confirmation widget
        var confirmHtml = (toolState === 2) ? buildConfirmHtml(callId, toolName) : '';

        return '<div class="chat-tool-invocation-part" data-call-id="' + callId + '">' +
            '<div class="tool-card' + stateClass + '">' +
                '<div class="tool-card-header" onclick="ChatApp._toggleCardDetail(this)">' +
                    chevronHtml +
                    '<span class="tool-card-icon">' + statusIcon + '</span>' +
                    '<span class="tool-card-title">' + MarkdownRenderer.escapeHtml(title) + '</span>' +
                    summaryInline +
                '</div>' +
                detailHtml +
                confirmHtml +
            '</div></div>';
    }

    function renderWorkspaceEditPart(part) {
        var files = part.files || [];
        if (files.length === 0) return '';

        var html = '<div class="chat-workspace-edit" data-type="workspaceEdit">';
        for (var i = 0; i < files.length; i++) {
            var f = files[i];
            var path = f.path || f.name || 'file';
            var escapedPath = MarkdownRenderer.escapeHtml(path);
            var fileName = path.split('/').pop().split('\\').pop();

            // File icon based on action
            var icon = '&#x1F4C4;'; // document
            if (f.action === 'created' || f.action === 1) icon = '&#x2795;'; // plus
            else if (f.action === 'deleted' || f.action === 2) icon = '&#x1F5D1;'; // trash

            // Stats
            var statsHtml = '';
            var insertions = (f.insertions !== undefined) ? f.insertions : (f.ins || 0);
            var deletions = (f.deletions !== undefined) ? f.deletions : (f.del || 0);
            if (insertions > 0 || deletions > 0) {
                statsHtml = '<span class="file-stats">';
                if (insertions > 0) statsHtml += '<span class="stat-add">+' + insertions + '</span> ';
                if (deletions > 0) statsHtml += '<span class="stat-del">-' + deletions + '</span>';
                statsHtml += '</span>';
            }

            html += '<div class="workspace-edit-file">' +
                '<div class="file-info">' +
                    '<span class="file-icon">' + icon + '</span>' +
                    '<span class="file-name" onclick="ChatApp._fileClick(\'' + escapedPath + '\')">' +
                        MarkdownRenderer.escapeHtml(fileName) +
                    '</span>' +
                    statsHtml +
                '</div>' +
                '<div class="workspace-edit-actions">' +
                    '<button class="ws-action-btn primary" onclick="ChatApp._keepFile(' + i +
                        ', \'' + escapedPath + '\')">Keep</button>' +
                    '<button class="ws-action-btn" onclick="ChatApp._undoFile(' + i +
                        ', \'' + escapedPath + '\')">Undo</button>' +
                '</div>' +
            '</div>';

            // Diff preview with line numbers and gutter
            if (f.diff) {
                html += '<div class="diff-content"><table class="diff-table">';
                var diffLines = f.diff.split('\n');
                var oldLine = 0, newLine = 0;
                for (var j = 0; j < diffLines.length; j++) {
                    var dl = diffLines[j];
                    var cls = '';
                    var gutterCls = 'diff-gutter';
                    var oldNum = '', newNum = '';
                    var gutterSign = ' ';
                    if (dl.charAt(0) === '+') {
                        cls = 'diff-line-add';
                        gutterCls += ' diff-gutter-add';
                        newLine++;
                        newNum = newLine;
                        gutterSign = '+';
                    } else if (dl.charAt(0) === '-') {
                        cls = 'diff-line-del';
                        gutterCls += ' diff-gutter-del';
                        oldLine++;
                        oldNum = oldLine;
                        gutterSign = '\u2212';
                    } else if (dl.substring(0, 2) === '@@') {
                        cls = 'diff-line-hunk';
                        var hunkMatch = dl.match(/@@ -(\d+)(?:,\d+)? \+(\d+)/);
                        if (hunkMatch) {
                            oldLine = parseInt(hunkMatch[1], 10) - 1;
                            newLine = parseInt(hunkMatch[2], 10) - 1;
                        }
                        gutterSign = ' ';
                    } else {
                        oldLine++; newLine++;
                        oldNum = oldLine; newNum = newLine;
                    }
                    // Hunk headers show full text, other lines strip the +/-/space prefix
                    var lineText = (cls === 'diff-line-hunk') ? dl : dl.substring(1);
                    html += '<tr class="' + cls + '">' +
                        '<td class="diff-ln diff-ln-old">' + oldNum + '</td>' +
                        '<td class="diff-ln diff-ln-new">' + newNum + '</td>' +
                        '<td class="' + gutterCls + '">' + gutterSign + '</td>' +
                        '<td class="diff-text">' + MarkdownRenderer.escapeHtml(lineText) + '</td>' +
                    '</tr>';
                }
                html += '</table></div>';
            }
        }
        html += '</div>';
        return html;
    }

    function renderFollowupsPart(part) {
        var items = part.items || [];
        if (items.length === 0) return '';

        var html = '<div class="interactive-session-followups" data-type="followups">';
        for (var i = 0; i < items.length; i++) {
            var msg = typeof items[i] === 'string' ? items[i] : (items[i].message || items[i].label || '');
            html += '<button class="followup-pill" onclick="ChatApp._followup(\'' +
                MarkdownRenderer.escapeHtml(msg) + '\')">' +
                MarkdownRenderer.escapeHtml(msg) + '</button>';
        }
        html += '</div>';
        return html;
    }

    function renderConfirmationPart(part) {
        var callId = MarkdownRenderer.escapeHtml(toolCallIdOf(part));
        var title = MarkdownRenderer.escapeHtml(part.confirmTitle || part.confirmationTitle || part.title || 'Confirm');
        var msg = MarkdownRenderer.escapeHtml(part.confirmMessage || part.confirmationMessage || part.message || '');

        // Warning triangle icon
        var warnIcon = '<span class="confirmation-icon">' +
            '<svg viewBox="0 0 16 16"><path d="M7.56 1h.88l6.54 12.26-.44.74H1.46L1.02 13.26 7.56 1zM8 2.28L2.28 13h11.44L8 2.28zM8.625 12v-1h-1.25v1h1.25zm-1.25-2V6h1.25v4h-1.25z"/></svg>' +
            '</span>';

        return '<div class="chat-confirmation-widget" data-type="confirmation" data-call-id="' + callId + '">' +
            '<div class="confirmation-header" onclick="ChatApp._toggleConfirmation(this.parentElement)">' +
                warnIcon +
                '<span class="confirmation-title">' + title + '</span>' +
                '<span class="confirmation-chevron">&#x25BC;</span>' +
            '</div>' +
            '<div class="confirmation-body">' +
                (msg ? '<div class="confirmation-msg">' + msg + '</div>' : '') +
                '<div class="confirmation-actions">' +
                    '<button class="ws-action-btn primary" onclick="ChatApp._confirmTool(\'' +
                        callId + '\', 2)">Always Allow</button>' +
                    '<button class="ws-action-btn primary" onclick="ChatApp._confirmTool(\'' +
                        callId + '\', 1)">Allow</button>' +
                    '<button class="ws-action-btn" onclick="ChatApp._confirmTool(\'' +
                        callId + '\', 0)">Deny</button>' +
                '</div>' +
            '</div>' +
        '</div>';
    }

    function renderWarningPart(part) {
        var text = MarkdownRenderer.escapeHtml(part.warningText || part.text || part.markdownText || '');
        var warnIcon = '<span class="notification-icon">' +
            '<svg viewBox="0 0 16 16"><path d="M7.56 1h.88l6.54 12.26-.44.74H1.46L1.02 13.26 7.56 1zM8 2.28L2.28 13h11.44L8 2.28zM8.625 12v-1h-1.25v1h1.25zm-1.25-2V6h1.25v4h-1.25z"/></svg>' +
            '</span>';
        return '<div class="warning-box" data-type="warning">' +
            warnIcon +
            '<span class="notification-text">' + text + '</span>' +
        '</div>';
    }

    function renderErrorPart(part) {
        var text = MarkdownRenderer.escapeHtml(part.errorText || part.text || part.markdownText || '');
        var errIcon = '<span class="notification-icon">' +
            '<svg viewBox="0 0 16 16"><path d="M8 1a7 7 0 1 0 0 14A7 7 0 0 0 8 1zm0 1a6 6 0 1 1 0 12A6 6 0 0 1 8 2zm-.75 3h1.5v5h-1.5V5zm0 6h1.5v1.5h-1.5V11z"/></svg>' +
            '</span>';
        return '<div class="error-box" data-type="error">' +
            errIcon +
            '<span class="notification-text">' + text + '</span>' +
        '</div>';
    }

    function renderProgressPart(part) {
        var label = MarkdownRenderer.escapeHtml(part.progressLabel || part.label || 'Working...');
        return '<div class="progress-box streaming" data-type="progress">' +
            '<div class="progress-spinner"></div>' +
            '<span class="progress-label">' + label + '</span>' +
        '</div>';
    }

    function renderFileTreePart(part) {
        var paths = part.paths || [];
        var html = '<div class="file-tree" data-type="fileTree">';
        for (var i = 0; i < paths.length; i++) {
            html += '<div class="file-tree-item" onclick="ChatApp._fileClick(\'' +
                MarkdownRenderer.escapeHtml(paths[i]) + '\')">' +
                MarkdownRenderer.escapeHtml(paths[i]) + '</div>';
        }
        html += '</div>';
        return html;
    }

    function renderCommandButtonPart(part) {
        return '<button class="command-button" data-type="commandButton" onclick="ChatApp._commandBtn(\'' +
            MarkdownRenderer.escapeHtml(part.command || '') + '\')">' +
            MarkdownRenderer.escapeHtml(part.label || 'Run') +
        '</button>';
    }

    function renderSubagentPart(part) {
        var collapsed = part._collapsed ? ' collapsed' : '';
        var streamingClass = part._streaming ? ' streaming' : '';
        var agentName = MarkdownRenderer.escapeHtml(part.agentName || part.name || 'Agent');
        var status = part._streaming ? 'Running...' : (part._error ? 'Failed' : 'Completed');

        // Agent icon (people/group icon)
        var agentIcon = '<span class="subagent-icon">' +
            '<svg viewBox="0 0 16 16"><path d="M8 1a2 2 0 1 1 0 4 2 2 0 0 1 0-4zm5 3a1.5 1.5 0 1 1 0 3 1.5 1.5 0 0 1 0-3zM3 4a1.5 1.5 0 1 1 0 3 1.5 1.5 0 0 1 0-3zm5 4c2.21 0 4 1.12 4 2.5V12H4v-1.5C4 9.12 5.79 8 8 8zm5 1c1.1 0 2 .67 2 1.5V12h-2v-1.5c0-.54-.17-1.05-.5-1.5zm-10 0c-.33.45-.5.96-.5 1.5V12H1v-1.5C1 9.67 1.9 9 3 9z"/></svg>' +
            '</span>';

        // Inner content
        var bodyHtml = '';
        var innerParts = part.parts || [];
        for (var i = 0; i < innerParts.length; i++) {
            bodyHtml += renderContentPart(innerParts[i], false);
        }
        // If there's a result text
        if (part.resultText) {
            bodyHtml += '<div class="rendered-markdown">' +
                MarkdownRenderer.render(part.resultText, false) +
            '</div>';
        }

        return '<div class="chat-subagent-box' + collapsed + streamingClass + '" data-type="subagent">' +
            '<button class="subagent-header-btn" onclick="ChatApp._toggleSubagent(this.parentElement)">' +
                agentIcon +
                '<span class="subagent-label">' + agentName + '</span>' +
                '<span class="subagent-chevron">&#x25BC;</span>' +
                '<span class="subagent-status">' + MarkdownRenderer.escapeHtml(status) + '</span>' +
            '</button>' +
            '<div class="subagent-content">' +
                '<div class="subagent-body">' + bodyHtml + '</div>' +
            '</div>' +
        '</div>';
    }

    function contextChipIcon(label, path) {
        var ext = '';
        var src = path || label || '';
        var dotIdx = src.lastIndexOf('.');
        if (dotIdx !== -1) ext = src.substring(dotIdx + 1).toLowerCase();
        // Language-specific icons
        var iconMap = {
            'cpp': '<svg viewBox="0 0 16 16" width="14" height="14"><path fill="#519aba" d="M2 3v10h12V3H2zm6.5 7.5h-1V9H6v1.5H5V7h1v1.5h1.5V7h1v3.5zm4 0h-1V9H10v1.5H9V7h1v1.5h1.5V7h1v3.5z"/></svg>',
            'h': '<svg viewBox="0 0 16 16" width="14" height="14"><path fill="#a074c4" d="M2 3v10h12V3H2zm7 7.5h-1V9H6v1.5H5V6h1v2h2V6h1v4.5z"/></svg>',
            'hpp': '<svg viewBox="0 0 16 16" width="14" height="14"><path fill="#a074c4" d="M2 3v10h12V3H2zm7 7.5h-1V9H6v1.5H5V6h1v2h2V6h1v4.5z"/></svg>',
            'py': '<svg viewBox="0 0 16 16" width="14" height="14"><path fill="#3572A5" d="M6 2C4.34 2 3 3.34 3 5v2h4V6H5V5c0-.55.45-1 1-1h4c.55 0 1 .45 1 1v1.5c0 .83-.67 1.5-1.5 1.5H6.46C5.1 8 4 9.1 4 10.46V13h5c1.66 0 3-1.34 3-5V5c0-1.66-1.34-3-3-3H6z"/></svg>',
            'js': '<svg viewBox="0 0 16 16" width="14" height="14"><path fill="#f0db4f" d="M2 2h12v12H2V2zm6.6 10c.9 0 1.6-.4 2-1.1l-1-.6c-.2.4-.5.6-.9.6-.6 0-.9-.4-.9-1V7h-1.3v3c0 1.2.8 2 2.1 2zm-3.4-.1c.8 0 1.4-.3 1.7-.9l-1-.5c-.2.3-.4.4-.7.4-.4 0-.7-.3-.7-.9V9h-.2V7.9c0-1.2.8-1.9 1.8-1.9z"/></svg>',
            'ts': '<svg viewBox="0 0 16 16" width="14" height="14"><path fill="#3178c6" d="M2 2h12v12H2V2zm5.5 5v1H6v4H5V8H3.5V7h4zm1 0h3.9v1H11v1.3c.4-.2.9-.3 1.3-.3.8 0 1.2.5 1.2 1.2V12h-1v-1.6c0-.4-.2-.6-.5-.6-.3 0-.6.1-1 .3V12H10V7z"/></svg>',
            'json': '<svg viewBox="0 0 16 16" width="14" height="14"><path fill="#cbcb41" d="M5 3c0-.55.45-1 1-1h4c.55 0 1 .45 1 1v2h-1.5V4h-3v3h2V8.5h-2v3h3v-1h1.5v2c0 .55-.45 1-1 1H6c-.55 0-1-.45-1-1V3z"/></svg>',
            'rs': '<svg viewBox="0 0 16 16" width="14" height="14"><path fill="#dea584" d="M8 2a6 6 0 1 0 0 12A6 6 0 0 0 8 2zm-.5 3h2c.83 0 1.5.67 1.5 1.5S10.33 8 9.5 8H9v2.5H7.5V5z"/></svg>',
            'cmake': '<svg viewBox="0 0 16 16" width="14" height="14"><path fill="#6d8086" d="M8 1L1 15h14L8 1zm0 4l3.5 8h-7L8 5z"/></svg>',
            'md': '<svg viewBox="0 0 16 16" width="14" height="14"><path fill="#519aba" d="M2 3v10h12V3H2zm2 8V5h1.5l1.5 2 1.5-2H10v6H8.5V7.5L7 9.5 5.5 7.5V11H4zm8-1.5L10 7h1.5V5h1v2H14l-2 2.5z"/></svg>'
        };
        if (iconMap[ext]) return iconMap[ext];
        // Generic file icon
        return '<svg viewBox="0 0 16 16" width="14" height="14"><path fill="var(--fg-dimmed)" d="M4 1h5.5L13 4.5V14a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V2a1 1 0 0 1 1-1zm5 1v3h3L9 2z"/></svg>';
    }

    function renderReferences(refs) {
        if (!refs || refs.length === 0) return '';
        var html = '<div class="chat-used-context">' +
            '<div class="chat-used-context-label">Context</div>' +
            '<div class="chat-used-context-list">';
        for (var i = 0; i < refs.length; i++) {
            var r = refs[i] || {};
            var label = r.label || '';
            var tip = r.tooltip || '';
            var path = r.filePath || '';
            var click = path ? (' onclick="ChatApp._openContext(\'' +
                MarkdownRenderer.escapeHtml(path).replace(/'/g, '\\\'') + '\')"') : '';
            var icon = '<span class="context-chip-icon">' + contextChipIcon(label, path) + '</span>';
            html += '<span class="context-chip"' + click +
                (tip ? (' title="' + MarkdownRenderer.escapeHtml(tip) + '"') : '') +
                '>' + icon + MarkdownRenderer.escapeHtml(label) + '</span>';
        }
        html += '</div></div>';
        return html;
    }

    // Type → renderer dispatch
    var partRenderers = {
        0: renderMarkdownPart,
        1: renderThinkingPart,
        2: renderToolInvocationPart,
        3: renderWorkspaceEditPart,
        4: renderFollowupsPart,
        5: renderConfirmationPart,
        6: renderWarningPart,
        7: renderErrorPart,
        8: renderProgressPart,
        9: renderFileTreePart,
        10: renderCommandButtonPart,
        11: renderSubagentPart
    };

    var partRenderersByName = {
        markdown: renderMarkdownPart,
        thinking: renderThinkingPart,
        toolInvocation: renderToolInvocationPart,
        workspaceEdit: renderWorkspaceEditPart,
        followup: renderFollowupsPart,
        followups: renderFollowupsPart,
        confirmation: renderConfirmationPart,
        warning: renderWarningPart,
        error: renderErrorPart,
        progress: renderProgressPart,
        fileTree: renderFileTreePart,
        commandButton: renderCommandButtonPart,
        subagent: renderSubagentPart
    };

    function renderContentPart(part, streaming) {
        var t = part.type;
        var renderer = partRenderers[t] || partRenderersByName[t];
        if (renderer) return renderer(part, streaming);
        return '<div class="warning-box">Unknown part type: ' + part.type + '</div>';
    }

    // ── Turn Rendering — VS Code interactive-item-container structure ─────

    function createTurnHtml(turnIndex, turn) {
        var turnId = turn.id || ('turn-' + turnIndex);
        var isUser = turn.userMessage !== undefined && turn.userMessage !== null;

        var html = '<div class="interactive-item-container' +
            (isUser ? ' interactive-request' : ' interactive-response') +
            '" id="turn-' + turnIndex + '" data-turn-id="' +
            MarkdownRenderer.escapeHtml(turnId) + '" role="article">';

        // ── User request section ─────────────────────────────────────────
        if (isUser && turn.userMessage) {
            // Header: avatar + name
            html += '<div class="header">' +
                '<div class="user">' +
                    '<div class="avatar-container">' +
                        USER_AVATAR +
                    '</div>' +
                    '<span class="username">You</span>' +
                '</div>' +
            '</div>';

            // User message value
            html += '<div class="value">' +
                '<div class="rendered-markdown">' +
                    MarkdownRenderer.render(turn.userMessage, false) +
                '</div>' +
            '</div>';
        }

        // ── Response section ─────────────────────────────────────────────
        var parts = turn.parts || [];
        if (parts.length > 0 || !isUser) {
            // AI header
            html += '<div class="header"' + (isUser ? ' style="margin-top:16px"' : '') + '>' +
                '<div class="user">' +
                    '<div class="avatar-container">' +
                        COPILOT_AVATAR +
                    '</div>' +
                    '<span class="username">Copilot</span>' +
                '</div>' +
            '</div>';

            html += '<div class="value" id="parts-' + turnIndex + '">';
            if (turn.references && turn.references.length > 0) {
                html += renderReferences(turn.references);
            }
            var turnIsStreaming = (turn.state === 0);
            var inWorkingBox = false;
            var wkStepCount = 0;
            var wkBoxStartIdx = 0;
            for (var i = 0; i < parts.length; i++) {
                var isLast = (i === parts.length - 1);
                var pt = parts[i];
                var isToolPart = (pt.type === 2 || pt.type === 'toolInvocation');
                if (isToolPart) {
                    if (!inWorkingBox) {
                        inWorkingBox = true;
                        wkStepCount = 0;
                        wkBoxStartIdx = html.length;
                        var wkActiveClass = turnIsStreaming ? ' wk-active' : ' wk-collapsed';
                        html += '<div class="wk-box' + wkActiveClass + '" data-type="workingBox">' +
                            '<button class="wk-header" onclick="ChatApp._toggleWorking(this.parentElement)">' +
                                '<span class="wk-header-icon">' +
                                    '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">' +
                                    '<path d="M7.976 10.072l4.357-7.532.894.516L7.976 12.192l-4.19-4.19.708-.708 3.482 2.778z"/>' +
                                    '</svg>' +
                                '</span>' +
                                '<span class="wk-header-label">__WK_LABEL__</span>' +
                            '</button>' +
                            '<div class="wk-body"><div class="wk-steps">';
                    }
                    html += renderWorkingStepHtml(pt);
                    wkStepCount++;
                } else {
                    if (inWorkingBox) {
                        html += '</div>'; // close .wk-steps
                        if (turnIsStreaming) {
                            html += '<div class="wk-spinner">' +
                                '<span class="wk-spinner-dot">&#9679;</span>' +
                                '<span class="wk-spinner-label">' +
                                    workingMessages[workingMsgIdx % workingMessages.length] +
                                '</span></div>';
                        }
                        html += '</div></div>'; // close .wk-body, .wk-box
                        var wkLabel = turnIsStreaming ? 'Working'
                            : 'Finished with ' + wkStepCount + ' step' + (wkStepCount === 1 ? '' : 's');
                        html = html.substring(0, wkBoxStartIdx) + html.substring(wkBoxStartIdx).replace('__WK_LABEL__', wkLabel);
                        inWorkingBox = false;
                    }
                    html += renderContentPart(pt, turnIsStreaming && isLast);
                }
            }
            if (inWorkingBox) {
                html += '</div>'; // close .wk-steps
                if (turnIsStreaming) {
                    html += '<div class="wk-spinner">' +
                        '<span class="wk-spinner-dot">&#9679;</span>' +
                        '<span class="wk-spinner-label">' +
                            workingMessages[workingMsgIdx % workingMessages.length] +
                        '</span></div>';
                }
                html += '</div></div>'; // close .wk-body, .wk-box
                var wkLabel2 = turnIsStreaming ? 'Working'
                    : 'Finished with ' + wkStepCount + ' step' + (wkStepCount === 1 ? '' : 's');
                html = html.substring(0, wkBoxStartIdx) + html.substring(wkBoxStartIdx).replace('__WK_LABEL__', wkLabel2);
            }
            html += '</div>';
        }

        // ── Footer toolbar (feedback) ────────────────────────────────────
        var showFooter = !isUser || parts.length > 0;
        if (showFooter && turn.state !== 0) {
            html += renderFooterToolbar(turnId, turn.feedback);
        }

        html += '</div>';
        return html;
    }

    function renderFooterToolbar(turnId, feedback) {
        var upActive = feedback === 1 ? ' active' : '';
        var downActive = feedback === 2 ? ' active' : '';
        return '<div class="chat-footer-toolbar">' +
            '<button class="feedback-btn' + upActive +
                '" onclick="ChatApp._feedback(\'' + turnId + '\', true)" title="Helpful">&#x1F44D;</button>' +
            '<button class="feedback-btn' + downActive +
                '" onclick="ChatApp._feedback(\'' + turnId + '\', false)" title="Not helpful">&#x1F44E;</button>' +
        '</div>';
    }

    // ── Public API (called from C++ via ChatJSBridge) ────────────────────────

    var api = {};

    api.addTurn = function(turnIndex, turnJson) {
        turns[turnIndex] = turnJson;

        welcomeEl.style.display = 'none';
        transcriptEl.style.display = 'block';

        var html = createTurnHtml(turnIndex, turnJson);
        transcriptEl.insertAdjacentHTML('beforeend', html);

        scheduleScroll();
    };

    api.appendMarkdownDelta = function(turnIndex, delta) {
        var turn = turns[turnIndex];
        if (!turn) return;

        var parts = turn.parts = turn.parts || [];
        var lastPart = parts.length > 0 ? parts[parts.length - 1] : null;

        if (!lastPart || lastPart.type !== 0) {
            lastPart = { type: 0, markdownText: '' };
            parts.push(lastPart);
        }

        lastPart.markdownText = (lastPart.markdownText || '') + delta;

        // Ensure parts container exists
        var partsEl = document.getElementById('parts-' + turnIndex);
        if (!partsEl) {
            var turnEl = document.getElementById('turn-' + turnIndex);
            if (!turnEl) return;

            if (!turnEl.querySelector('.value[id]')) {
                var headerHtml = '<div class="header" style="margin-top:16px">' +
                    '<div class="user">' +
                        '<div class="avatar-container">' +
                            COPILOT_AVATAR +
                        '</div>' +
                        '<span class="username">Copilot</span>' +
                    '</div>' +
                '</div>' +
                '<div class="value" id="parts-' + turnIndex + '"></div>';
                turnEl.insertAdjacentHTML('beforeend', headerHtml);
                partsEl = document.getElementById('parts-' + turnIndex);
            }
        }

        if (!partsEl) return;

        // Find or create last markdown div
        var mdDivs = partsEl.querySelectorAll('[data-type="markdown"]');
        var mdCount = parts.filter(function(p) { return p.type === 0; }).length;

        if (mdDivs.length === 0 || mdDivs.length < mdCount) {
            partsEl.insertAdjacentHTML('beforeend',
                '<div class="rendered-markdown" data-type="markdown"></div>');
            mdDivs = partsEl.querySelectorAll('[data-type="markdown"]');
        }

        var mdDiv = mdDivs[mdDivs.length - 1];
        mdDiv.innerHTML = MarkdownRenderer.render(lastPart.markdownText, true);

        scheduleScroll();
    };

    api.appendThinkingDelta = function(turnIndex, delta) {
        var turn = turns[turnIndex];
        if (!turn) return;

        var parts = turn.parts = turn.parts || [];
        var lastPart = parts.length > 0 ? parts[parts.length - 1] : null;

        if (!lastPart || lastPart.type !== 1) {
            lastPart = { type: 1, thinkingText: '', thinkingCollapsed: false, _streaming: true };
            parts.push(lastPart);
        }

        lastPart.thinkingText = (lastPart.thinkingText || '') + delta;

        // Rotate the status message periodically
        thinkingMsgIndex = Math.floor((lastPart.thinkingText || '').length / 120) % thinkingMessages.length;

        var partsEl = document.getElementById('parts-' + turnIndex);
        if (!partsEl) return;

        var thinkingDivs = partsEl.querySelectorAll('[data-type="thinking"]');
        if (thinkingDivs.length === 0) {
            partsEl.insertAdjacentHTML('beforeend', renderThinkingPart(lastPart));
        } else {
            var thinkingDiv = thinkingDivs[thinkingDivs.length - 1];
            var bodyEl = thinkingDiv.querySelector('.thinking-text');
            if (bodyEl) {
                bodyEl.innerHTML = MarkdownRenderer.escapeHtml(lastPart.thinkingText).replace(/\n/g, '<br>');
            }
            // Update status message
            var statusEl = thinkingDiv.querySelector('.thinking-status-msg');
            if (statusEl) {
                statusEl.textContent = '\u2014 ' + thinkingMessages[thinkingMsgIndex % thinkingMessages.length] + '...';
            }
            thinkingDiv.classList.add('streaming');
            thinkingDiv.classList.remove('collapsed');
        }

        scheduleScroll();
    };

    api.addContentPart = function(turnIndex, partJson) {
        var turn = turns[turnIndex];
        if (!turn) return;

        var parts = turn.parts = turn.parts || [];

        // Auto-collapse last thinking if we get a non-thinking part
        if (partJson.type !== 1 && parts.length > 0) {
            var prev = parts[parts.length - 1];
            if (prev.type === 1) {
                prev.thinkingCollapsed = true;
                prev._streaming = false;
                var partsEl2 = document.getElementById('parts-' + turnIndex);
                if (partsEl2) {
                    var thinkingDivs = partsEl2.querySelectorAll('[data-type="thinking"]');
                    var last = thinkingDivs[thinkingDivs.length - 1];
                    if (last) {
                        last.classList.add('collapsed');
                        last.classList.remove('streaming');
                        // Swap spinner for chevron
                        var spinnerEl = last.querySelector('.thinking-spinner-icon');
                        if (spinnerEl) {
                            spinnerEl.outerHTML = '<span class="thinking-chevron">&#x25BC;</span>';
                        }
                        // Remove status message
                        var statusMsg = last.querySelector('.thinking-status-msg');
                        if (statusMsg) statusMsg.remove();
                    }
                }
            }
        }

        parts.push(partJson);

        var partsEl = document.getElementById('parts-' + turnIndex);
        if (!partsEl) {
            var turnEl = document.getElementById('turn-' + turnIndex);
            if (!turnEl) return;
            if (!turnEl.querySelector('.value[id]')) {
                var headerHtml = '<div class="header" style="margin-top:16px">' +
                    '<div class="user">' +
                        '<div class="avatar-container">' +
                            COPILOT_AVATAR +
                        '</div>' +
                        '<span class="username">Copilot</span>' +
                    '</div>' +
                '</div>' +
                '<div class="value" id="parts-' + turnIndex + '"></div>';
                turnEl.insertAdjacentHTML('beforeend', headerHtml);
                partsEl = document.getElementById('parts-' + turnIndex);
            }
        }
        if (!partsEl) return;

        // Tool invocations go into the Working box (VS Code style)
        if (partJson.type === 2 || partJson.type === 'toolInvocation') {
            var wkBox = partsEl.querySelector('.wk-box');
            if (!wkBox) {
                partsEl.insertAdjacentHTML('beforeend', createWorkingBoxHtml());
                wkBox = partsEl.querySelector('.wk-box');
            }
            var stepsEl = wkBox.querySelector('.wk-steps');
            if (stepsEl) {
                stepsEl.insertAdjacentHTML('beforeend', renderWorkingStepHtml(partJson));
            }
            // Track step count
            var count = parseInt(wkBox.getAttribute('data-step-count') || '0', 10) + 1;
            wkBox.setAttribute('data-step-count', String(count));
            // Update spinner message
            workingMsgIdx++;
            var spinLabel = wkBox.querySelector('.wk-spinner-label');
            if (spinLabel) {
                spinLabel.textContent = workingMessages[workingMsgIdx % workingMessages.length];
            }
            // Auto-scroll body to bottom
            var wkBody = wkBox.querySelector('.wk-body');
            if (wkBody) {
                wkBody.scrollTop = wkBody.scrollHeight;
            }
            scheduleScroll();
            return;
        }

        partsEl.insertAdjacentHTML('beforeend', renderContentPart(partJson, false));
        scheduleScroll();
    };

    api.updateToolState = function(turnIndex, callId, stateJson) {
        var partsEl = document.getElementById('parts-' + turnIndex);
        if (!partsEl) return;

        var toolPart = partsEl.querySelector('[data-call-id="' + callId + '"]');
        if (!toolPart) return;

        // Working box step (VS Code style)
        if (toolPart.classList.contains('wk-step')) {
            toolPart.outerHTML = renderWorkingStepHtml(stateJson);
            scheduleScroll();
            return;
        }

        // Terminal tools: full re-render for simplicity
        if (isTerminalTool(stateJson)) {
            toolPart.outerHTML = renderTerminalToolPart(stateJson);
            scheduleScroll();
            return;
        }

        var st = toolStateOf(stateJson);
        var cardEl = toolPart.querySelector('.tool-card');
        if (cardEl) {
            // Update card state class
            cardEl.classList.remove('is-streaming', 'is-complete', 'is-error');
            if (st === 1) cardEl.classList.add('is-streaming');
            else if (st === 3) cardEl.classList.add('is-complete');
            else if (st === 4) cardEl.classList.add('is-error');
        }

        // Update status icon
        var iconEl = toolPart.querySelector('.tool-card-icon');
        if (iconEl) {
            iconEl.innerHTML = toolStatusIcon(st);
        }

        // Update title (present → past tense)
        var titleEl = toolPart.querySelector('.tool-card-title');
        if (titleEl) {
            var newTitle = '';
            if (st === 3 || st === 4) {
                newTitle = toolPastTenseMsgOf(stateJson) || toolTitleOf(stateJson) || toolNameOf(stateJson) || 'Tool';
            } else {
                newTitle = toolInvocationMsgOf(stateJson) || toolTitleOf(stateJson) || toolNameOf(stateJson) || 'Tool';
            }
            titleEl.textContent = newTitle;
        }

        // Update summary inline
        var args = parseToolInputJson(toolInputOf(stateJson));
        var summary = summarizeToolArgs(toolNameOf(stateJson), args);
        var headerEl = toolPart.querySelector('.tool-card-header');
        if (headerEl) {
            var existingSummary = headerEl.querySelector('.tool-card-summary-inline');
            if (summary && !existingSummary) {
                headerEl.insertAdjacentHTML('beforeend',
                    '<span class="tool-card-summary-inline">' +
                    MarkdownRenderer.escapeHtml(summary) + '</span>');
            } else if (summary && existingSummary) {
                existingSummary.textContent = summary;
            }
        }

        // Update or create detail section
        var stInput = toolInputOf(stateJson);
        var stOutput = toolOutputOf(stateJson);
        if (stOutput || stInput) {
            var existingDetail = toolPart.querySelector('.tool-card-detail');
            var newDetailHtml = buildCardDetailHtml(toolNameOf(stateJson), stInput, stOutput);
            if (existingDetail) {
                existingDetail.outerHTML = newDetailHtml;
            } else if (cardEl) {
                // Insert before confirmation widget or at end of card
                var confirmWidget = cardEl.querySelector('.chat-confirmation-widget');
                if (confirmWidget) {
                    confirmWidget.insertAdjacentHTML('beforebegin', newDetailHtml);
                } else {
                    cardEl.insertAdjacentHTML('beforeend', newDetailHtml);
                }
                // Add chevron if not present
                if (headerEl && !headerEl.querySelector('.tool-card-chevron')) {
                    headerEl.insertAdjacentHTML('afterbegin',
                        '<span class="tool-card-chevron">&#x25B6;</span>');
                }
            }
        }

        // Remove confirmation widget if complete
        if (st === 3 || st === 4) {
            var confirmWidget = toolPart.querySelector('.chat-confirmation-widget');
            if (confirmWidget) confirmWidget.remove();
        }

        scheduleScroll();
    };

    api.finishTurn = function(turnIndex, state) {
        var turn = turns[turnIndex];
        if (!turn) return;
        turn.state = state;

        var turnEl = document.getElementById('turn-' + turnIndex);
        if (!turnEl) return;

        // Remove streaming cursors
        var cursors = turnEl.querySelectorAll('.streaming-cursor, .chat-animated-ellipsis');
        for (var i = 0; i < cursors.length; i++) {
            cursors[i].remove();
        }

        // Remove streaming class from thinking boxes
        var thinkingBoxes = turnEl.querySelectorAll('.chat-thinking-box.streaming');
        for (var j = 0; j < thinkingBoxes.length; j++) {
            thinkingBoxes[j].classList.remove('streaming');
            // Swap spinner for chevron
            var spinnerEl = thinkingBoxes[j].querySelector('.thinking-spinner-icon');
            if (spinnerEl) {
                spinnerEl.outerHTML = '<span class="thinking-chevron">&#x25BC;</span>';
            }
            // Remove status message
            var statusMsg = thinkingBoxes[j].querySelector('.thinking-status-msg');
            if (statusMsg) statusMsg.remove();
        }

        // Remove streaming shimmer from tool labels
        var streamingSteps = turnEl.querySelectorAll('.progress-step.is-streaming');
        for (var k = 0; k < streamingSteps.length; k++) {
            streamingSteps[k].classList.remove('is-streaming');
        }

        // Clean up tool cards still in streaming state
        var streamingCards = turnEl.querySelectorAll('.tool-card.is-streaming');
        for (var tc = 0; tc < streamingCards.length; tc++) {
            streamingCards[tc].classList.remove('is-streaming');
            streamingCards[tc].classList.add('is-complete');
            // Update status dot
            var statusDot = streamingCards[tc].querySelector('.tool-card-status-dot');
            if (statusDot) statusDot.classList.remove('spinning');
        }

        // Clean up streaming subagent boxes
        var subagentBoxes = turnEl.querySelectorAll('.chat-subagent-box.streaming');
        for (var s = 0; s < subagentBoxes.length; s++) {
            subagentBoxes[s].classList.remove('streaming');
            var statusEl = subagentBoxes[s].querySelector('.subagent-status');
            if (statusEl) statusEl.textContent = 'Completed';
        }

        // Remove streaming progress boxes
        var progressBoxes = turnEl.querySelectorAll('.progress-box.streaming');
        for (var p = 0; p < progressBoxes.length; p++) {
            progressBoxes[p].remove();
        }

        // Finalize Working boxes: remove spinner, deactivate shimmer, auto-collapse
        var wkBoxes = turnEl.querySelectorAll('.wk-box.wk-active');
        for (var wb = 0; wb < wkBoxes.length; wb++) {
            var box = wkBoxes[wb];
            box.classList.remove('wk-active');
            var spinner = box.querySelector('.wk-spinner');
            if (spinner) spinner.remove();
            // Mark any remaining streaming steps as done
            var activeSteps = box.querySelectorAll('.wk-step-streaming');
            for (var as = 0; as < activeSteps.length; as++) {
                activeSteps[as].classList.remove('wk-step-streaming');
                activeSteps[as].classList.add('wk-step-done');
                var iconEl = activeSteps[as].querySelector('.wk-step-icon');
                if (iconEl) {
                    iconEl.outerHTML = '<span class="wk-step-icon wk-check">' +
                        '<svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">' +
                        '<path d="M6.27 10.87h.71l4.56-6.47-.71-.5-4.2 5.94-2.26-2.03-.64.56 2.54 2.5z"/></svg></span>';
                }
            }
            // Auto-collapse like VS Code
            box.classList.add('wk-collapsed');
            // Update header label with step count
            var stepCount = parseInt(box.getAttribute('data-step-count') || '0', 10);
            if (stepCount === 0) {
                stepCount = box.querySelectorAll('.wk-step').length;
            }
            var label = box.querySelector('.wk-header-label');
            if (label) {
                label.textContent = stepCount > 0
                    ? 'Finished with ' + stepCount + ' step' + (stepCount === 1 ? '' : 's')
                    : 'Finished Working';
            }
        }

        // Mark as most recent for persistent footer visibility
        var allContainers = transcriptEl.querySelectorAll('.interactive-item-container.most-recent');
        for (var m = 0; m < allContainers.length; m++) {
            allContainers[m].classList.remove('most-recent');
        }
        turnEl.classList.add('most-recent');

        // Add footer toolbar if not present
        var existingFooter = turnEl.querySelector('.chat-footer-toolbar');
        if (!existingFooter && turn.id) {
            turnEl.insertAdjacentHTML('beforeend', renderFooterToolbar(turn.id, turn.feedback));
        }

        // Show error state
        if (state === 3) { // State::Error
            var errorMark = turnEl.querySelector('.error-box');
            if (!errorMark) {
                // Add retry button for errors
                var partsEl = document.getElementById('parts-' + turnIndex);
                if (partsEl) {
                    partsEl.insertAdjacentHTML('beforeend',
                        '<button class="ws-action-btn" style="margin-top:8px" onclick="ChatApp._retry(\'' +
                        (turn.id || '') + '\')">Retry</button>');
                }
            }
        }

        scheduleScroll();
    };

    api.setTokenUsage = function(turnIndex, promptTokens, completionTokens, totalTokens) {
        var turnEl = document.getElementById('turn-' + turnIndex);
        if (!turnEl) return;
        var footer = turnEl.querySelector('.chat-footer-toolbar');
        if (!footer) return;
        var badge = footer.querySelector('.token-usage');
        var text = totalTokens.toLocaleString() + ' tokens';
        if (badge) {
            badge.textContent = text;
        } else {
            footer.insertAdjacentHTML('beforeend',
                '<span class="token-usage">' + text + '</span>');
        }
    };

    api.clearSession = function() {
        turns = [];
        transcriptEl.innerHTML = '';
        welcomeEl.style.display = 'flex';
        transcriptEl.style.display = 'none';
    };

    // ── View State ───────────────────────────────────────────────────────────

    function updateWelcomeState(state) {
        if (welcomeTitleEl)
            welcomeTitleEl.textContent = state === 'auth' ? 'Sign In Required'
                : state === 'noProvider' ? 'No Provider Configured'
                : 'Ask Copilot';
        if (welcomeSubtitleEl)
            welcomeSubtitleEl.innerHTML = state === 'auth'
                ? 'Sign in with GitHub to use Copilot in Exorcist.'
                : state === 'noProvider'
                    ? 'Configure an AI provider to get started.'
                    : 'Ask a question or type <span class="kbd">/</span> for commands';
        if (welcomeActionEl) {
            if (state === 'auth') {
                welcomeActionEl.innerHTML = '<button class="welcome-action-btn" id="welcomeActionBtn">Sign In with GitHub</button>';
                var authBtn = document.getElementById('welcomeActionBtn');
                if (authBtn) {
                    authBtn.onclick = function() {
                        if (window.exorcist) window.exorcist.sendToHost('signInRequested', {});
                    };
                }
            } else if (state === 'noProvider') {
                welcomeActionEl.innerHTML = '<button class="welcome-action-btn secondary" id="welcomeActionBtn">Open AI Settings</button>';
                var settingsActionBtn = document.getElementById('welcomeActionBtn');
                if (settingsActionBtn) {
                    settingsActionBtn.onclick = function() {
                        if (window.exorcist) window.exorcist.sendToHost('settingsRequested', {});
                    };
                }
            } else {
                welcomeActionEl.innerHTML = '';
            }
        }
        if (suggestionsEl)
            suggestionsEl.style.display = (state === 'default') ? '' : 'none';
        if (toolCountEl)
            toolCountEl.style.display = (state === 'default') ? '' : 'none';
    }

    api.showWelcome = function(state) {
        var resolvedState = state || 'default';
        welcomeEl.style.display = 'flex';
        transcriptEl.style.display = 'none';
        welcomeEl.className = 'welcome' + (resolvedState !== 'default' ? ' ' + resolvedState : '');
        updateWelcomeState(resolvedState);
    };

    api.showTranscript = function() {
        welcomeEl.style.display = 'none';
        transcriptEl.style.display = 'block';
    };

    api.setStreamingActive = function(active) {
        isStreaming = active;
        userScrolledAway = false;
        if (active) scheduleScroll();
    };

    api.scrollToBottom = function() {
        scrollToBottom();
    };

    // ── Theme ────────────────────────────────────────────────────────────────

    api.setTheme = function(tokens) {
        var root = document.documentElement;
        var map = {
            'panelBg': '--panel-bg',
            'editorBg': '--editor-bg',
            'inputBg': '--input-bg',
            'inputBorder': '--input-border',
            'sideBarBg': '--sidebar-bg',
            'fgPrimary': '--fg-primary',
            'fgSecondary': '--fg-secondary',
            'fgDimmed': '--fg-dimmed',
            'fgMuted': '--fg-muted',
            'fgBright': '--fg-bright',
            'fgCode': '--preformat-fg',
            'linkBlue': '--link-fg',
            'accentBlue': '--accent-blue',
            'accentPurple': '--accent-purple',
            'selectionBg': '--selection-bg',
            'buttonBg': '--button-bg',
            'buttonHover': '--button-hover',
            'buttonFg': '--button-fg',
            'secondaryBtnBg': '--secondary-btn-bg',
            'secondaryBtnFg': '--secondary-btn-fg',
            'secondaryBtnHover': '--secondary-btn-hover',
            'border': '--border',
            'requestBorder': '--request-border',
            'sepLine': '--sep-line',
            'hoverBg': '--hover-bg',
            'codeBg': '--code-bg',
            'codeHeaderBg': '--code-header-bg',
            'inlineCodeBg': '--inline-code-bg',
            'errorFg': '--error-fg',
            'warningFg': '--warning-fg',
            'successFg': '--success-fg',
            'thinkingShimmer': '--thinking-shimmer',
            'thinkingFg': '--thinking-fg',
            'diffInsertBg': '--diff-insert-bg',
            'diffDeleteBg': '--diff-delete-bg',
            'diffInsertFg': '--diff-insert-fg',
            'diffDeleteFg': '--diff-delete-fg',
            'avatarBg': '--avatar-bg',
            'avatarUserBg': '--avatar-user-bg',
            'scrollTrack': '--scroll-track',
            'scrollThumb': '--scroll-thumb',
            'scrollThumbHover': '--scroll-thumb-hover'
        };

        for (var key in tokens) {
            var prop = map[key];
            if (prop) root.style.setProperty(prop, tokens[key]);
        }
    };

    api.setWelcomeSuggestions = function(suggestions) {
        if (!suggestionsEl) return;
        suggestionsEl.innerHTML = '';
        for (var i = 0; i < suggestions.length; i++) {
            var chip = document.createElement('button');
            chip.className = 'suggestion-chip';
            chip.textContent = suggestions[i];
            chip.onclick = (function(msg) {
                return function() {
                    if (window.exorcist) window.exorcist.sendToHost('suggestionClicked', { message: msg });
                };
            })(suggestions[i]);
            suggestionsEl.appendChild(chip);
        }
    };

    api.setToolCount = function(count) {
        if (toolCountEl)
            toolCountEl.textContent = count > 0 ? (count + ' tools available') : '';
        if (inputToolCountEl)
            inputToolCountEl.textContent = count > 0 ? ('\uD83D\uDD27 ' + count) : '';
    };

    // ── Input Area API (called from C++) ─────────────────────────────────────

    api.setInputText = function(text) {
        if (!inputEl) return;
        inputEl.value = text;
        autoResizeInput();
    };

    api.focusInput = function() {
        if (inputEl) inputEl.focus();
    };

    api.clearInput = function() {
        if (inputEl) {
            inputEl.value = '';
            autoResizeInput();
        }
        if (attachChipBarEl) {
            attachChipBarEl.innerHTML = '';
            attachChipBarEl.style.display = 'none';
        }
    };

    api.setStreamingState = function(streaming) {
        isStreaming = streaming;
        userScrolledAway = false;
        if (sendBtnEl) sendBtnEl.style.display = streaming ? 'none' : '';
        if (cancelBtnEl) cancelBtnEl.style.display = streaming ? '' : 'none';
        if (streamingBarEl) streamingBarEl.style.display = streaming ? '' : 'none';
        if (inputEl) inputEl.disabled = streaming;
        if (inputWrapperEl) {
            if (streaming) inputWrapperEl.classList.add('disabled');
            else inputWrapperEl.classList.remove('disabled');
        }
        if (streaming) scheduleScroll();
    };

    api.setInputEnabled = function(enabled) {
        if (inputEl) inputEl.disabled = !enabled;
        if (inputWrapperEl) {
            if (enabled) inputWrapperEl.classList.remove('disabled');
            else inputWrapperEl.classList.add('disabled');
        }
    };

    api.setSlashCommands = function(commands) {
        // commands: [{name, desc}] or ["name", ...]
        slashCommands = [];
        for (var i = 0; i < commands.length; i++) {
            var c = commands[i];
            if (typeof c === 'string') {
                slashCommands.push({ name: c, desc: '' });
            } else {
                slashCommands.push({ name: c.name || c, desc: c.desc || '' });
            }
        }
    };

    // ── Mode API ─────────────────────────────────────────────────────────────

    api.setMode = function(mode) {
        currentMode = mode;
        updateModeButtons();
    };

    function updateModeButtons() {
        for (var i = 0; i < modeBtns.length; i++) {
            var m = parseInt(modeBtns[i].getAttribute('data-mode'), 10);
            if (m === currentMode) modeBtns[i].classList.add('active');
            else modeBtns[i].classList.remove('active');
        }
        updatePlaceholder();
    }

    function updatePlaceholder() {
        if (!inputEl) return;
        var placeholders = [
            'Ask anything, @ to mention, # to attach',
            'Describe what to edit...',
            'Ask in agent mode — tools enabled'
        ];
        inputEl.placeholder = placeholders[currentMode] || placeholders[0];
    }

    // ── Model API ────────────────────────────────────────────────────────────

    api.setModels = function(modelList) {
        models = modelList || [];
    };

    api.setCurrentModel = function(id) {
        currentModelId = id;
        if (modelLabelEl) {
            var found = null;
            for (var i = 0; i < models.length; i++) {
                if (models[i].id === id) { found = models[i]; break; }
            }
            modelLabelEl.textContent = found ? (found.displayName || found.name || found.id) : (id || 'Model');
        }
    };

    api.clearModels = function() {
        models = [];
        currentModelId = '';
        if (modelLabelEl) modelLabelEl.textContent = 'Model';
    };

    api.addModel = function(modelInfo) {
        models.push(modelInfo);
    };

    // ── Thinking toggle ──────────────────────────────────────────────────────

    api.setThinkingVisible = function(visible) {
        if (thinkingBtnEl) thinkingBtnEl.style.display = visible ? '' : 'none';
    };

    api.isThinkingEnabled = function() {
        return thinkingEnabled;
    };

    // ── Header API ───────────────────────────────────────────────────────────

    api.setSessionTitle = function(title) {
        if (sessionTitleEl) sessionTitleEl.textContent = title || 'Copilot';
    };

    // ── Changes Bar API ──────────────────────────────────────────────────────

    api.showChangesBar = function(count) {
        if (!changesBarEl) return;
        if (changesLabelEl)
            changesLabelEl.textContent = count + ' file' + (count === 1 ? '' : 's') + ' changed';
        changesBarEl.style.display = '';
    };

    api.hideChangesBar = function() {
        if (changesBarEl) changesBarEl.style.display = 'none';
    };

    // ── Attachment chips API ─────────────────────────────────────────────────

    api.addAttachmentChip = function(name, index) {
        if (!attachChipBarEl) return;
        attachChipBarEl.style.display = '';
        var chip = document.createElement('span');
        chip.className = 'attach-chip';
        chip.setAttribute('data-index', index);
        chip.innerHTML = '<span class="chip-name">' + esc(name) + '</span>' +
            '<button class="chip-remove" data-index="' + index + '">&times;</button>';
        chip.querySelector('.chip-remove').onclick = function() {
            if (window.exorcist) window.exorcist.sendToHost('removeAttachment', { index: index });
        };
        attachChipBarEl.appendChild(chip);
    };

    api.removeAttachmentChip = function(index) {
        if (!attachChipBarEl) return;
        var chip = attachChipBarEl.querySelector('[data-index="' + index + '"]');
        if (chip) chip.remove();
        if (attachChipBarEl.children.length === 0)
            attachChipBarEl.style.display = 'none';
    };

    api.clearAttachmentChips = function() {
        if (attachChipBarEl) {
            attachChipBarEl.innerHTML = '';
            attachChipBarEl.style.display = 'none';
        }
    };

    // ── Internal Action Handlers (onclick → C++ bridge) ──────────────────────

    api._toggleThinking = function(boxEl) {
        if (boxEl) boxEl.classList.toggle('collapsed');
    };

    api._toggleWorking = function(boxEl) {
        if (boxEl) boxEl.classList.toggle('wk-collapsed');
    };

    api._toggleSubagent = function(boxEl) {
        if (boxEl) boxEl.classList.toggle('collapsed');
    };

    api._toggleConfirmation = function(widgetEl) {
        if (widgetEl) widgetEl.classList.toggle('collapsed');
    };

    api._toggleToolDetail = function(toggleBtn) {
        var container = toggleBtn.parentElement;
        if (!container) return;
        var body = container.querySelector('.tool-detail-body');
        if (body) {
            body.classList.toggle('hidden');
            var arrow = toggleBtn.querySelector('span');
            if (arrow) {
                arrow.innerHTML = body.classList.contains('hidden') ? '&#x25B6;' : '&#x25BC;';
            }
        }
    };

    api._toggleCardDetail = function(headerEl) {
        if (!headerEl) return;
        var card = headerEl.closest('.tool-card');
        if (!card) return;
        var detail = card.querySelector('.tool-card-detail');
        if (detail) {
            var isHidden = detail.classList.toggle('hidden');
            if (isHidden) {
                card.classList.remove('expanded');
            } else {
                card.classList.add('expanded');
            }
        }
    };

    api._copyCode = function(blockId) {
        var block = document.getElementById(blockId);
        if (!block) return;
        var code = block.querySelector('code');
        if (code && window.exorcist) {
            window.exorcist.sendToHost('copyCode', { code: code.textContent });
        }
    };

    api._insertCode = function(blockId) {
        var block = document.getElementById(blockId);
        if (!block) return;
        var code = block.querySelector('code');
        var lang = block.getAttribute('data-lang') || '';
        if (code && window.exorcist) {
            window.exorcist.sendToHost('insertCode', { code: code.textContent, language: lang });
        }
    };

    api.setMentionItems = function(trigger, items) {
        if (!mentionPopupEl || !inputEl) return;
        if (!items || items.length === 0) {
            mentionPopupEl.style.display = 'none';
            return;
        }
        var html = '';
        for (var i = 0; i < items.length; i++) {
            var it = items[i];
            html += '<div class="autocomplete-item" data-insert="' + esc(it.insertText || '') + '">' +
                '<span class="cmd-name">' + esc(it.label || '') + '</span>' +
                (it.desc ? '<span class="cmd-desc">' + esc(it.desc) + '</span>' : '') +
            '</div>';
        }
        mentionPopupEl.innerHTML = html;
        mentionPopupEl.style.display = 'block';

        var list = mentionPopupEl.querySelectorAll('.autocomplete-item');
        for (var j = 0; j < list.length; j++) {
            list[j].onclick = (function(insertText) {
                return function() {
                    insertMention(insertText);
                    mentionPopupEl.style.display = 'none';
                    inputEl.focus();
                };
            })(list[j].getAttribute('data-insert'));
        }
    };

    api._applyCode = function(blockId) {
        var block = document.getElementById(blockId);
        if (!block) return;
        var code = block.querySelector('code');
        var lang = block.getAttribute('data-lang') || '';
        var filePath = block.getAttribute('data-file') || '';
        if (code && window.exorcist) {
            window.exorcist.sendToHost('applyCode', {
                code: code.textContent,
                language: lang,
                filePath: filePath
            });
        }
    };

    api._runCode = function(blockId) {
        var block = document.getElementById(blockId);
        if (!block) return;
        var code = block.querySelector('code');
        var lang = block.getAttribute('data-lang') || '';
        if (code && window.exorcist) {
            window.exorcist.sendToHost('runCode', { code: code.textContent, language: lang });
        }
    };

    api._followup = function(message) {
        if (window.exorcist) window.exorcist.sendToHost('followupClicked', { message: message });
    };

    api._feedback = function(turnId, helpful) {
        if (window.exorcist) window.exorcist.sendToHost('feedbackGiven', { turnId: turnId, helpful: helpful });
        var turnEl = transcriptEl.querySelector('[data-turn-id="' + turnId + '"]');
        if (!turnEl) return;
        var btns = turnEl.querySelectorAll('.feedback-btn');
        for (var i = 0; i < btns.length; i++) btns[i].classList.remove('active');
        var idx = helpful ? 0 : 1;
        if (btns[idx]) btns[idx].classList.add('active');
    };

    api._confirmTool = function(callId, approval) {
        if (window.exorcist) window.exorcist.sendToHost('toolConfirmed', { callId: callId, approval: approval });
    };

    api._fileClick = function(path) {
        if (window.exorcist) window.exorcist.sendToHost('fileClicked', { path: path });
    };

    api._openContext = function(path) {
        if (window.exorcist) window.exorcist.sendToHost('fileClicked', { path: path });
    };

    api._keepFile = function(index, path) {
        if (window.exorcist) window.exorcist.sendToHost('keepFile', { index: index, path: path });
    };

    api._undoFile = function(index, path) {
        if (window.exorcist) window.exorcist.sendToHost('undoFile', { index: index, path: path });
    };

    api._commandBtn = function(cmd) {
        if (window.exorcist) window.exorcist.sendToHost('commandBtn', { command: cmd });
    };

    api._retry = function(turnId) {
        if (window.exorcist) window.exorcist.sendToHost('retryRequested', { turnId: turnId });
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // DOM Event Wiring
    // ═══════════════════════════════════════════════════════════════════════════

    // ── Header buttons ───────────────────────────────────────────────────────
    if (historyBtn) historyBtn.onclick = function() {
        if (window.exorcist) window.exorcist.sendToHost('historyRequested', {});
    };
    if (newSessionBtn) newSessionBtn.onclick = function() {
        if (window.exorcist) window.exorcist.sendToHost('newSessionRequested', {});
    };
    if (settingsBtn) settingsBtn.onclick = function() {
        if (window.exorcist) window.exorcist.sendToHost('settingsRequested', {});
    };

    // ── Changes bar ──────────────────────────────────────────────────────────
    if (keepAllBtn) keepAllBtn.onclick = function() {
        if (window.exorcist) window.exorcist.sendToHost('keepAll', {});
    };
    if (undoAllBtn) undoAllBtn.onclick = function() {
        if (window.exorcist) window.exorcist.sendToHost('undoAll', {});
    };

    // ── Send / Cancel ────────────────────────────────────────────────────────
    function sendMessage() {
        if (!inputEl) return;
        var text = inputEl.value.trim();
        if (!text || isStreaming) return;
        // Save to history
        if (inputHistory.length === 0 || inputHistory[inputHistory.length - 1] !== text)
            inputHistory.push(text);
        historyIndex = -1;
        if (window.exorcist) window.exorcist.sendToHost('sendRequested', { text: text, mode: currentMode });
        inputEl.value = '';
        autoResizeInput();
    }

    if (sendBtnEl) sendBtnEl.onclick = function() { sendMessage(); };
    if (cancelBtnEl) cancelBtnEl.onclick = function() {
        if (window.exorcist) window.exorcist.sendToHost('cancelRequested', {});
    };

    // ── Textarea keyboard ────────────────────────────────────────────────────
    if (inputEl) {
        inputEl.addEventListener('keydown', function(e) {
            if (e.key === 'Enter' && !e.shiftKey && !e.ctrlKey && !e.altKey) {
                // Check if mention popup is visible
                if (mentionPopupEl && mentionPopupEl.style.display !== 'none') {
                    var msel = mentionPopupEl.querySelector('.autocomplete-item.selected');
                    if (!msel) msel = mentionPopupEl.querySelector('.autocomplete-item');
                    if (msel) {
                        msel.click();
                        e.preventDefault();
                        return;
                    }
                }
                // Check if slash popup is visible and has a selected item
                if (slashPopupEl && slashPopupEl.style.display !== 'none') {
                    var sel = slashPopupEl.querySelector('.autocomplete-item.selected');
                    if (!sel) sel = slashPopupEl.querySelector('.autocomplete-item');
                    if (sel) {
                        sel.click();
                        e.preventDefault();
                        return;
                    }
                }
                e.preventDefault();
                sendMessage();
                return;
            }
            if (e.key === 'Escape') {
                hideAllPopups();
                return;
            }
            // Up arrow at empty → history
            if (e.key === 'ArrowUp' && inputEl.value === '' && inputHistory.length > 0) {
                e.preventDefault();
                if (historyIndex === -1) historyIndex = inputHistory.length;
                historyIndex = Math.max(0, historyIndex - 1);
                inputEl.value = inputHistory[historyIndex] || '';
                autoResizeInput();
                return;
            }
            if (e.key === 'ArrowDown' && historyIndex >= 0) {
                e.preventDefault();
                historyIndex = Math.min(inputHistory.length, historyIndex + 1);
                inputEl.value = historyIndex < inputHistory.length ? inputHistory[historyIndex] : '';
                autoResizeInput();
                return;
            }
        });

        inputEl.addEventListener('input', function() {
            autoResizeInput();
            updateSlashPopup();
            requestMentionItems();
        });
    }

    // ── Attach button ────────────────────────────────────────────────────────
    if (attachBtnEl) attachBtnEl.onclick = function() {
        if (window.exorcist) window.exorcist.sendToHost('attachFileRequested', {});
    };

    // ── Mode buttons ─────────────────────────────────────────────────────────
    for (var mi = 0; mi < modeBtns.length; mi++) {
        modeBtns[mi].onclick = (function(btn) {
            return function() {
                var mode = parseInt(btn.getAttribute('data-mode'), 10);
                currentMode = mode;
                updateModeButtons();
                if (window.exorcist) window.exorcist.sendToHost('modeChanged', { mode: mode });
            };
        })(modeBtns[mi]);
    }

    // ── Model picker ─────────────────────────────────────────────────────────
    if (modelBtnEl) modelBtnEl.onclick = function(e) {
        e.stopPropagation();
        if (modelPopupEl && modelPopupEl.style.display !== 'none') {
            modelPopupEl.style.display = 'none';
        } else {
            showModelPopup();
        }
    };

    // ── Thinking toggle ──────────────────────────────────────────────────────
    if (thinkingBtnEl) thinkingBtnEl.onclick = function() {
        thinkingEnabled = !thinkingEnabled;
        if (thinkingEnabled) thinkingBtnEl.classList.add('active');
        else thinkingBtnEl.classList.remove('active');
        if (window.exorcist) window.exorcist.sendToHost('thinkingToggled', { enabled: thinkingEnabled });
    };

    // ── Close popups on outside click ────────────────────────────────────────
    document.addEventListener('click', function(e) {
        if (modelPopupEl && modelPopupEl.style.display !== 'none') {
            if (!modelPopupEl.contains(e.target) && e.target !== modelBtnEl) {
                modelPopupEl.style.display = 'none';
            }
        }
        if (slashPopupEl && slashPopupEl.style.display !== 'none') {
            if (!slashPopupEl.contains(e.target) && e.target !== inputEl) {
                slashPopupEl.style.display = 'none';
            }
        }
        if (mentionPopupEl && mentionPopupEl.style.display !== 'none') {
            if (!mentionPopupEl.contains(e.target) && e.target !== inputEl) {
                mentionPopupEl.style.display = 'none';
            }
        }

        // ── External link interception ───────────────────────────────────
        // Intercept <a href="..."> clicks and route through bridge so the
        // host can open them in the system browser instead of navigating
        // the embedded Ultralight view.
        var anchor = e.target.closest ? e.target.closest('a[href]') : null;
        if (!anchor) {
            var el = e.target;
            while (el && el !== document) {
                if (el.tagName === 'A' && el.getAttribute('href')) { anchor = el; break; }
                el = el.parentElement;
            }
        }
        if (anchor) {
            var href = anchor.getAttribute('href');
            if (href && href !== '#' && !href.startsWith('javascript:')) {
                e.preventDefault();
                e.stopPropagation();
                if (window.exorcist) {
                    window.exorcist.sendToHost('openExternalUrl', { url: href });
                }
            }
        }
    });

    // ── Initial state ────────────────────────────────────────────────────────
    updateModeButtons();
    autoResizeInput();

    return api;
})();

