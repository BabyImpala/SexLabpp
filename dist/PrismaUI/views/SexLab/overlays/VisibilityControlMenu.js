'use strict';

// ── CONSTANTS

const VCM_SCALE_MIN        = 0.5;
const VCM_SCALE_MAX        = 2.5;
const VCM_SCALE_STEP       = 0.01;
const VCM_SCALE_STEP_SHIFT = 0.1;

const VCM_ICON_ON  = 'overlays/icons/slp_toggle_on.png';
const VCM_ICON_OFF = 'overlays/icons/slp_toggle_off.png';

const VCM_CSS = Object.freeze({
    OPEN: 'open',
});

const VCM_IDS = Object.freeze({
    OVERLAY:           'overlay_visibilityControl',
    PANEL:             'vcmPanel',
    BACKDROP:          'vcmBackdrop',
    PULL_TAB:          'vcmPullTab',
    OVERLAYS_HEADER:   'vcm-overlays-header',
    SCALE_SLIDER:      'vcmScaleSlider',
    SCALE_VAL:         'vcmScaleVal',
});

// Index maps for overlays must explicitly match PrismaUI::PrismaOverlayIndex
const VCM_OVERLAY_INDICES = Object.freeze(['-1', '1', '2', '3', '4', '5']);

// ── STATE

let vcm_scaleRafPending = 0; // Pending RAF id for scale wheel throttle
const vcm_sectionOpen   = { overlays: true };
let vcm_panelOpen       = false;

const vcm_overlayState = new Map([
    ['-1', true], // Game HUD
    ['1', true], // AnimSpeedOverlay
    ['2', true], // EnjoymentBars
    ['3', true], // OffsetAdjustMenu
    ['4', true], // SceneSelectorMenu
    ['5', true], // ThreadConfigMenu
]);

// ── CACHED ELEMENTS

let vcm_el = {};
const vcm_iconEls = new Map();
const vcm_sectionRefs = {};

function vcm_cacheElements() {
    vcm_el = {
        overlay:         document.getElementById(VCM_IDS.OVERLAY),
        panel:           document.getElementById(VCM_IDS.PANEL),
        backdrop:        document.getElementById(VCM_IDS.BACKDROP),
        pullTab:         document.getElementById(VCM_IDS.PULL_TAB),
        overlaysHeader:  document.getElementById(VCM_IDS.OVERLAYS_HEADER),
        scaleSlider:     document.getElementById(VCM_IDS.SCALE_SLIDER),
        scaleVal:        document.getElementById(VCM_IDS.SCALE_VAL),
    };

    for (const index of VCM_OVERLAY_INDICES) {
        const el = document.getElementById('vcm-icon-' + index);
        if (el) vcm_iconEls.set(index, el);
    }

    vcm_sectionRefs['overlays'] = {
        body : document.getElementById('vcm-overlays-body'),
        arrow: document.getElementById('vcm-overlays-arrow'),
    };
}

// ── C++ INIT AND DESTROY

window.vcm_initOverlay = function(jsonStr) {
    if (vcm_el.overlay) vcm_el.overlay.style.display = '';
    let data;
    try { data = JSON.parse(jsonStr) || {}; } catch(e) { return; }

    const liveScaleMult = parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--s-adj')) || 1.5;
    vcm_applyScale((data.scaleAdj !== undefined) ? data.scaleAdj : liveScaleMult, false);

    const states = data.states || {};
    for (const index of VCM_OVERLAY_INDICES) {
        if (states[index] !== undefined) vcm_overlayState.set(index, !!states[index]);
    }
    vcm_syncAllToggleIcons();
};

window.vcm_destroyOverlay = function() {
    vcm_closePanel();
    vcm_sectionOpen.overlays = true;
    for (const index of VCM_OVERLAY_INDICES) vcm_overlayState.set(index, true);
    vcm_syncAllToggleIcons();
    vcm_panelOpen = false;
    if (vcm_el.overlay) vcm_el.overlay.style.display = 'none';
};

// ── C++ TO JS

window.vcm_setOverlayState = function(payload) {
    const sep = payload.indexOf('^');
    if (sep === -1) return;
    const index = payload.substring(0, sep);
    const state = payload.substring(sep + 1) === 'true';
    if (vcm_overlayState.has(index)) {
        vcm_overlayState.set(index, state);
        vcm_syncToggle(index, state);
    }
};

// ── JS TO C++

function vcm_fireScaleChange(val) {
    if (typeof window.vcm_OnMenuScaleChange === 'function')
        window.vcm_OnMenuScaleChange(String(val));
}

function vcm_fireOverlayToggle(index, state) {
    if (typeof window.vcm_OnOverlayToggle === 'function')
        window.vcm_OnOverlayToggle(index + '^' + (state ? 'true' : 'false'));
}

// ── SCALE

function vcm_applyScale(val, fireEvent) {
    const clamped = Math.max(VCM_SCALE_MIN, Math.min(VCM_SCALE_MAX, val));
    document.documentElement.style.setProperty('--s-adj', clamped);
    if (vcm_el.scaleSlider) vcm_el.scaleSlider.value = clamped;
    if (vcm_el.scaleVal) vcm_el.scaleVal.textContent = clamped.toFixed(2) + 'x';
    if (fireEvent) vcm_fireScaleChange(clamped);
}

