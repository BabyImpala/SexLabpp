'use strict';

// ── CONSTANTS

const OAM_COORDS       = ['X', 'Y', 'Z', 'R'];
const OAM_RANGE_R      = 180;
const OAM_RANGE_XYZ    = 200;
const OAM_DRAG_SCALE   = 300;
const OAM_PANEL_DELAY  = 180;
const OAM_RAD_TO_DEG   = 180 / Math.PI;

const OAM_AXIS_INC_KEYS = new Set(['ArrowUp', 'ArrowRight']);
const OAM_ARROW_KEYS    = new Set(['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight']);

const OAM_ICON_ON  = 'overlays/icons/slp_toggle_on.png';
const OAM_ICON_OFF = 'overlays/icons/slp_toggle_off.png';

const OAM_CSS = Object.freeze({
    OPEN:           'open',
    DRAGGING:       'oam_dragging',
    NONZERO:        'nonzero',
    ROW:            'slp-panel-row slp-panel-row--clickable',
    ROW_SELECTED:   'slp-panel-row--selected',
    ROW_NAME:       'slp-panel-row-name',
    BADGE_GREEN:    'slp-badge slp-badge--green',
    BADGE_BLUE:     'slp-badge slp-badge--blue',
    SECTION_HEADER: '.slp-section-header',
    OFFSET_ROW:     'oam-offset-row',
    OFFSET_HEADER:  'oam-offset-header',
    OFFSET_LABEL:   'oam-offset-label',
    OFFSET_VAL:     'oam-offset-val',
    OFFSET_TRACK:   'oam-offset-track',
    OFFSET_FILL:    'oam-offset-fill',
    OFFSET_NEEDLE:  'oam-offset-needle',
    OFFSET_HIT:     'oam-offset-hitarea',
});

const OAM_IDS = Object.freeze({
    OVERLAY:       'overlay_offsetAdjust',
    OFFSETS:       'oamOffsets',
    ACTOR_PICKER:  'actorPicker',
    PANEL:         'oamPanel',
    PANEL_TITLE:   'panelTitle',
    PULL_TAB:      'oamPullTab',
    ICON_STAGE:    'icon-stageOnly',
    TOGGLE_STAGE:  'toggleStageOnlyRow',
    RESET_OFFSETS: 'resetOffsetsRow',
});

// ── STATE

let oam_pickerOpen      = false
let oam_panelOpen       = true
let oam_actorsData      = [];
let oam_hasFurniture    = false;
let oam_centerIsPlayer  = false;
let oam_adjustStageOnly = false;
let oam_selectedId      = null;
let oam_drag            = null;
let oam_dragRafPending  = false;
let oam_sortedItems     = null;

const oam_origValues  = {}; // actorId → { X, Y, Z, R } baseline values
const oam_sliderRefs  = {}; // actorId → { X, Y, Z, R } → { track, needle, fill, valEl, value }

// ── CACHED ELEMENTS

let oam_el = {};
let oam_pickerHeader = null;

function oam_cacheElements() {
    oam_el = {
        overlay:         document.getElementById(OAM_IDS.OVERLAY),
        offsets:         document.getElementById(OAM_IDS.OFFSETS),
        actorPicker:     document.getElementById(OAM_IDS.ACTOR_PICKER),
        panel:           document.getElementById(OAM_IDS.PANEL),
        panelTitle:      document.getElementById(OAM_IDS.PANEL_TITLE),
        pullTab:         document.getElementById(OAM_IDS.PULL_TAB),
        iconStageOnly:   document.getElementById(OAM_IDS.ICON_STAGE),
        toggleStageOnly: document.getElementById(OAM_IDS.TOGGLE_STAGE),
        resetOffsets:    document.getElementById(OAM_IDS.RESET_OFFSETS),
    };
    if (oam_el.actorPicker)
        oam_pickerHeader = oam_el.actorPicker.querySelector(OAM_CSS.SECTION_HEADER);
}

// ── C++ INIT AND DESTROY

window.oam_initOverlay = function(jsonStr) {
    if (oam_el.overlay) oam_el.overlay.style.display = '';
    let data;
    try { data = JSON.parse(jsonStr) || {}; } catch(e) { return; }

    oam_actorsData      = data.oam_actorsData || [];
    oam_hasFurniture    = !!data.oam_hasFurniture;
    oam_centerIsPlayer  = !!data.oam_centerIsPlayer;
    oam_adjustStageOnly = !!data.oam_adjustStageOnly;
    oam_sortedItems     = null;
    oam_syncToggle(oam_el.iconStageOnly, oam_adjustStageOnly);
};

