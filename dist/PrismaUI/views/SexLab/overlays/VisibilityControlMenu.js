'use strict';

let vcm_sectionOpen  = { overlays: true };
const vcm_overlayEl  = document.getElementById('overlay_visibilityControl');

// Index maps for overlays must explicitly match PrismaUI::PrismaOverlayIndex
let vcm_overlayState = {
  "-1": true, // Game HUD
  "1" : true, // AnimSpeedOverlay
  "2" : true, // EnjoymentBars
  "3" : true, // OffsetAdjustMenu
  "4" : true, // SceneSelectorMenu
  "5" : true  // ThreadConfigMenu
};

// ── C++ INIT AND DESTROY

window.vcm_initOverlay = function(jsonStr) {
  if (vcm_overlayEl) vcm_overlayEl.style.display = '';
  let data;
  try { data = JSON.parse(jsonStr) || {}; } catch (e) { return; }

  const liveScaleMult = parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--s-adj')) || 1.5;
  const persisted = (data.scaleAdj !== undefined) ? data.scaleAdj : liveScaleMult;
  vcm_applyScale(persisted, false);

  const states = data.states || {};
  for (const index of Object.keys(vcm_overlayState)) {
    if (states[index] !== undefined) {
      vcm_overlayState[index] = !!states[index];
    }
  }
  vcm_syncAllToggleIcons();
};

window.vcm_destroyOverlay = function() {
  vcm_closePanel();
  vcm_sectionOpen = { overlays: true };
  for (const index of Object.keys(vcm_overlayState)) {
    vcm_overlayState[index] = true;
  }
  vcm_syncAllToggleIcons();
  if (vcm_overlayEl) vcm_overlayEl.style.display = 'none';
};

// ── C++ TO JS

window.vcm_setOverlayState = function(payload) {
  const sep = payload.indexOf('^');
  if (sep === -1) return;
  const index = payload.substring(0, sep);
  const state = payload.substring(sep + 1) === 'true';
  if (vcm_overlayState[index] !== undefined) {
    vcm_overlayState[index] = state;
    vcm_syncToggle('vcm-icon-' + index, state);
  }
};

// ── JS TO C++

function vcm_fireScaleChange(val) {
  if (typeof window.vcm_OnMenuScaleChange === 'function') {
    window.vcm_OnMenuScaleChange(String(val));
  }
};

function vcm_fireOverlayToggle(index, state) {
  if (typeof window.vcm_OnOverlayToggle === 'function') {
    window.vcm_OnOverlayToggle(index + '^' + (state ? 'true' : 'false'));
  }
};

// ── SCALE

function vcm_applyScale(val, fireEvent) {
  const clamped = Math.max(0.5, Math.min(3.0, val));
  document.documentElement.style.setProperty('--s-adj', clamped);

  const slider = document.getElementById('vcmScaleSlider');
  if (slider) slider.value = clamped;

  const textVal = document.getElementById('vcmScaleVal');
  if (textVal) textVal.textContent = clamped.toFixed(2) + 'x';

  if (fireEvent) vcm_fireScaleChange(clamped);
};

// ── OVERLAY TOGGLE

function vcm_toggleOverlay(index) {
  const nextState = !vcm_overlayState[index];
  vcm_overlayState[index] = nextState;
  vcm_syncToggle('vcm-icon-' + index, nextState);
  vcm_fireOverlayToggle(index, nextState);
};

// ── VISIBILITY

function vcm_openPanel() {
  slp_collapseAllPanels();
  document.getElementById('vcmBackdrop').classList.add('open');
  document.getElementById('vcmPanel').classList.add('open');
};

function vcm_closePanel() {
  document.getElementById('vcmPanel').classList.remove('open');
  document.getElementById('vcmBackdrop').classList.remove('open');
};

function vcm_collapseAll() {
  vcm_closePanel();
};

// ── SECTION TOGGLES

function vcm_toggleSection(key) {
  vcm_sectionOpen[key] = !vcm_sectionOpen[key];
  document.getElementById(key + '-body').classList.toggle('open', vcm_sectionOpen[key]);
  document.getElementById(key + '-arrow').textContent = vcm_sectionOpen[key] ? '▼' : '▲';
};

// ── HELPERS — TOGGLE ICON

function vcm_syncToggle(id, state) {
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

function vcm_syncAllToggleIcons() {
  for (const index of Object.keys(vcm_overlayState)) {
    vcm_syncToggle('vcm-icon-' + index, vcm_overlayState[index]);
  }
};

// ── DOM LOAD

document.addEventListener('DOMContentLoaded', () => {
  const pullTab        = document.getElementById('vcmPullTab');
  const backdrop       = document.getElementById('vcmBackdrop');
  const overlaysHeader = document.getElementById('overlays-header');
  const slider         = document.getElementById('vcmScaleSlider');

  if (pullTab)        pullTab.addEventListener('click', vcm_openPanel);
  if (backdrop)       backdrop.addEventListener('click', vcm_collapseAll);
  if (overlaysHeader) overlaysHeader.addEventListener('click', () => vcm_toggleSection('overlays'));

  if (slider) {
    slider.addEventListener('input',  e => {
      e.stopPropagation();
      const textVal = document.getElementById('vcmScaleVal');
      if (textVal) textVal.textContent = parseFloat(slider.value).toFixed(2) + 'x';
    });
    slider.addEventListener('change', e => {
      e.stopPropagation();
      vcm_applyScale(parseFloat(slider.value), true);
    });
    slider.addEventListener('pointerdown', e => e.stopPropagation());
    slider.addEventListener('keydown',     e => e.stopPropagation());
    slider.addEventListener('wheel', e => {
      e.preventDefault(); e.stopPropagation();
      const step = e.shiftKey ? 0.1 : 0.01;
      vcm_applyScale(parseFloat(slider.value) + (e.deltaY < 0 ? step : -step), true);
    }, { passive: false });
  }

  for (const index of Object.keys(vcm_overlayState)) {
    const row = document.getElementById('vcm-row-' + index);
    if (row) row.addEventListener('click', () => vcm_toggleOverlay(index));
  }

  document.addEventListener('pointerdown', e => {
    const panel    = document.getElementById('vcmPanel');
    const pullTab  = document.getElementById('vcmPullTab');
    const backdrop = document.getElementById('vcmBackdrop');
    if (panel && panel.classList.contains('open') &&
        !panel.contains(e.target) &&
        !pullTab.contains(e.target) &&
        !backdrop.contains(e.target)) {
      vcm_collapseAll();
    }
  }, { capture: true });

  document.addEventListener('keydown', e => {
    if (document.activeElement && (document.activeElement.tagName === 'INPUT' || document.activeElement.isContentEditable)) return;
    if (e.key === 'Escape') vcm_collapseAll();
  });
});
