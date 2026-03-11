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

    // ── DOM refs ─────────────────────────────────────────────────────────────
    var welcomeEl       = document.getElementById('welcome');
    var transcriptEl    = document.getElementById('transcript');
    var suggestionsEl   = document.getElementById('welcome-suggestions');
    var toolCountEl     = document.getElementById('welcome-tool-count');

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

    // ── Model popup ──────────────────────────────────────────────────────────
    function showModelPopup() {
        if (!modelPopupEl || models.length === 0) return;
        var html = '';
        for (var i = 0; i < models.length; i++) {
            var m = models[i];
            var sel = m.id === currentModelId ? ' selected' : '';
            var badges = '';
            if (m.premium) badges += '<span class="model-badge premium">PRO</span>';
            if (m.thinking) badges += '<span class="model-badge thinking">Think</span>';
            if (m.vision) badges += '<span class="model-badge vision">Vision</span>';
            if (m.tools) badges += '<span class="model-badge tools">Tools</span>';
            html += '<div class="model-item' + sel + '" data-id="' + esc(m.id) + '">' +
                '<span class="model-name">' + esc(m.displayName || m.id) + '</span>' +
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
        if (modelPopupEl) modelPopupEl.style.display = 'none';
    }

    // ── Escape helper ────────────────────────────────────────────────────────
    function esc(s) {
        if (!s) return '';
        return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
                .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
    }
    // Maps ToolState enum → icon HTML
    function stepIconHtml(toolState, isStreamingTool) {
        switch (toolState) {
            case 0: // Queued
                return '<span class="step-icon queued">&#x25CB;</span>';
            case 1: // Streaming
                return '<span class="step-icon spinning">&#x25E6;</span>';
            case 2: // ConfirmationNeeded
                return '<span class="step-icon confirm">&#x26A0;</span>';
            case 3: // CompleteSuccess
                return '<span class="step-icon check">&#x2713;</span>';
            case 4: // CompleteError
                return '<span class="step-icon error">&#x2717;</span>';
            default:
                return '<span class="step-icon queued">&#x25CB;</span>';
        }
    }

    // ── Content Part Renderers ───────────────────────────────────────────────

    function renderMarkdownPart(part, streaming) {
        return '<div class="rendered-markdown" data-type="markdown">' +
            MarkdownRenderer.render(part.markdownText || '', streaming) +
        '</div>';
    }

    function renderThinkingPart(part) {
        var collapsed = part.thinkingCollapsed ? ' collapsed' : '';
        var streamingClass = part._streaming ? ' streaming' : '';
        var text = MarkdownRenderer.escapeHtml(part.thinkingText || '');

        // Nested tool items
        var toolsHtml = '';
        if (part._tools && part._tools.length > 0) {
            for (var i = 0; i < part._tools.length; i++) {
                toolsHtml += renderToolInvocationPart(part._tools[i]);
            }
        }

        return '<div class="chat-thinking-box' + collapsed + streamingClass + '" data-type="thinking">' +
            '<button class="thinking-header-btn" onclick="ChatApp._toggleThinking(this.parentElement)">' +
                '<span class="thinking-chevron">&#x25BC;</span>' +
                '<span class="thinking-label">Thinking</span>' +
            '</button>' +
            '<div class="thinking-content">' +
                '<div class="thinking-text">' + text.replace(/\n/g, '<br>') + '</div>' +
                toolsHtml +
            '</div>' +
        '</div>';
    }

    function renderToolInvocationPart(part) {
        var toolState = part.toolState || 0;
        var isStreamingState = (toolState === 1);
        var icon = stepIconHtml(toolState, isStreamingState);
        var title = part.toolPastTenseMsg || part.toolInvocationMsg ||
                    part.toolFriendlyTitle || part.toolName || 'Tool';
        var streamingClass = isStreamingState ? ' is-streaming' : '';
        var callId = MarkdownRenderer.escapeHtml(part.toolCallId || '');

        // Tool detail (input/output)
        var detailHtml = '';
        if (part.toolInput || part.toolOutput) {
            detailHtml = '<div class="tool-detail-container">' +
                '<button class="tool-detail-toggle" onclick="ChatApp._toggleToolDetail(this)">' +
                    '<span>&#x25B6;</span> Details' +
                '</button>' +
                '<div class="tool-detail-body hidden">';
            if (part.toolInput) {
                detailHtml += '<div class="tool-detail-section">' +
                    '<div class="detail-label">Input</div>' +
                    MarkdownRenderer.escapeHtml(part.toolInput) +
                '</div>';
            }
            if (part.toolOutput) {
                detailHtml += '<div class="tool-detail-section">' +
                    '<div class="detail-label">Output</div>' +
                    MarkdownRenderer.escapeHtml(part.toolOutput) +
                '</div>';
            }
            detailHtml += '</div></div>';
        }

        // Confirmation actions
        var confirmHtml = '';
        if (toolState === 2) {
            confirmHtml = '<div class="chat-confirmation-widget">' +
                '<div class="confirmation-title">Allow \u201C' +
                    MarkdownRenderer.escapeHtml(part.toolName || 'tool') +
                '\u201D?</div>' +
                '<div class="confirmation-actions">' +
                    '<button class="ws-action-btn primary" onclick="ChatApp._confirmTool(\'' +
                        callId + '\', 2)">Allow Always</button>' +
                    '<button class="ws-action-btn primary" onclick="ChatApp._confirmTool(\'' +
                        callId + '\', 1)">Allow</button>' +
                    '<button class="ws-action-btn" onclick="ChatApp._confirmTool(\'' +
                        callId + '\', 0)">Deny</button>' +
                '</div>' +
            '</div>';
        }

        return '<div class="chat-tool-invocation-part" data-call-id="' + callId + '">' +
            '<div class="progress-step' + streamingClass + '">' +
                icon +
                '<span class="step-label">' + MarkdownRenderer.escapeHtml(title) + '</span>' +
            '</div>' +
            detailHtml +
            confirmHtml +
        '</div>';
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
            if (f.action === 'created') icon = '&#x2795;'; // plus
            else if (f.action === 'deleted') icon = '&#x1F5D1;'; // trash

            // Stats
            var statsHtml = '';
            if (f.insertions > 0 || f.deletions > 0) {
                statsHtml = '<span class="file-stats">';
                if (f.insertions > 0) statsHtml += '<span class="stat-add">+' + f.insertions + '</span> ';
                if (f.deletions > 0) statsHtml += '<span class="stat-del">-' + f.deletions + '</span>';
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

            // Diff preview
            if (f.diff) {
                html += '<div class="diff-content">';
                var diffLines = f.diff.split('\n');
                for (var j = 0; j < diffLines.length; j++) {
                    var dl = diffLines[j];
                    var cls = '';
                    if (dl.charAt(0) === '+') cls = ' class="diff-line-add"';
                    else if (dl.charAt(0) === '-') cls = ' class="diff-line-del"';
                    html += '<div' + cls + '>' + MarkdownRenderer.escapeHtml(dl) + '</div>';
                }
                html += '</div>';
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
        var callId = MarkdownRenderer.escapeHtml(part.toolCallId || '');
        return '<div class="chat-confirmation-widget" data-type="confirmation" data-call-id="' + callId + '">' +
            '<div class="confirmation-title">' +
                MarkdownRenderer.escapeHtml(part.confirmTitle || 'Confirm') + '</div>' +
            '<div class="confirmation-msg">' +
                MarkdownRenderer.escapeHtml(part.confirmMessage || '') + '</div>' +
            '<div class="confirmation-actions">' +
                '<button class="ws-action-btn primary" onclick="ChatApp._confirmTool(\'' +
                    callId + '\', 1)">Allow</button>' +
                '<button class="ws-action-btn" onclick="ChatApp._confirmTool(\'' +
                    callId + '\', 0)">Deny</button>' +
            '</div>' +
        '</div>';
    }

    function renderWarningPart(part) {
        return '<div class="warning-box" data-type="warning">' +
            MarkdownRenderer.escapeHtml(part.warningText || part.markdownText || '') +
        '</div>';
    }

    function renderErrorPart(part) {
        return '<div class="error-box" data-type="error">' +
            MarkdownRenderer.escapeHtml(part.errorText || part.markdownText || '') +
        '</div>';
    }

    function renderProgressPart(part) {
        return '<div class="progress-box" data-type="progress">' +
            '<div class="progress-spinner"></div>' +
            '<span>' + MarkdownRenderer.escapeHtml(part.progressLabel || 'Working...') + '</span>' +
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
        10: renderCommandButtonPart
    };

    function renderContentPart(part, streaming) {
        var renderer = partRenderers[part.type];
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
                        '<div class="avatar avatar-user">U</div>' +
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
                        '<div class="avatar avatar-ai">&#x2728;</div>' +
                    '</div>' +
                    '<span class="username">Copilot</span>' +
                '</div>' +
            '</div>';

            html += '<div class="value" id="parts-' + turnIndex + '">';
            var turnIsStreaming = (turn.state === 0);
            for (var i = 0; i < parts.length; i++) {
                var isLast = (i === parts.length - 1);
                html += renderContentPart(parts[i], turnIsStreaming && isLast);
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
                            '<div class="avatar avatar-ai">&#x2728;</div>' +
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
                            '<div class="avatar avatar-ai">&#x2728;</div>' +
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

        partsEl.insertAdjacentHTML('beforeend', renderContentPart(partJson, false));
        scheduleScroll();
    };

    api.updateToolState = function(turnIndex, callId, stateJson) {
        var partsEl = document.getElementById('parts-' + turnIndex);
        if (!partsEl) return;

        var toolPart = partsEl.querySelector('[data-call-id="' + callId + '"]');
        if (!toolPart) return;

        // Update icon
        var stepEl = toolPart.querySelector('.progress-step');
        if (stepEl) {
            var iconEl = stepEl.querySelector('.step-icon');
            if (iconEl && stateJson.toolState !== undefined) {
                iconEl.outerHTML = stepIconHtml(stateJson.toolState);
            }

            // Update streaming class
            if (stateJson.toolState === 1) {
                stepEl.classList.add('is-streaming');
            } else {
                stepEl.classList.remove('is-streaming');
            }
        }

        // Update label
        if (stateJson.toolPastTenseMsg) {
            var labelEl = toolPart.querySelector('.step-label');
            if (labelEl) labelEl.textContent = stateJson.toolPastTenseMsg;
        }

        // Update or create tool detail
        if (stateJson.toolOutput || stateJson.toolInput) {
            var detailContainer = toolPart.querySelector('.tool-detail-container');
            if (!detailContainer) {
                var detailHtml = '<div class="tool-detail-container">' +
                    '<button class="tool-detail-toggle" onclick="ChatApp._toggleToolDetail(this)">' +
                        '<span>&#x25B6;</span> Details' +
                    '</button>' +
                    '<div class="tool-detail-body hidden">';
                if (stateJson.toolInput) {
                    detailHtml += '<div class="tool-detail-section">' +
                        '<div class="detail-label">Input</div>' +
                        MarkdownRenderer.escapeHtml(stateJson.toolInput) +
                    '</div>';
                }
                if (stateJson.toolOutput) {
                    detailHtml += '<div class="tool-detail-section">' +
                        '<div class="detail-label">Output</div>' +
                        MarkdownRenderer.escapeHtml(stateJson.toolOutput) +
                    '</div>';
                }
                detailHtml += '</div></div>';
                toolPart.insertAdjacentHTML('beforeend', detailHtml);
            } else {
                var bodyEl = detailContainer.querySelector('.tool-detail-body');
                if (bodyEl) {
                    var newDetailHtml = '';
                    if (stateJson.toolInput) {
                        newDetailHtml += '<div class="tool-detail-section">' +
                            '<div class="detail-label">Input</div>' +
                            MarkdownRenderer.escapeHtml(stateJson.toolInput) +
                        '</div>';
                    }
                    if (stateJson.toolOutput) {
                        newDetailHtml += '<div class="tool-detail-section">' +
                            '<div class="detail-label">Output</div>' +
                            MarkdownRenderer.escapeHtml(stateJson.toolOutput) +
                        '</div>';
                    }
                    bodyEl.innerHTML = newDetailHtml;
                }
            }
        }

        // Remove confirmation widget if tool is now complete
        if (stateJson.toolState === 3 || stateJson.toolState === 4) {
            var confirmWidget = toolPart.querySelector('.chat-confirmation-widget');
            if (confirmWidget) confirmWidget.remove();
        }
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
        }

        // Remove streaming shimmer from tool labels
        var streamingSteps = turnEl.querySelectorAll('.progress-step.is-streaming');
        for (var k = 0; k < streamingSteps.length; k++) {
            streamingSteps[k].classList.remove('is-streaming');
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

    api.showWelcome = function(state) {
        welcomeEl.style.display = 'flex';
        transcriptEl.style.display = 'none';
        welcomeEl.className = 'welcome' + (state && state !== 'default' ? ' ' + state : '');
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
            modelLabelEl.textContent = found ? (found.displayName || found.id) : (id || 'Model');
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
                // Check if slash popup is visible and has a selected item
                if (slashPopupEl && slashPopupEl.style.display !== 'none') {
                    var sel = slashPopupEl.querySelector('.autocomplete-item.selected');
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
    });

    // ── Initial state ────────────────────────────────────────────────────────
    updateModeButtons();
    autoResizeInput();

    return api;
})();