window.oam_destroyOverlay = function() {
    if (oam_drag) {
        oam_drag.track.classList.remove(OAM_CSS.DRAGGING);
        oam_drag = null;
    }
    oam_actorsData      = [];
    oam_hasFurniture    = false;
    oam_centerIsPlayer  = false;
    oam_adjustStageOnly = false;
    oam_selectedId      = null;
    oam_sortedItems     = null;
    oam_dragRafPending  = false;
    for (const key in oam_origValues) delete oam_origValues[key];
    for (const key in oam_sliderRefs) delete oam_sliderRefs[key];
    oam_collapseAll();
    if (oam_el.offsets) oam_el.offsets.innerHTML = '';
    if (oam_el.actorPicker) {
        oam_el.actorPicker.innerHTML = '';
        if (oam_pickerHeader) oam_el.actorPicker.appendChild(oam_pickerHeader);
    }
    if (oam_el.overlay) oam_el.overlay.style.display = 'none';
};

// ── C++ TO JS

window.oam_setOffsetsDisplay = function(payload) {
    // format: actorId^X^Y^Z^R
    const s0 = payload.indexOf('^');
    if (s0 === -1) return;
    const s1 = payload.indexOf('^', s0 + 1);
    const s2 = payload.indexOf('^', s1 + 1);
    const s3 = payload.indexOf('^', s2 + 1);
    if (s3 === -1) return;

    const actorId = payload.substring(0, s0);
    if (!oam_origValues[actorId]) oam_origValues[actorId] = {};

    const rawVals = [
        parseFloat(payload.substring(s0 + 1, s1)),
        parseFloat(payload.substring(s1 + 1, s2)),
        parseFloat(payload.substring(s2 + 1, s3)),
        parseFloat(payload.substring(s3 + 1)),
    ];

    for (let i = 0; i < OAM_COORDS.length; i++) {
        const axis = OAM_COORDS[i];
        const val  = axis === 'R' ? rawVals[i] * OAM_RAD_TO_DEG : rawVals[i];
        if (oam_origValues[actorId][axis] === undefined) oam_origValues[actorId][axis] = val;
        if (oam_drag && oam_drag.actorId === actorId && oam_drag.axis === axis) continue;
        oam_setSliderValue(axis, actorId, val);
    }
};

// ── JS TO C++

function oam_fireActorSelected(actorId) {
    if (typeof window.oam_OnActorSelected === 'function')
        window.oam_OnActorSelected(actorId);
}

function oam_fireOffsetChange(axis, actorId, value) {
    if (typeof window.oam_OnSetOffset === 'function')
        window.oam_OnSetOffset(axis + '^' + actorId + '^' + value.toFixed(6));
}

function oam_fireResetForSelected() {
    if (oam_selectedId === null) return;
    if (typeof window.oam_OnResetOffsets === 'function')
        window.oam_OnResetOffsets('');
    if (oam_origValues[oam_selectedId]) oam_origValues[oam_selectedId] = {};
}

function oam_toggleAdjustStageOnly() {
    oam_adjustStageOnly = !oam_adjustStageOnly;
    oam_syncToggle(oam_el.iconStageOnly, oam_adjustStageOnly);
    if (typeof window.oam_OnSetAdjustStageOnly === 'function')
        window.oam_OnSetAdjustStageOnly(oam_adjustStageOnly ? 'true' : 'false');
}

// ── VISIBILITY

function oam_openActorPicker() {
    if (typeof slp_collapseAllPanels === 'function') slp_collapseAllPanels();
    const items = oam_getProcessedActorsList();
    if (items.length === 1) {
        const item = items[0];
        oam_selectEntityForAdjust(item.id, item.label + (item.isScene ? ' (Scene)' : ''));
    } else {
        oam_buildActorPickerFromList(items);
        oam_el.actorPicker.classList.add(OAM_CSS.OPEN);
    }
    oam_pickerOpen = true
}

function oam_openOffsetAdjustPanel(label) {
    oam_el.actorPicker.classList.remove(OAM_CSS.OPEN);
    oam_el.panelTitle.textContent = label;
    setTimeout(() => oam_el.panel.classList.add(OAM_CSS.OPEN), OAM_PANEL_DELAY);
    oam_panelOpen = true
}

function oam_closePicker() {
    oam_el.actorPicker.classList.remove(OAM_CSS.OPEN);
    oam_pickerOpen = false
}

