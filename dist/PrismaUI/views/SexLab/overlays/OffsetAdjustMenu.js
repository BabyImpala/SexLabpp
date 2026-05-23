'use strict';

const oam_COORDS         = ['X', 'Y', 'Z', 'R'];
let oam_stateStageOnly   = false;
let oam_actorsData       = [];
let oam_hasFurniture     = false;
let oam_centerIsPlayer   = false;
let oam_selectedId       = null;
let oam_drag             = null;
const oam_origValues     = {};
const oam_overlayEl      = document.getElementById('overlay_offsetAdjust');

// ── C++ INIT AND DESTROY

window.oam_initOverlay = function(jsonStr) {
  if (oam_overlayEl) oam_overlayEl.style.display = '';
  let data;
  try { data = JSON.parse(jsonStr) || {}; } catch(e) { return; }
  oam_hasFurniture = !!data.oam_hasFurniture;
  oam_centerIsPlayer = !!data.oam_centerIsPlayer;
  oam_stateStageOnly = !!data.adjustStageOnly;
  oam_syncToggle('icon-stageOnly', oam_stateStageOnly);
  oam_actorsData = data.actors || [];
};

window.oam_destroyOverlay = function() {
  // Cancel any active oam_drag
  if (oam_drag && oam_drag.track) {
    oam_drag.track.classList.remove('oam_dragging');
  }
  oam_drag = null;
  // Reset state
  oam_stateStageOnly = false;
  oam_actorsData     = [];
  oam_hasFurniture   = false;
  oam_centerIsPlayer = false;
  oam_selectedId     = null;
  // Clear cached original values
  for (const key in oam_origValues) delete oam_origValues[key];
  // Close panels then clear dynamic DOM
  oam_collapseAll();
  const offsets = document.getElementById('oamOffsets');
  if (offsets) offsets.innerHTML = '';
  const picker = document.getElementById('actorPicker');
  if (picker) {
    // Preserve the static section header, remove generated rows
    const header = picker.querySelector('.slp-section-header');
    picker.innerHTML = '';
    if (header) picker.appendChild(header);
  }
  if (oam_overlayEl) oam_overlayEl.style.display = 'none';
};

// ── C++ TO JS

window.oam_setOffsetsDisplay = function(payload) {
  const p = payload.split('^');
  if (p.length < 5) return;
  const actorId = p[0];
  if (!oam_origValues[actorId]) oam_origValues[actorId] = {};

  for (let i = 0; i < 4; i++) {
    const axis = oam_COORDS[i];
    const raw  = parseFloat(p[1 + i]);
    const val  = (axis === 'R') ? raw * (180 / Math.PI) : raw;

    if (oam_origValues[actorId][axis] === undefined) oam_origValues[actorId][axis] = val;
    if (oam_drag && oam_drag.actorId === actorId && oam_drag.axis === axis) continue;

    oam_setSliderValue(axis, actorId, val);
  }
};

// ── JS TO C++

function oam_fireActorSelected(actorId) {
  if (typeof window.oam_OnActorSelected === 'function') {
    window.oam_OnActorSelected(actorId);
  }
};

function oam_fireOffsetChange(axis, actorId, value) {
  if (typeof window.oam_OnSetOffset === 'function') {
    window.oam_OnSetOffset(`${axis}^${actorId}^${value.toFixed(6)}`);
  }
};

function oam_fireResetForSelected() {
  if (oam_selectedId !== null) {
    if (typeof window.oam_OnResetOffsets === 'function') {
      window.oam_OnResetOffsets('');
    }
    if (oam_origValues[oam_selectedId]) oam_origValues[oam_selectedId] = {};
  }
};

function oam_toggleAdjustStageOnly() {
  oam_stateStageOnly = !oam_stateStageOnly;
  oam_syncToggle('icon-stageOnly', oam_stateStageOnly);
  if (typeof window.oam_OnSetAdjustStageOnly === 'function') {
    window.oam_OnSetAdjustStageOnly(oam_stateStageOnly ? 'true' : 'false');
  }
};

// ── VISIBILITY

