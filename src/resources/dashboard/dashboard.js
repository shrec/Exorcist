// ── Agent Dashboard — JavaScript ──────────────────────────────────────────────
//
// Handles structured events from C++ via window.exorcist.sendToHost bridge.
// C++ calls dashboardBridge.handleEvent(jsonString) to push events.

(function() {
    'use strict';

    // ── State ─────────────────────────────────────────────────────────────────
    const state = {
        missionId: null,
        missionTitle: '',
        missionStatus: 'idle',
        steps: {},          // stepId → { label, status, detail, order }
        metrics: {},        // key → { value, unit }
        logs: [],           // [{ time, level, message }]
        artifacts: [],      // [{ type, path, meta }]
        logCount: 0,
    };

    // ── DOM refs ──────────────────────────────────────────────────────────────
    const $ = id => document.getElementById(id);
    const dom = {
        dashboard:    document.querySelector('.dashboard'),
        missionTitle: $('missionTitle'),
        missionStatus:$('missionStatus'),
        metricsBar:   $('metricsBar'),
        stepsList:    $('stepsList'),
        logsList:     $('logsList'),
        logCount:     $('logCount'),
        artifactsList:$('artifactsList'),
        artifactsBar: $('artifactsBar'),
        emptyState:   $('emptyState'),
    };

    // ── Event handler dispatch ────────────────────────────────────────────────
    const handlers = {
        MissionCreated:   onMissionCreated,
        MissionUpdated:   onMissionUpdated,
        MissionCompleted: onMissionCompleted,
        StepAdded:        onStepAdded,
        StepUpdated:      onStepUpdated,
        MetricUpdated:    onMetricUpdated,
        LogAdded:         onLogAdded,
        ArtifactAdded:    onArtifactAdded,
        PanelCreated:     onPanelCreated,
        PanelUpdated:     onPanelUpdated,
        PanelRemoved:     onPanelRemoved,
        CustomEvent:      onCustomEvent,
    };

    // ── Main entry point (called from C++) ────────────────────────────────────
    window.dashboardBridge = {
        handleEvent: function(jsonStr) {
            try {
                const event = JSON.parse(jsonStr);
                const handler = handlers[event.type];
                if (handler)
                    handler(event.payload, event.missionId, event.timestamp);
            } catch(e) {
                console.error('Dashboard event error:', e);
            }
        },

        clear: function() {
            clearDashboard();
        },

        setTheme: function(tokens) {
            try {
                const t = typeof tokens === 'string' ? JSON.parse(tokens) : tokens;
                const root = document.documentElement;
                for (const [key, value] of Object.entries(t))
                    root.style.setProperty('--' + key, value);
            } catch(e) {}
        },
    };

    // ── Mission handlers ──────────────────────────────────────────────────────

    function onMissionCreated(p, missionId) {
        clearDashboard();
        state.missionId = missionId;
        state.missionTitle = p.title || 'Mission';
        state.missionStatus = 'created';

        dom.missionTitle.textContent = state.missionTitle;
        setStatus('created');
        dom.dashboard.classList.add('has-mission');
    }

    function onMissionUpdated(p) {
        state.missionStatus = p.status || 'running';
        setStatus(state.missionStatus);
    }

    function onMissionCompleted(p) {
        state.missionStatus = p.success ? 'completed' : 'failed';
        setStatus(state.missionStatus);
        if (p.summary)
            addLog('info', p.summary);
    }

    // ── Step handlers ─────────────────────────────────────────────────────────

    function onStepAdded(p) {
        const stepId = p.stepId;
        state.steps[stepId] = {
            label: p.label || stepId,
            status: p.status || 'pending',
            detail: '',
            order: p.order >= 0 ? p.order : Object.keys(state.steps).length,
        };
        renderSteps();
    }

    function onStepUpdated(p) {
        const step = state.steps[p.stepId];
        if (!step) return;
        step.status = p.status || step.status;
        if (p.detail) step.detail = p.detail;
        renderSteps();
    }

    // ── Metric handler ────────────────────────────────────────────────────────

    function onMetricUpdated(p) {
        state.metrics[p.key] = { value: p.value, unit: p.unit || '' };
        renderMetrics();
    }

    // ── Log handler ───────────────────────────────────────────────────────────

    function onLogAdded(p, missionId, timestamp) {
        const ts = timestamp ? new Date(timestamp) : new Date();
        addLog(p.level || 'info', p.message || '', ts);
    }

    // ── Artifact handler ──────────────────────────────────────────────────────

    function onArtifactAdded(p) {
        state.artifacts.push({
            type: p.type || 'file',
            path: p.path || '',
            meta: p.meta || {},
        });
        renderArtifacts();
    }

    // ── Panel handlers (minimal — future expansion) ───────────────────────────

    function onPanelCreated(p) {
        addLog('info', 'Panel created: ' + (p.title || p.panelId));
    }

    function onPanelUpdated(p) {
        // Future: update sub-panel content
    }

    function onPanelRemoved(p) {
        addLog('info', 'Panel removed: ' + p.panelId);
    }

    function onCustomEvent(p) {
        addLog('debug', 'Custom: ' + (p.eventName || 'unknown'));
    }

    // ── Render functions ──────────────────────────────────────────────────────

    function setStatus(status) {
        dom.missionStatus.textContent = status;
        dom.missionStatus.className = 'mission-status badge ' + status;
    }

    function renderSteps() {
        const sorted = Object.entries(state.steps)
            .sort((a, b) => a[1].order - b[1].order);

        dom.stepsList.innerHTML = sorted.map(([id, s]) => {
            const icons = { pending: '&#9675;', running: '&#9679;', completed: '&#10003;', failed: '&#10007;', skipped: '&#8722;' };
            return '<div class="step-item" data-step="' + esc(id) + '">' +
                '<div class="step-indicator ' + esc(s.status) + '">' + (icons[s.status] || '&#9675;') + '</div>' +
                '<div class="step-content">' +
                    '<div class="step-label">' + esc(s.label) + '</div>' +
                    (s.detail ? '<div class="step-detail">' + esc(s.detail) + '</div>' : '') +
                '</div>' +
            '</div>';
        }).join('');
    }

    function renderMetrics() {
        dom.metricsBar.innerHTML = Object.entries(state.metrics).map(([key, m]) => {
            return '<div class="metric-card">' +
                '<span class="metric-label">' + esc(key) + '</span>' +
                '<span class="metric-value">' + esc(String(m.value)) +
                    (m.unit ? '<span class="metric-unit">' + esc(m.unit) + '</span>' : '') +
                '</span>' +
            '</div>';
        }).join('');
    }

    function addLog(level, message, time) {
        const t = time || new Date();
        state.logs.push({ time: t, level, message });
        state.logCount++;

        const timeStr = t.toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' });

        const el = document.createElement('div');
        el.className = 'log-entry ' + level;
        el.innerHTML =
            '<span class="log-time">' + timeStr + '</span>' +
            '<span class="log-level ' + level + '">' + level + '</span>' +
            '<span class="log-message">' + esc(message) + '</span>';

        dom.logsList.appendChild(el);
        dom.logCount.textContent = state.logCount;

        // Auto-scroll
        dom.logsList.scrollTop = dom.logsList.scrollHeight;
    }

    function renderArtifacts() {
        if (state.artifacts.length === 0) {
            dom.artifactsBar.classList.add('hidden');
            return;
        }
        dom.artifactsBar.classList.remove('hidden');

        dom.artifactsList.innerHTML = state.artifacts.map((a, i) => {
            const name = a.path.split(/[/\\]/).pop() || a.path;
            return '<div class="artifact-chip" data-index="' + i + '" title="' + esc(a.path) + '">' +
                '<span class="artifact-type">' + esc(a.type) + '</span>' +
                '<span>' + esc(name) + '</span>' +
            '</div>';
        }).join('');

        // Click to open file
        dom.artifactsList.querySelectorAll('.artifact-chip').forEach(chip => {
            chip.addEventListener('click', () => {
                const idx = parseInt(chip.dataset.index, 10);
                const art = state.artifacts[idx];
                if (art && window.exorcist)
                    window.exorcist.sendToHost('openArtifact', { path: art.path, type: art.type });
            });
        });
    }

    function clearDashboard() {
        state.missionId = null;
        state.missionTitle = '';
        state.missionStatus = 'idle';
        state.steps = {};
        state.metrics = {};
        state.logs = [];
        state.artifacts = [];
        state.logCount = 0;

        dom.missionTitle.textContent = 'Agent Dashboard';
        setStatus('idle');
        dom.metricsBar.innerHTML = '';
        dom.stepsList.innerHTML = '';
        dom.logsList.innerHTML = '';
        dom.logCount.textContent = '0';
        dom.artifactsList.innerHTML = '';
        dom.artifactsBar.classList.add('hidden');
        dom.dashboard.classList.remove('has-mission');
    }

    // ── Util ──────────────────────────────────────────────────────────────────
    function esc(s) {
        const d = document.createElement('div');
        d.textContent = s;
        return d.innerHTML;
    }

})();