function oam_closePanel() {
    oam_el.panel.classList.remove(OAM_CSS.OPEN);
    oam_panelOpen = false
}

function oam_collapseAll() {
    oam_closePicker();
    oam_closePanel();
}

// ── ACTOR PICKER

function oam_getProcessedActorsList() {
    if (oam_sortedItems) return oam_sortedItems;

    const items = [];
    if (oam_hasFurniture && !oam_centerIsPlayer)
        items.push({ id: '0', label: 'Center', isScene: true, isPlayer: false });

    const sorted = oam_actorsData.slice().sort((a, b) => {
        if (a.isPlayer !== b.isPlayer) return a.isPlayer ? -1 : 1;
        return a.name.localeCompare(b.name);
    });
    for (const a of sorted)
        items.push({ id: String(a.id), label: a.name, isScene: false, isPlayer: a.isPlayer });

    oam_sortedItems = items;
    return items;
}

function oam_makePickerRow(item) {
    const row  = document.createElement('div');
    const name = document.createElement('span');
    row.className    = OAM_CSS.ROW;
    name.className   = OAM_CSS.ROW_NAME;
    name.textContent = item.label;
    row.appendChild(name);

    if (item.isPlayer) {
        const b = document.createElement('span');
        b.className   = OAM_CSS.BADGE_GREEN;
        b.textContent = SLP_STRINGS.OAM_BADGE_PLAYER;
        row.appendChild(b);
    }
    if (item.isScene) {
        const b = document.createElement('span');
        b.className   = OAM_CSS.BADGE_BLUE;
        b.textContent = SLP_STRINGS.OAM_BADGE_SCENE;
        row.appendChild(b);
    }

    row.dataset.itemId    = item.id;
    row.dataset.itemLabel = item.label;
    row.dataset.isScene   = item.isScene ? '1' : '';
    return row;
}

const oam_pickerRowMap = new Map();

function oam_buildActorPickerFromList(items) {
    const container = oam_el.actorPicker;
    container.innerHTML = '';
    if (oam_pickerHeader) container.appendChild(oam_pickerHeader);

    for (const item of items) {
        let row = oam_pickerRowMap.get(item.id);
        if (!row) {
            row = oam_makePickerRow(item);
            oam_pickerRowMap.set(item.id, row);
        }
        row.classList.toggle(OAM_CSS.ROW_SELECTED, item.id === oam_selectedId);
        container.appendChild(row);
    }
}

// ── OFFSET PANEL

function oam_selectEntityForAdjust(id, label) {
    oam_selectedId = id;
    if (!oam_origValues[id]) oam_origValues[id] = {};

    const saved = {};
    const refs  = oam_sliderRefs[id];
    if (refs) {
        for (const axis of OAM_COORDS)
            if (refs[axis]) saved[axis] = refs[axis].value;
    }

    oam_buildOffsetSliders(id);

    for (const axis of OAM_COORDS)
        if (saved[axis] !== undefined) oam_setSliderValue(axis, id, saved[axis]);

    oam_openOffsetAdjustPanel(label);
    oam_fireActorSelected(id);
}

// ── SLIDERS

function oam_buildOffsetSliders(actorId) {
    if (!oam_el.offsets) return;
    oam_el.offsets.innerHTML = '';
    delete oam_sliderRefs[actorId];
    oam_sliderRefs[actorId] = {};
    for (const axis of OAM_COORDS)
        oam_el.offsets.appendChild(oam_makeOffsetRow(axis, actorId));
}

function oam_makeOffsetRow(axis, actorId) {
    const row    = document.createElement('div');
    const header = document.createElement('div');
    row.className    = OAM_CSS.OFFSET_ROW;
    header.className = OAM_CSS.OFFSET_HEADER;

    const label    = document.createElement('span');
    label.className    = OAM_CSS.OFFSET_LABEL;
    label.textContent  = axis;
    label.title        = 'Double-click to reset';
    label.style.cursor = 'pointer';

    const valInput     = document.createElement('input');
    valInput.type      = 'text';
    valInput.className = OAM_CSS.OFFSET_VAL;
    valInput.value     = '0';

    const track  = document.createElement('div');
    const fill   = document.createElement('div');
    const needle = document.createElement('div');
    const hit    = document.createElement('div');

    track.className   = OAM_CSS.OFFSET_TRACK;
    fill.className    = OAM_CSS.OFFSET_FILL;
    needle.className  = OAM_CSS.OFFSET_NEEDLE;
    needle.style.left = '50%';
    hit.className     = OAM_CSS.OFFSET_HIT;

    track.appendChild(fill);
    track.appendChild(needle);
    track.appendChild(hit);
    header.appendChild(label);
    header.appendChild(valInput);
    row.appendChild(header);
    row.appendChild(track);

    oam_sliderRefs[actorId][axis] = { track, needle, fill, valEl: valInput, value: 0 };

    oam_attachTrackListeners(track, axis, actorId, valInput);
    oam_attachWheelListener(track, axis, actorId);
    oam_attachInputListeners(valInput, track, axis, actorId);
    oam_attachLabelReset(label, axis, actorId);

    return row;
}