function oam_openActorPicker() {
  if (typeof slp_collapseAllPanels === 'function') {
    slp_collapseAllPanels();
  }
  const items = oam_getProcessedActorsList();
  if (items.length === 1) {
    const singleItem = items[0];
    const computedLabel = singleItem.label + (singleItem.isScene ? ' (Scene)' : '');
    oam_selectEntityForAdjust(singleItem.id, computedLabel);
  } else {
    oam_buildActorPickerFromList(items);
    document.getElementById('actorPicker').classList.add('open');
  }
};

function oam_openOffsetAdjustPanel(label) {
  document.getElementById('actorPicker').classList.remove('open');
  document.getElementById('panelTitle').textContent = label;
  setTimeout(() => { document.getElementById('oamPanel').classList.add('open'); }, 180);
};

function oam_closePicker() {
  const picker = document.getElementById('actorPicker');
  const pullTab = document.getElementById('oamPullTab');
  if (picker && picker.classList.contains('open')) {
    picker.classList.remove('open');
  }
};

function oam_closePanel() {
  const panel = document.getElementById('oamPanel');
  if (panel && panel.classList.contains('open')) {
    panel.classList.remove('open');
  }
};

function oam_collapseAll() {
  oam_closePicker();
  oam_closePanel();
};

// ── BUILD ACTOR PICKER

function oam_getProcessedActorsList() {
  const items = [];
  if (oam_hasFurniture && !oam_centerIsPlayer) {
    items.push({ id: '0', label: 'Center', isScene: true, isPlayer: false });
  }

  oam_actorsData.slice()
    .sort((a, b) => {
      if (a.isPlayer !== b.isPlayer) return a.isPlayer ? -1 : 1;
      return a.name.localeCompare(b.name);
    })
    .forEach(a => {
      items.push({ id: String(a.id), label: a.name, isScene: false, isPlayer: a.isPlayer });
    });
    
  return items;
}

function oam_buildActorPickerFromList(items) {
  const container = document.getElementById('actorPicker');
  const header = container.querySelector('.slp-section-header');
  container.innerHTML = '';
  if (header) container.appendChild(header);

  items.forEach(item => {
    const row = document.createElement('div');
    row.className = 'slp-panel-row slp-panel-row--clickable';
    if (item.id === oam_selectedId) row.classList.add('slp-panel-row--selected');

    const name = document.createElement('span');
    name.className = 'slp-panel-row-name';
    name.textContent = item.label;
    row.appendChild(name);

    if (item.isPlayer) {
      const b = document.createElement('span');
      b.className = 'slp-badge slp-badge--green';
      b.textContent = SLP_STRINGS.OAM_BADGE_PLAYER;
      row.appendChild(b);
    }
    if (item.isScene) {
      const b2 = document.createElement('span');
      b2.className = 'slp-badge slp-badge--blue';
      b2.textContent = SLP_STRINGS.OAM_BADGE_SCENE;
      row.appendChild(b2);
    }

    row.addEventListener('click', e => {
      e.stopPropagation();
      oam_selectEntityForAdjust(item.id, item.label + (item.isScene ? ' (Scene)' : ''));
    });
    container.appendChild(row);
  });
};

// ── BUILD OFFSET PANEL

function oam_selectEntityForAdjust(id, label) {
  oam_selectedId = id;
  if (!oam_origValues[id]) oam_origValues[id] = {};

  const saved = {};
  oam_COORDS.forEach(axis => {
    const t = document.getElementById(`track-${axis}-${id}`);
    if (t) saved[axis] = parseFloat(t.dataset.value) || 0;
  });

  oam_buildOffsetSliders(id);

  oam_COORDS.forEach(axis => {
    if (saved[axis] !== undefined) oam_setSliderValue(axis, id, saved[axis]);
  });

  oam_openOffsetAdjustPanel(label);
  oam_fireActorSelected(id);
};

// ── BUILD SLIDERS

function oam_buildOffsetSliders(actorId) {
  const container = document.getElementById('oamOffsets');
  container.innerHTML = '';
  oam_COORDS.forEach(axis => { container.appendChild(oam_makeOffsetRow(axis, actorId)); });
};

