// ── Exorcist Chat Rendering Engine ───────────────────────────────────────────
//
// Pure JS (no framework) chat transcript renderer for Ultralight.
// Called from C++ via ChatJSBridge → evaluateScript("ChatApp.method(...)").
// Sends user actions to C++ via window.exorcist.sendToHost(type, payload).

var ChatApp = (function() {
    'use strict';

    // ── State ────────────────────────────────────────────────────────────────
    var turns = [];          // Array of turn data mirrors
    var isStreaming = false;
    var userScrolledAway = false;
    var scrollRAF = null;

    // ── DOM refs ─────────────────────────────────────────────────────────────
    var welcomeEl = document.getElementById('welcome');
    var transcriptEl = document.getElementById('transcript');
    var suggestionsEl = document.getElementById('welcome-suggestions');
    var toolCountEl = document.getElementById('welcome-tool-count');

    // ── Auto-scroll ──────────────────────────────────────────────────────────
    function isNearBottom() {
        return transcriptEl.scrollHeight - transcriptEl.scrollTop - transcriptEl.clientHeight < 30;
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

    // ── Tool State Icons ─────────────────────────────────────────────────────
    var toolStateIcons = {
        0: '<span class="tool-state-icon queued">\u25CB</span>',        // Queued (circle)
        1: '<span class="tool-state-icon streaming">\u25CF</span>',     // Streaming (filled)
        2: '<span class="tool-state-icon queued">\u26A0</span>',        // ConfirmationNeeded
        3: '<span class="tool-state-icon success">\u2713</span>',       // CompleteSuccess
        4: '<span class="tool-state-icon error">\u2717</span>'          // CompleteError
    };

    // ── Content Part Renderers ───────────────────────────────────────────────

    function renderMarkdownPart(part, streaming) {
        return '<div class="markdown-content" data-type="markdown">' +
            MarkdownRenderer.render(part.markdownText || '', streaming) +
        '</div>';
    }

    function renderThinkingPart(part) {
        var collapsed = part.thinkingCollapsed ? ' collapsed' : '';
        var streamingClass = part._streaming ? ' streaming' : '';
        var text = MarkdownRenderer.escapeHtml(part.thinkingText || '');

        var toolsHtml = '';
        if (part._tools && part._tools.length > 0) {
            toolsHtml = '<div class="thinking-tools">';
            for (var i = 0; i < part._tools.length; i++) {
                toolsHtml += renderToolItem(part._tools[i]);
            }
            toolsHtml += '</div>';
        }

        return '<div class="thinking-box' + collapsed + streamingClass + '" data-type="thinking">' +
            '<div class="thinking-header" onclick="ChatApp._toggleThinking(this)">' +
                '<span class="thinking-chevron">\u25BC</span>' +
                '<span class="thinking-label">Thinking</span>' +
            '</div>' +
            '<div class="thinking-body">' +
                text.replace(/\n/g, '<br>') +
            '</div>' +
            toolsHtml +
        '</div>';
    }

    function renderToolItem(part) {
        var stateIcon = toolStateIcons[part.toolState || 0] || toolStateIcons[0];
        var title = part.toolPastTenseMsg || part.toolInvocationMsg || part.toolFriendlyTitle || part.toolName || 'Tool';

        var detailHtml = '';
        if (part.toolInput || part.toolOutput) {
            var hidden = part.toolCollapsedIO ? ' hidden' : '';
            detailHtml = '<div class="tool-detail' + hidden + '">';
            if (part.toolInput) detailHtml += '<div><strong>Input:</strong> ' + MarkdownRenderer.escapeHtml(part.toolInput) + '</div>';
            if (part.toolOutput) detailHtml += '<div><strong>Output:</strong> ' + MarkdownRenderer.escapeHtml(part.toolOutput) + '</div>';
            detailHtml += '</div>';
        }

        return '<div class="chain-line">' +
            '<div class="tool-item" data-call-id="' + MarkdownRenderer.escapeHtml(part.toolCallId || '') + '">' +
                stateIcon +
                '<span class="tool-title">' + MarkdownRenderer.escapeHtml(title) + '</span>' +
            '</div>' +
            detailHtml +
        '</div>';
    }

    function renderToolInvocationPart(part) {
        return renderToolItem(part);
    }

    function renderWorkspaceEditPart(part) {
        var files = part.files || [];
        var html = '<div class="workspace-edit" data-type="workspaceEdit">';

        for (var i = 0; i < files.length; i++) {
            var f = files[i];
            html += '<div class="workspace-edit-header">' +
                '<span class="workspace-edit-file">' +
                    '<span>\uD83D\uDCC4</span>' +
                    '<span>' + MarkdownRenderer.escapeHtml(f.path || f.name || 'file') + '</span>' +
                '</span>' +
                '<div class="workspace-edit-actions">' +
                    '<button class="ws-action-btn primary" onclick="ChatApp._keepFile(' + i + ', \'' + MarkdownRenderer.escapeHtml(f.path || '') + '\')">Keep</button>' +
                    '<button class="ws-action-btn" onclick="ChatApp._undoFile(' + i + ', \'' + MarkdownRenderer.escapeHtml(f.path || '') + '\')">Undo</button>' +
                '</div>' +
            '</div>';

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

        var html = '<div class="followups" data-type="followups">';
        for (var i = 0; i < items.length; i++) {
            html += '<button class="followup-pill" onclick="ChatApp._followup(\'' +
                MarkdownRenderer.escapeHtml(items[i]) + '\')">' +
                MarkdownRenderer.escapeHtml(items[i]) + '</button>';
        }
        html += '</div>';
        return html;
    }

    function renderConfirmationPart(part) {
        return '<div class="confirmation-box" data-type="confirmation" data-call-id="' +
            MarkdownRenderer.escapeHtml(part.toolCallId || '') + '">' +
            '<div class="confirmation-title">' + MarkdownRenderer.escapeHtml(part.confirmTitle || 'Confirm') + '</div>' +
            '<div class="confirmation-msg">' + MarkdownRenderer.escapeHtml(part.confirmMessage || '') + '</div>' +
            '<div class="confirmation-actions">' +
                '<button class="ws-action-btn primary" onclick="ChatApp._confirmTool(\'' +
                    MarkdownRenderer.escapeHtml(part.toolCallId || '') + '\', 1)">Allow</button>' +
                '<button class="ws-action-btn" onclick="ChatApp._confirmTool(\'' +
                    MarkdownRenderer.escapeHtml(part.toolCallId || '') + '\', 0)">Deny</button>' +
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
        0: renderMarkdownPart,        // Markdown
        1: renderThinkingPart,        // Thinking
        2: renderToolInvocationPart,  // ToolInvocation
        3: renderWorkspaceEditPart,   // WorkspaceEdit
        4: renderFollowupsPart,       // Followup
        5: renderConfirmationPart,    // Confirmation
        6: renderWarningPart,         // Warning
        7: renderErrorPart,           // Error
        8: renderProgressPart,        // Progress
        9: renderFileTreePart,        // FileTree
        10: renderCommandButtonPart   // CommandButton
    };

    function renderContentPart(part, streaming) {
        var renderer = partRenderers[part.type];
        if (renderer) return renderer(part, streaming);
        return '<div class="warning-box">Unknown part type: ' + part.type + '</div>';
    }

    // ── Turn Rendering ───────────────────────────────────────────────────────

    function createTurnHtml(turnIndex, turn) {
        var isUser = turn.userMessage !== undefined && turn.userMessage !== null;
        var avatarClass = isUser ? 'avatar-user' : 'avatar-ai';
        var avatarText = isUser ? 'U' : 'AI';
        var roleText = isUser ? 'You' : 'Copilot';
        var turnId = turn.id || ('turn-' + turnIndex);

        var html = '<div class="turn" id="turn-' + turnIndex + '" data-turn-id="' +
            MarkdownRenderer.escapeHtml(turnId) + '" role="article">';

        // User message
        if (isUser && turn.userMessage) {
            html += '<div class="turn-header">' +
                '<div class="avatar ' + avatarClass + '">' + avatarText + '</div>' +
                '<span class="turn-role">' + roleText + '</span>' +
            '</div>';
            html += '<div class="turn-parts">';
            html += '<div class="markdown-content">' +
                MarkdownRenderer.render(turn.userMessage, false) + '</div>';
            html += '</div>';
        }

        // Response parts
        var parts = turn.parts || [];
        if (parts.length > 0) {
            if (isUser) {
                // Add AI header for response
                html += '<div class="turn-header" style="margin-top:12px">' +
                    '<div class="avatar avatar-ai">AI</div>' +
                    '<span class="turn-role">Copilot</span>' +
                '</div>';
            } else if (!isUser) {
                html += '<div class="turn-header">' +
                    '<div class="avatar avatar-ai">AI</div>' +
                    '<span class="turn-role">Copilot</span>' +
                '</div>';
            }

            html += '<div class="turn-parts" id="parts-' + turnIndex + '">';
            var turnIsStreaming = (turn.state === 0); // State::Streaming
            for (var i = 0; i < parts.length; i++) {
                var isLast = (i === parts.length - 1);
                html += renderContentPart(parts[i], turnIsStreaming && isLast);
            }
            html += '</div>';
        }

        // Feedback buttons (only for completed AI turns)
        if (!isUser && turn.state !== 0) {
            html += '<div class="turn-footer">' +
                '<button class="feedback-btn' + (turn.feedback === 1 ? ' active' : '') +
                    '" onclick="ChatApp._feedback(\'' + turnId + '\', true)" title="Helpful">\u{1F44D}</button>' +
                '<button class="feedback-btn' + (turn.feedback === 2 ? ' active' : '') +
                    '" onclick="ChatApp._feedback(\'' + turnId + '\', false)" title="Not helpful">\u{1F44E}</button>' +
            '</div>';
        }

        html += '</div>';
        return html;
    }

    // ── Public API (called from C++ via ChatJSBridge) ────────────────────────

    var api = {};

    /// Add a turn (new or restored). turnJson is ChatTurnModel::toJson().
    api.addTurn = function(turnIndex, turnJson) {
        turns[turnIndex] = turnJson;

        // Show transcript, hide welcome
        welcomeEl.style.display = 'none';
        transcriptEl.style.display = 'block';

        var html = createTurnHtml(turnIndex, turnJson);
        var div = document.createElement('div');
        div.innerHTML = html;
        transcriptEl.appendChild(div.firstChild);

        scheduleScroll();
    };

    /// Append markdown delta during streaming.
    api.appendMarkdownDelta = function(turnIndex, delta) {
        var turn = turns[turnIndex];
        if (!turn) return;

        // Find or create the last markdown part
        var parts = turn.parts = turn.parts || [];
        var lastPart = parts.length > 0 ? parts[parts.length - 1] : null;

        if (!lastPart || lastPart.type !== 0) {
            // Create new markdown part
            lastPart = { type: 0, markdownText: '' };
            parts.push(lastPart);
        }

        lastPart.markdownText = (lastPart.markdownText || '') + delta;

        // Update DOM — find the last markdown-content div in this turn's parts
        var partsEl = document.getElementById('parts-' + turnIndex);
        if (!partsEl) {
            // No parts container yet — create one
            var turnEl = document.getElementById('turn-' + turnIndex);
            if (!turnEl) return;

            // Add AI header if not present
            if (!turnEl.querySelector('.turn-parts')) {
                var headerHtml = '<div class="turn-header" style="margin-top:12px">' +
                    '<div class="avatar avatar-ai">AI</div>' +
                    '<span class="turn-role">Copilot</span>' +
                '</div>' +
                '<div class="turn-parts" id="parts-' + turnIndex + '"></div>';
                turnEl.insertAdjacentHTML('beforeend', headerHtml);
                partsEl = document.getElementById('parts-' + turnIndex);
            }
        }

        if (!partsEl) return;

        // Find last markdown div
        var mdDivs = partsEl.querySelectorAll('[data-type="markdown"]');
        if (mdDivs.length === 0 || (lastPart === parts[parts.length - 1] && mdDivs.length < parts.filter(function(p) { return p.type === 0; }).length)) {
            // Need new markdown div
            partsEl.insertAdjacentHTML('beforeend', '<div class="markdown-content" data-type="markdown"></div>');
            mdDivs = partsEl.querySelectorAll('[data-type="markdown"]');
        }

        var mdDiv = mdDivs[mdDivs.length - 1];
        mdDiv.innerHTML = MarkdownRenderer.render(lastPart.markdownText, true);

        scheduleScroll();
    };

    /// Append thinking delta during streaming.
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

        // Update DOM
        var partsEl = document.getElementById('parts-' + turnIndex);
        if (!partsEl) return;

        var thinkingDivs = partsEl.querySelectorAll('[data-type="thinking"]');
        if (thinkingDivs.length === 0) {
            partsEl.insertAdjacentHTML('beforeend', renderThinkingPart(lastPart));
        } else {
            var thinkingDiv = thinkingDivs[thinkingDivs.length - 1];
            var bodyEl = thinkingDiv.querySelector('.thinking-body');
            if (bodyEl) {
                bodyEl.innerHTML = MarkdownRenderer.escapeHtml(lastPart.thinkingText).replace(/\n/g, '<br>');
            }
            // Ensure streaming class
            thinkingDiv.classList.add('streaming');
            thinkingDiv.classList.remove('collapsed');
        }

        scheduleScroll();
    };

    /// Add a new content part to a turn.
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
                // Update DOM
                var partsEl = document.getElementById('parts-' + turnIndex);
                if (partsEl) {
                    var thinkingDivs = partsEl.querySelectorAll('[data-type="thinking"]');
                    var last = thinkingDivs[thinkingDivs.length - 1];
                    if (last) {
                        last.classList.add('collapsed');
                        last.classList.remove('streaming');
                    }
                }
            }
        }

        parts.push(partJson);

        // Render and append to DOM
        var partsEl = document.getElementById('parts-' + turnIndex);
        if (!partsEl) return;

        partsEl.insertAdjacentHTML('beforeend', renderContentPart(partJson, false));
        scheduleScroll();
    };

    /// Update tool invocation state.
    api.updateToolState = function(turnIndex, callId, stateJson) {
        var partsEl = document.getElementById('parts-' + turnIndex);
        if (!partsEl) return;

        var toolItem = partsEl.querySelector('[data-call-id="' + callId + '"]');
        if (!toolItem) return;

        // Update icon
        var iconEl = toolItem.querySelector('.tool-state-icon');
        if (iconEl && stateJson.toolState !== undefined) {
            iconEl.outerHTML = toolStateIcons[stateJson.toolState] || toolStateIcons[0];
        }

        // Update title if past-tense message provided
        if (stateJson.toolPastTenseMsg) {
            var titleEl = toolItem.querySelector('.tool-title');
            if (titleEl) titleEl.textContent = stateJson.toolPastTenseMsg;
        }

        // Update detail
        if (stateJson.toolOutput || stateJson.toolInput) {
            var detailEl = toolItem.parentElement.querySelector('.tool-detail');
            if (!detailEl) {
                toolItem.parentElement.insertAdjacentHTML('beforeend',
                    '<div class="tool-detail"></div>');
                detailEl = toolItem.parentElement.querySelector('.tool-detail');
            }
            var detailHtml = '';
            if (stateJson.toolInput) detailHtml += '<div><strong>Input:</strong> ' +
                MarkdownRenderer.escapeHtml(stateJson.toolInput) + '</div>';
            if (stateJson.toolOutput) detailHtml += '<div><strong>Output:</strong> ' +
                MarkdownRenderer.escapeHtml(stateJson.toolOutput) + '</div>';
            detailEl.innerHTML = detailHtml;
            detailEl.classList.remove('hidden');
        }
    };

    /// Mark turn as finished.
    api.finishTurn = function(turnIndex, state) {
        var turn = turns[turnIndex];
        if (!turn) return;
        turn.state = state;

        // Remove streaming cursor
        var turnEl = document.getElementById('turn-' + turnIndex);
        if (!turnEl) return;

        var cursors = turnEl.querySelectorAll('.streaming-cursor');
        for (var i = 0; i < cursors.length; i++) {
            cursors[i].remove();
        }

        // Remove streaming class from thinking
        var thinkingBoxes = turnEl.querySelectorAll('.thinking-box.streaming');
        for (var j = 0; j < thinkingBoxes.length; j++) {
            thinkingBoxes[j].classList.remove('streaming');
        }

        // Add feedback buttons
        if (turn.id) {
            var existingFooter = turnEl.querySelector('.turn-footer');
            if (!existingFooter) {
                turnEl.insertAdjacentHTML('beforeend',
                    '<div class="turn-footer">' +
                        '<button class="feedback-btn" onclick="ChatApp._feedback(\'' +
                            turn.id + '\', true)" title="Helpful">\u{1F44D}</button>' +
                        '<button class="feedback-btn" onclick="ChatApp._feedback(\'' +
                            turn.id + '\', false)" title="Not helpful">\u{1F44E}</button>' +
                    '</div>');
            }
        }

        scheduleScroll();
    };

    /// Clear all turns.
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
            'fgCode': '--fg-code',
            'linkBlue': '--link-blue',
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
            'sepLine': '--sep-line',
            'hoverBg': '--hover-bg',
            'codeBg': '--code-bg',
            'codeHeaderBg': '--code-header-bg',
            'inlineCodeBg': '--inline-code-bg',
            'errorFg': '--error-fg',
            'warningFg': '--warning-fg',
            'successFg': '--success-fg',
            'thinkingBg': '--thinking-bg',
            'thinkingBorder': '--thinking-border',
            'thinkingFg': '--thinking-fg',
            'toolBg': '--tool-bg',
            'toolBorderDefault': '--tool-border-default',
            'toolBorderActive': '--tool-border-active',
            'toolBorderOk': '--tool-border-ok',
            'toolBorderFail': '--tool-border-fail',
            'diffInsertBg': '--diff-insert-bg',
            'diffDeleteBg': '--diff-delete-bg',
            'diffInsertFg': '--diff-insert-fg',
            'diffDeleteFg': '--diff-delete-fg',
            'avatarUserBg': '--avatar-user-bg',
            'avatarAiBg': '--avatar-ai-bg',
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
        if (!toolCountEl) return;
        toolCountEl.textContent = count > 0 ? (count + ' tools available') : '';
    };

    // ── Internal Action Handlers (onclick → C++ bridge) ──────────────────────

    api._toggleThinking = function(headerEl) {
        var box = headerEl.parentElement;
        box.classList.toggle('collapsed');
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
        var lang = '';
        var langEl = block.querySelector('.code-block-lang');
        if (langEl) lang = langEl.textContent.toLowerCase();
        if (code && window.exorcist) {
            window.exorcist.sendToHost('insertCode', { code: code.textContent, language: lang });
        }
    };

    api._followup = function(message) {
        if (window.exorcist) window.exorcist.sendToHost('followupClicked', { message: message });
    };

    api._feedback = function(turnId, helpful) {
        if (window.exorcist) window.exorcist.sendToHost('feedbackGiven', { turnId: turnId, helpful: helpful });
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

    return api;
})();