// ── SLIDER DISPLAY

function oam_setSliderValue(axis, actorId, value) {
    const refs = oam_sliderRefs[actorId]?.[axis];
    if (!refs) return;
    const { track, needle, fill, valEl } = refs;

    refs.value = value; // numeric — avoids dataset read/write string coercion on hot paths
    track.dataset.value = value;

    const range = axis === 'R' ? OAM_RANGE_R : OAM_RANGE_XYZ;
    const pct   = Math.max(0, Math.min(100, 50 + (value / range) * 50));
    needle.style.left = pct + '%';

    if (value >= 0) {
        fill.style.left  = '50%';
        fill.style.width = Math.max(0, pct - 50) + '%';
    } else {
        fill.style.left  = pct + '%';
        fill.style.width = Math.max(0, 50 - pct) + '%';
    }

    if (document.activeElement !== valEl)
        valEl.value = (value > 0 ? '+' : '') + Math.round(value);

    valEl.classList.toggle(OAM_CSS.NONZERO, Math.abs(value) >= 1);
}

// ── INPUT HANDLERS

function oam_attachTrackListeners(track, axis, actorId) {
    track.addEventListener('pointerdown', e => {
        if (e.button !== 0) return;
        e.preventDefault();
        e.stopPropagation();
        const refs = oam_sliderRefs[actorId]?.[axis];
        oam_drag = { track, axis, actorId, startX: e.clientX, startValue: refs ? refs.value : 0 };
        track.setPointerCapture(e.pointerId);
        track.classList.add(OAM_CSS.DRAGGING);
    });

    track.addEventListener('pointermove', e => {
        if (!oam_drag || oam_drag.track !== track) return;
        const range = axis === 'R' ? OAM_RANGE_R : OAM_RANGE_XYZ;
        let value = Math.round(oam_drag.startValue + (e.clientX - oam_drag.startX) * (range / OAM_DRAG_SCALE));
        if (axis === 'R') value = Math.max(-OAM_RANGE_R, Math.min(OAM_RANGE_R, value));
        oam_setSliderValue(axis, actorId, value);
        // Throttle C++ interop to one call per animation frame
        if (!oam_dragRafPending) {
            oam_dragRafPending = true;
            requestAnimationFrame(() => {
                oam_dragRafPending = false;
                if (oam_drag) {
                    const r = oam_sliderRefs[oam_drag.actorId]?.[oam_drag.axis];
                    if (r) oam_fireOffsetChange(oam_drag.axis, oam_drag.actorId, r.value);
                }
            });
        }
    });

    const endDrag = e => {
        if (!oam_drag || oam_drag.track !== track) return;
        track.releasePointerCapture(e.pointerId);
        track.classList.remove(OAM_CSS.DRAGGING);
        oam_drag = null;
        oam_dragRafPending = false;
    };
    track.addEventListener('pointerup',     endDrag);
    track.addEventListener('pointercancel', endDrag);

    track.setAttribute('tabindex', '-1');
    track.addEventListener('mouseenter', () => track.focus());
    track.addEventListener('mouseleave', () => track.blur());

    track.addEventListener('keydown', e => {
        if (!OAM_ARROW_KEYS.has(e.key)) return;
        e.preventDefault();
        const refs  = oam_sliderRefs[actorId]?.[axis];
        let newVal  = (refs ? refs.value : 0) + (OAM_AXIS_INC_KEYS.has(e.key) ? 1 : -1);
        if (axis === 'R') newVal = Math.max(-OAM_RANGE_R, Math.min(OAM_RANGE_R, newVal));
        oam_setSliderValue(axis, actorId, newVal);
        oam_fireOffsetChange(axis, actorId, newVal);
    });
}