function oam_makeOffsetRow(axis, actorId) {
  const row = document.createElement('div');
  row.className = 'oam-offset-row';

  const header = document.createElement('div');
  header.className = 'oam-offset-header';

  const label = document.createElement('span');
  label.className = 'oam-offset-label';
  label.textContent = axis;
  label.title = 'Double-click to reset';
  label.style.cursor = 'pointer';
  label.addEventListener('dblclick', e => {
    e.preventDefault();
    const orig = (oam_origValues[actorId] && oam_origValues[actorId][axis] !== undefined)
                 ? oam_origValues[actorId][axis] : 0;
    oam_setSliderValue(axis, actorId, orig);
    oam_fireOffsetChange(axis, actorId, orig);
  });

  const valInput = document.createElement('input');
  valInput.type = 'text';
  valInput.className = 'oam-offset-val';
  valInput.id = `val-${axis}-${actorId}`;
  valInput.value = '0';

  const commitInput = () => {
    let v = parseFloat(valInput.value);
    if (isNaN(v)) {
      const t = document.getElementById(`track-${axis}-${actorId}`);
      v = parseFloat(t ? t.dataset.value : 0) || 0;
    }
    v = Math.round(v);
    if (axis === 'R') v = Math.max(-180, Math.min(180, v));
    oam_setSliderValue(axis, actorId, v);
    oam_fireOffsetChange(axis, actorId, v);
  };

  valInput.addEventListener('keydown', e => {
    e.stopPropagation();
    if (e.key === 'Enter')  { commitInput(); valInput.blur(); }
    if (e.key === 'Escape') {
      const t = document.getElementById(`track-${axis}-${actorId}`);
      oam_setSliderValue(axis, actorId, parseFloat(t ? t.dataset.value : 0) || 0);
      valInput.blur();
    }
  });
  valInput.addEventListener('blur', commitInput);
  valInput.addEventListener('pointerdown', e => { e.stopPropagation(); });

  header.appendChild(label);
  header.appendChild(valInput);

  const track = document.createElement('div');
  track.className = 'oam-offset-track';
  track.id = `track-${axis}-${actorId}`;
  track.dataset.value = '0';

  const fill = document.createElement('div');
  fill.className = 'oam-offset-fill';
  fill.id = `fill-${axis}-${actorId}`;

  const needle = document.createElement('div');
  needle.className = 'oam-offset-needle';
  needle.id = `needle-${axis}-${actorId}`;
  needle.style.left = '50%';

  const hit = document.createElement('div');
  hit.className = 'oam-offset-hitarea';

  track.appendChild(fill);
  track.appendChild(needle);
  track.appendChild(hit);
  row.appendChild(header);
  row.appendChild(track);

  oam_attachTrackListeners(track, axis, actorId);
  oam_attachWheelListener(track, axis, actorId);

  return row;
};

// ── SLIDER DISPLAY

function oam_setSliderValue(axis, actorId, value) {
  const track  = document.getElementById(`track-${axis}-${actorId}`);
  const needle = document.getElementById(`needle-${axis}-${actorId}`);
  const fill   = document.getElementById(`fill-${axis}-${actorId}`);
  const valEl  = document.getElementById(`val-${axis}-${actorId}`);
  if (!track || !needle || !fill || !valEl) return;

  track.dataset.value = value;

  const range = (axis === 'R') ? 180 : 200;
  const pct   = Math.max(0, Math.min(100, 50 + (value / range) * 50));
  needle.style.left = `${pct}%`;

  if (value >= 0) {
    fill.style.left  = '50%';
    fill.style.width = `${Math.max(0, pct - 50)}%`;
  } else {
    fill.style.left  = `${pct}%`;
    fill.style.width = `${Math.max(0, 50 - pct)}%`;
  }

  if (document.activeElement !== valEl) {
    valEl.value = (value > 0 ? '+' : '') + Math.round(value);
  }
  valEl.classList.toggle('nonzero', Math.abs(value) >= 1);
};

// ── USER INPUTS (oam_drag, WHEEL, KEYS)