// ── OVERLAY TOGGLE

function vcm_toggleOverlay(index) {
    const nextState = !vcm_overlayState.get(index);
    vcm_overlayState.set(index, nextState);
    vcm_syncToggle(index, nextState);
    vcm_fireOverlayToggle(index, nextState);
}

// ── VISIBILITY

function vcm_openPanel() {
    if (typeof slp_collapseAllPanels === 'function') slp_collapseAllPanels();
    if (vcm_el.backdrop) vcm_el.backdrop.classList.add(VCM_CSS.OPEN);
    if (vcm_el.panel) vcm_el.panel.classList.add(VCM_CSS.OPEN);
    vcm_panelOpen = true;
}

function vcm_closePanel() {
    if (vcm_el.panel) vcm_el.panel.classList.remove(VCM_CSS.OPEN);
    if (vcm_el.backdrop) vcm_el.backdrop.classList.remove(VCM_CSS.OPEN);
    vcm_panelOpen = false;
}

function vcm_collapseAll() {
    vcm_closePanel();
}

// ── SECTION TOGGLES

function vcm_toggleSection(key) {
    vcm_sectionOpen[key] = !vcm_sectionOpen[key];
    const refs = vcm_sectionRefs[key];
    if (!refs) return;
    refs.body.classList.toggle(VCM_CSS.OPEN, vcm_sectionOpen[key]);
    refs.arrow.textContent = vcm_sectionOpen[key] ? '▼' : '▲';
}

// ── HELPERS

function vcm_syncToggle(index, state) {
    const el = vcm_iconEls.get(index);
    if (!el) return;
    if (el.tagName === 'IMG') {
        el.src = state ? VCM_ICON_ON : VCM_ICON_OFF;
        el.alt = state ? '⬤' : '◯';
    } else {
        el.textContent = state ? '⬤' : '◯';
        el.style.color = state ? 'var(--hub-accent,#7a9060)' : 'var(--text-muted)';
    }
}

function vcm_syncAllToggleIcons() {
    for (const [index, state] of vcm_overlayState) vcm_syncToggle(index, state);
}

// ── DOM LOAD

document.addEventListener('DOMContentLoaded', () => {
    vcm_cacheElements();

    if (vcm_el.backdrop) vcm_el.backdrop.addEventListener('click', vcm_collapseAll);
    if (vcm_el.overlaysHeader) vcm_el.overlaysHeader.addEventListener('click', () => vcm_toggleSection('overlays'));

    if (vcm_el.pullTab) { vcm_el.pullTab.addEventListener('click', (e) => { e.stopPropagation();
        if (vcm_panelOpen) { vcm_closePanel(); } else { vcm_openPanel(); } }); }

    if (vcm_el.scaleSlider) {
        const slider = vcm_el.scaleSlider;

        slider.addEventListener('input', e => {
            e.stopPropagation();
            if (vcm_el.scaleVal) vcm_el.scaleVal.textContent = parseFloat(slider.value).toFixed(2) + 'x';
        });

        slider.addEventListener('change', e => {
            e.stopPropagation();
            vcm_applyScale(parseFloat(slider.value), true);
        });

        slider.addEventListener('pointerdown', e => e.stopPropagation());
        slider.addEventListener('keydown', e => e.stopPropagation());

        // RAF-throttled wheel — C++ interop fires at most once per animation frame
        slider.addEventListener('wheel', e => {
            e.preventDefault();
            e.stopPropagation();
            const next = parseFloat(slider.value) + (e.deltaY < 0 ? 1 : -1) * (e.shiftKey ? VCM_SCALE_STEP_SHIFT : VCM_SCALE_STEP);
            vcm_applyScale(next, false);
            if (!vcm_scaleRafPending) {
                vcm_scaleRafPending = requestAnimationFrame(() => {
                    vcm_scaleRafPending = 0;
                    vcm_fireScaleChange(Math.max(VCM_SCALE_MIN, Math.min(VCM_SCALE_MAX, next)));
                });
            }
        }, { passive: false });
    }

    for (const index of VCM_OVERLAY_INDICES) {
        const row = document.getElementById('vcm-row-' + index);
        if (row) row.addEventListener('click', () => vcm_toggleOverlay(index));
    }

    document.addEventListener('pointerdown', e => {
        if (!vcm_panelOpen) return;
        const inPanel = vcm_el.panel && vcm_el.panel.contains(e.target);
        const inTab = vcm_el.pullTab && vcm_el.pullTab.contains(e.target);
        const inBackdrop = vcm_el.backdrop && vcm_el.backdrop.contains(e.target);
        if (!inPanel && !inTab && !inBackdrop) vcm_collapseAll();
    }, { capture: true });

    document.addEventListener('keydown', e => {
        if (document.activeElement &&
            (document.activeElement.tagName === 'INPUT' || document.activeElement.isContentEditable)) return;
        if (e.key === 'Escape') vcm_collapseAll();
    });
});