// RAF-throttled wheel — matches drag throttle pattern, avoids per-tick C++ interop
function oam_attachWheelListener(track, axis, actorId) {
    let rafPending = 0;
    track.addEventListener('wheel', e => {
        e.preventDefault();
        e.stopPropagation();
        const refs  = oam_sliderRefs[actorId]?.[axis];
        let newVal  = (refs ? refs.value : 0) + (e.deltaY < 0 ? 1 : -1);
        if (axis === 'R') newVal = Math.max(-OAM_RANGE_R, Math.min(OAM_RANGE_R, newVal));
        oam_setSliderValue(axis, actorId, newVal);
        if (!rafPending) {
            rafPending = requestAnimationFrame(() => {
                rafPending = 0;
                const r = oam_sliderRefs[actorId]?.[axis];
                if (r) oam_fireOffsetChange(axis, actorId, r.value);
            });
        }
    }, { passive: false });
}

function oam_attachInputListeners(valInput, track, axis, actorId) {
    const commit = () => {
        const refs = oam_sliderRefs[actorId]?.[axis];
        let v = parseFloat(valInput.value);
        if (isNaN(v)) v = refs ? refs.value : 0;
        v = Math.round(v);
        if (axis === 'R') v = Math.max(-OAM_RANGE_R, Math.min(OAM_RANGE_R, v));
        oam_setSliderValue(axis, actorId, v);
        oam_fireOffsetChange(axis, actorId, v);
    };

    valInput.addEventListener('keydown', e => {
        e.stopPropagation();
        if (e.key === 'Enter') { commit(); valInput.blur(); return; }
        if (e.key === 'Escape') {
            const refs = oam_sliderRefs[actorId]?.[axis];
            oam_setSliderValue(axis, actorId, refs ? refs.value : 0);
            valInput.blur();
        }
    });
    valInput.addEventListener('blur', commit);
    valInput.addEventListener('pointerdown', e => e.stopPropagation());
}

function oam_attachLabelReset(label, axis, actorId) {
    label.addEventListener('dblclick', e => {
        e.preventDefault();
        const orig = oam_origValues[actorId]?.[axis] ?? 0;
        oam_setSliderValue(axis, actorId, orig);
        oam_fireOffsetChange(axis, actorId, orig);
    });
}

// ── HELPERS

function oam_syncToggle(el, state) {
    if (!el) return;
    if (el.tagName === 'IMG') {
        el.src = state ? OAM_ICON_ON : OAM_ICON_OFF;
        el.alt = state ? '⬤' : '◯';
    } else {
        el.textContent = state ? '⬤' : '◯';
        el.style.color = state ? 'var(--hub-accent,#7a9060)' : 'var(--text-muted)';
    }
}

// ── DOM LOAD

document.addEventListener('DOMContentLoaded', () => {
    oam_cacheElements();

    if (oam_el.toggleStageOnly) oam_el.toggleStageOnly.addEventListener('click', oam_toggleAdjustStageOnly);
    if (oam_el.resetOffsets) oam_el.resetOffsets.addEventListener('click', oam_fireResetForSelected);

    if (oam_el.pullTab) { oam_el.pullTab.addEventListener('click', (e) => { e.stopPropagation();
        if (oam_pickerOpen || oam_panelOpen) { oam_collapseAll(); } else { oam_openActorPicker(); } }); }

    if (oam_el.iconStageOnly) {
        oam_el.iconStageOnly.addEventListener('error', function() {
            this.outerHTML = '<span id="' + OAM_IDS.ICON_STAGE + '" class="slp-toggle-text">○</span>';
        });
    }

    if (oam_el.actorPicker) {
        oam_el.actorPicker.addEventListener('click', e => {
            e.stopPropagation();
            const row = e.target.closest('[data-item-id]');
            if (!row) return;
            const id = row.dataset.itemId;
            const label = row.dataset.itemLabel;
            const isScene = !!row.dataset.isScene;
            oam_selectEntityForAdjust(id, label + (isScene ? ' (Scene)' : ''));
        });
    }

    document.addEventListener('pointerdown', e => {
        const inPanel = oam_el.panel && oam_el.panel.contains(e.target);
        const inPicker = oam_el.actorPicker && oam_el.actorPicker.contains(e.target);
        const inTab = oam_el.pullTab && oam_el.pullTab.contains(e.target);
        if (!inPanel && !inPicker && !inTab) oam_collapseAll();
    }, { capture: true });

    document.addEventListener('keydown', e => {
        if (e.key === 'Escape') oam_collapseAll();
    });
});