function oam_attachTrackListeners(track, axis, actorId) {
  track.addEventListener('pointerdown', e => {
    if (e.button !== 0) return;
    e.preventDefault();
    e.stopPropagation();
    oam_drag = {
      track,
      axis,
      actorId,
      startX     : e.clientX,
      startValue : parseFloat(track.dataset.value) || 0,
    };
    track.setPointerCapture(e.pointerId);
    track.classList.add('oam_dragging');
  });

  track.addEventListener('pointermove', e => {
    if (!oam_drag || oam_drag.track !== track) return;
    const range = (axis === 'R') ? 180 : 200;
    let value = Math.round(oam_drag.startValue + (e.clientX - oam_drag.startX) * (range / 300));
    if (axis === 'R') value = Math.max(-180, Math.min(180, value));
    oam_setSliderValue(axis, actorId, value);
    oam_fireOffsetChange(axis, actorId, value);
  });

  const endoam_drag = e => {
    if (!oam_drag || oam_drag.track !== track) return;
    track.releasePointerCapture(e.pointerId);
    track.classList.remove('oam_dragging');
    oam_drag = null;
  };

  track.addEventListener('pointerup', endoam_drag);
  track.addEventListener('pointercancel', endoam_drag);

  track.setAttribute('tabindex', '-1');
  track.addEventListener('mouseenter', () => track.focus());
  track.addEventListener('mouseleave', () => track.blur());
  
  track.addEventListener('keydown', e => {
    if (!['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.key)) return;
    e.preventDefault();
    const current = parseFloat(track.dataset.value) || 0;
    let newVal  = current + (['ArrowUp', 'ArrowRight'].includes(e.key) ? 1 : -1);
    if (axis === 'R') newVal = Math.max(-180, Math.min(180, newVal));
    oam_setSliderValue(axis, actorId, newVal);
    oam_fireOffsetChange(axis, actorId, newVal);
  });
};

function oam_attachWheelListener(track, axis, actorId) {
  track.addEventListener('wheel', e => {
    e.preventDefault();
    e.stopPropagation();
    const current = parseFloat(track.dataset.value) || 0;
    let newVal = current + (e.deltaY < 0 ? 1 : -1);
    if (axis === 'R') newVal = Math.max(-180, Math.min(180, newVal));
    oam_setSliderValue(axis, actorId, newVal);
    oam_fireOffsetChange(axis, actorId, newVal);
  }, { passive: false });
};

// ── HELPERS

function oam_syncToggle(id, state) {
  const el = document.getElementById(id);
  if (!el) return;
  if (el.tagName === 'IMG') {
    el.src = state ? 'overlays/icons/slp_toggle_on.png' : 'overlays/icons/slp_toggle_off.png';
    el.alt = state ? '⬤' : '◯';
  } else {
    el.textContent = state ? '⬤' : '◯';
    el.style.color = state ? 'var(--hub-accent,#7a9060)' : 'var(--text-muted)';
  }
};

// ── INIT

document.addEventListener('DOMContentLoaded', () => {
  const pullTab = document.getElementById('oamPullTab');
  const toggleStageOnlyRow = document.getElementById('toggleStageOnlyRow');
  const resetOffsetsRow = document.getElementById('resetOffsetsRow');
  const iconStageOnly = document.getElementById('icon-stageOnly');

  if (pullTab) {
    pullTab.addEventListener('click', oam_openActorPicker);
  }

  // Outside-click OR Esc-Key
  document.addEventListener('pointerdown', e => {
    const panel  = document.getElementById('oamPanel');
    const picker = document.getElementById('actorPicker');
    const tab    = document.getElementById('oamPullTab');

    const inPanel  = panel  && panel.contains(e.target);
    const inPicker = picker && picker.contains(e.target);
    const inTab    = tab    && tab.contains(e.target);

    if (!inPanel && !inPicker && !inTab) {
      oam_collapseAll();
    }
  }, { capture: true });
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape') oam_collapseAll();
  });

  if (toggleStageOnlyRow) {
    toggleStageOnlyRow.addEventListener('click', oam_toggleAdjustStageOnly);
  }
  if (resetOffsetsRow) {
    resetOffsetsRow.addEventListener('click', oam_fireResetForSelected);
  }
  if (iconStageOnly) {
    iconStageOnly.addEventListener('error', function() {
      this.outerHTML = '<span id="icon-stageOnly" class="slp-toggle-text">○</span>';
    })
  }
});