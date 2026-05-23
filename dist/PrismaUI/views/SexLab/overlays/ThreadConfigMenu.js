'use strict';

let tcm_stateAutoPlay  = false;
let tcm_actorsData     = [];
let tcm_sectionOpen    = { thread: true, actors: true };
let tcm_openDd         = null;
const tcm_overlayEl    = document.getElementById('overlay_threadConfig');

// ── C++ INIT AND DESTROY

window.tcm_initOverlay = function(jsonStr) {
  if (tcm_overlayEl) tcm_overlayEl.style.display = '';
  let data;
  try { data = JSON.parse(jsonStr); } catch(e) { return; }
  tcm_stateAutoPlay = !!data.autoPlay;
  tcm_syncToggle('icon-autoplay', tcm_stateAutoPlay);
  tcm_actorsData = data.actors || [];
  tcm_buildActors();
};

window.tcm_destroyOverlay = function() {
  // Close any open dropdown and detach its floating list from body
  tcm_closeDropdowns();
  // Reset state
  tcm_stateAutoPlay = false;
  tcm_actorsData    = [];
  tcm_sectionOpen   = { thread: true, actors: true };
  // Close panel and clear dynamic actor DOM
  tcm_closePanel();
  const body = document.getElementById('actors-body');
  if (body) body.innerHTML = '';
  // Reset autoplay toggle icon to off
  tcm_syncToggle('icon-autoplay', false);
  if (tcm_overlayEl) tcm_overlayEl.style.display = 'none';
};

// ── C++ TO JS

window.tcm_setPermutation = function(payload) {
  const p = payload.split('^');
  if (p.length < 3) return;
  const el = document.getElementById('perm-val-' + p[0]);
  if (el) el.textContent = p[1] + ' / ' + p[2];
};

// ── JS TO C++

function tcm_fireRandomScene() {
  if (typeof window.tcm_OnRandomScene === 'function')
    window.tcm_OnRandomScene('');
};

function tcm_fireMoveScene() {
  if (typeof window.tcm_OnMoveScene === 'function')
    window.tcm_OnMoveScene('');
};

function tcm_fireAutoPlaySet(val) {
  if (typeof window.tcm_OnAutoPlaySet === 'function')
    window.tcm_OnAutoPlaySet(val);
};

function tcm_fireNextPermutation(actorId) {
  if (typeof window.tcm_OnNextPermutation === 'function')
    window.tcm_OnNextPermutation(actorId);
};

function tcm_fireSetExpression(actorId, exprId) {
  if (typeof window.tcm_OnSetExpression === 'function')
    window.tcm_OnSetExpression(actorId + '|' + exprId);
};

function tcm_fireSetVoice(actorId, voiceId) {
  if (typeof window.tcm_OnSetVoice === 'function')
    window.tcm_OnSetVoice(actorId + '|' + voiceId);
};

function tcm_fireSetActorAlpha(actorId, val) {
  if (typeof window.tcm_OnSetActorAlpha === 'function')
    window.tcm_OnSetActorAlpha(actorId + '^' + val);
};

// ── VISIBILITY

function tcm_openPanel() {
  slp_collapseAllPanels();
  document.getElementById('tcmBackdrop').classList.add('open');
  document.getElementById('tcmPanel').classList.add('open');
};

function tcm_closePanel() {
  document.getElementById('tcmPanel').classList.remove('open');
  document.getElementById('tcmBackdrop').classList.remove('open');
};

function tcm_collapseAll() {
  tcm_closePanel();
};

// ── SECTION TOGGLES

function tcm_toggleSection(key) {
  tcm_sectionOpen[key] = !tcm_sectionOpen[key];
  document.getElementById(key + '-body').classList.toggle('open', tcm_sectionOpen[key]);
  document.getElementById(key + '-arrow').textContent = tcm_sectionOpen[key] ? '▼' : '▲';
};

function tcm_toggleActorCard(id) {
  const body  = document.getElementById('actor-body-'  + id);
  const arrow = document.getElementById('actor-arrow-' + id);
  if (!body) return;
  const open = body.classList.toggle('open');
  arrow.textContent = open ? '▼' : '▲';
};

// ── AUTOPLAY

function tcm_toggleAutoPlay() {
  tcm_stateAutoPlay = !tcm_stateAutoPlay;
  tcm_syncToggle('icon-autoplay', tcm_stateAutoPlay);
  tcm_fireAutoPlaySet(tcm_stateAutoPlay ? 'true' : 'false');
};

// ── BUILD ACTORS

function tcm_buildActors() {
  const container = document.getElementById('actors-body');
  container.innerHTML = '';
  tcm_actorsData.forEach(actor => { container.appendChild(tcm_makeActorCard(actor)); });
};

function tcm_makeActorCard(actor) {
  const id = String(actor.id);

  const card = document.createElement('div');
  card.className = 'tcm-actor-card';
  card.id = 'actor-card-' + id;

  const header = document.createElement('div');
  header.className = 'tcm-actor-header slp-section-header--clickable';
  header.addEventListener('click', () => { tcm_toggleActorCard(id); });

  const nameSpan = document.createElement('span');
  nameSpan.className = 'tcm-actor-name';
  nameSpan.textContent = actor.name;

  const badges = document.createElement('span');
  badges.className = 'tcm-actor-badges';
  if (actor.isPlayer) {
    const b = document.createElement('span');
    b.className = 'slp-badge slp-badge--green';
    b.textContent = SLP_STRINGS.OAM_BADGE_PLAYER;
    badges.appendChild(b);
  }

  const arrow = document.createElement('span');
  arrow.className = 'tcm-arrow';
  arrow.id = 'actor-arrow-' + id;
  arrow.textContent = '▼';

  header.appendChild(nameSpan);
  header.appendChild(badges);
  header.appendChild(arrow);
  card.appendChild(header);

  const body = document.createElement('div');
  body.className = 'tcm-actor-body open';
  body.id = 'actor-body-' + id;

  // Permutation row
  const permRow = tcm_makeSimpleRow(SLP_STRINGS.TCM_PERMUTATION);
  const permVal = document.createElement('span');
  permVal.className = 'tcm-perm-val';
  permVal.id = 'perm-val-' + id;
  permVal.textContent = actor.permCurrent + ' / ' + actor.permTotal;
  const permBtn = document.createElement('button');
  permBtn.className = 'slp-btn';
  permBtn.textContent = SLP_STRINGS.TCM_PERMUTATION_NEXT;
  permBtn.addEventListener('click', e => { e.stopPropagation(); tcm_fireNextPermutation(id); });
  const permRight = document.createElement('span');
  permRight.className = 'tcm-row-right';
  permRight.appendChild(permVal);
  permRight.appendChild(permBtn);
  permRow.appendChild(permRight);
  body.appendChild(permRow);

  // Expression row
  if (actor.expressions && actor.expressions.length > 0) {
    const exprRow = tcm_makeSimpleRow(SLP_STRINGS.TCM_EXPRESSION);
    exprRow.appendChild(tcm_makeSelect(actor.expressions, actor.expressionId, newId => {
      tcm_fireSetExpression(id, newId);
    }));
    body.appendChild(exprRow);
  }

  // Voice row
  if (actor.voices && actor.voices.length > 0) {
    const voiceRow = tcm_makeSimpleRow(SLP_STRINGS.TCM_VOICE);
    voiceRow.appendChild(tcm_makeSelect(actor.voices, actor.voiceId, newId => {
      tcm_fireSetVoice(id, newId);
    }));
    body.appendChild(voiceRow);
  }

  body.appendChild(tcm_makeAlphaRow(actor));
  card.appendChild(body);
  return card;
};

// ── HELPERS — ROWS

function tcm_makeSimpleRow(label) {
  const row = document.createElement('div');
  row.className = 'slp-panel-row tcm-actor-row';
  const lbl = document.createElement('span');
  lbl.className = 'tcm-actor-row-label';
  lbl.textContent = label;
  row.appendChild(lbl);
  return row;
};

function tcm_makeAlphaRow(actor) {
  const id           = String(actor.id);
  const currentAlpha = (actor.alpha !== undefined) ? actor.alpha : 100;

  const row = document.createElement('div');
  row.className = 'slp-panel-row tcm-actor-row tcm-alpha-row';

  const lbl = document.createElement('span');
  lbl.className = 'tcm-actor-row-label';
  lbl.textContent = SLP_STRINGS.TCM_ALPHA;

  const sliderWrap = document.createElement('span');
  sliderWrap.className = 'tcm-alpha-wrap';

  const slider = document.createElement('input');
  slider.type = 'range'; slider.min = '0'; slider.max = '100';
  slider.step = '1'; slider.value = String(currentAlpha);
  slider.className = 'tcm-alpha-slider';
  slider.id = 'alpha-slider-' + id;

  const valLabel = document.createElement('span');
  valLabel.className = 'tcm-alpha-val';
  valLabel.id = 'alpha-val-' + id;
  valLabel.textContent = currentAlpha + '%';

  slider.addEventListener('input',       e => { e.stopPropagation(); valLabel.textContent = slider.value + '%'; });
  slider.addEventListener('change',      e => { e.stopPropagation(); tcm_fireSetActorAlpha(id, slider.value); });
  slider.addEventListener('pointerdown', e => { e.stopPropagation(); });
  slider.addEventListener('keydown',     e => { e.stopPropagation(); });
  slider.addEventListener('wheel', e => {
    e.preventDefault(); e.stopPropagation();
    const v = Math.min(100, Math.max(0, parseInt(slider.value) + (e.deltaY < 0 ? 1 : -1)));
    slider.value = String(v);
    valLabel.textContent = v + '%';
    tcm_fireSetActorAlpha(id, v);
  }, { passive: false });

  sliderWrap.appendChild(slider);
  sliderWrap.appendChild(valLabel);
  row.appendChild(lbl);
  row.appendChild(sliderWrap);
  return row;
};

// ── HELPERS — DROPDOWN

function tcm_closeDropdowns() {
  if (tcm_openDd) {
    tcm_openDd.classList.remove('open');
    if (tcm_openDd._list && tcm_openDd._list.parentNode) {
      tcm_openDd._list.parentNode.removeChild(tcm_openDd._list);
    }
    tcm_openDd = null;
  }
};

function tcm_makeSelect(options, selectedId, onChange) {
  let currentId = selectedId;

  const dd = document.createElement('div');
  dd.className = 'tcm-dd';

  const trigger = document.createElement('div');
  trigger.className = 'tcm-dd-trigger';

  const label = document.createElement('span');
  label.className = 'tcm-dd-label';
  const init = options.find(o => o.id === selectedId);
  label.textContent = init ? init.name : (options[0] ? options[0].name : '');

  const arrow = document.createElement('span');
  arrow.className = 'tcm-dd-arrow';
  arrow.textContent = '▼';

  trigger.appendChild(label);
  trigger.appendChild(arrow);

  const list = document.createElement('div');
  list.className = 'tcm-dd-list';

  options.forEach(opt => {
    const item = document.createElement('div');
    item.className = 'tcm-dd-option' + (opt.id === selectedId ? ' selected' : '');
    item.textContent = opt.name;

    item.addEventListener('pointerdown', e => {
      e.stopPropagation();
      if (opt.id === currentId) { tcm_closeDropdowns(); return; }
      const prev = list.querySelector('.selected');
      if (prev) prev.classList.remove('selected');
      item.classList.add('selected');
      currentId = opt.id;
      label.textContent = opt.name;
      tcm_closeDropdowns();
      if (onChange) onChange(opt.id);
    });

    list.appendChild(item);
  });

  trigger.addEventListener('pointerdown', e => {
    e.stopPropagation();
    const isOpen = dd.classList.contains('open');
    tcm_closeDropdowns();
    if (!isOpen) {
      const rect = trigger.getBoundingClientRect();
      list.style.left  = rect.left + 'px';
      list.style.top   = (rect.bottom + 2) + 'px';
      list.style.width = rect.width + 'px';
      document.body.appendChild(list);
      dd.classList.add('open');
      tcm_openDd = dd;
      tcm_openDd._list = list;
      const sel = list.querySelector('.selected');
      if (sel) sel.scrollIntoView({ block: 'nearest' });
    }
  });

  dd.appendChild(trigger);
  return dd;
};

// ── HELPERS — TOGGLE ICON

function tcm_syncToggle(id, state) {
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

// ── DOM LOAD

document.addEventListener('DOMContentLoaded', () => {
  const pullTab        = document.getElementById('tcmPullTab');
  const backdrop       = document.getElementById('tcmBackdrop');
  const threadHeader   = document.getElementById('thread-header');
  const actorsHeader   = document.getElementById('actors-header');
  const randomSceneRow = document.getElementById('randomSceneRow');
  const moveSceneRow   = document.getElementById('moveSceneRow');
  const autoPlayRow    = document.getElementById('autoPlayRow');

  if (pullTab)        pullTab.addEventListener('click', tcm_openPanel);
  if (backdrop)       backdrop.addEventListener('click', tcm_collapseAll);
  if (threadHeader)   threadHeader.addEventListener('click', () => tcm_toggleSection('thread'));
  if (actorsHeader)   actorsHeader.addEventListener('click', () => tcm_toggleSection('actors'));
  if (randomSceneRow) randomSceneRow.addEventListener('click', tcm_fireRandomScene);
  if (moveSceneRow)   moveSceneRow.addEventListener('click', tcm_fireMoveScene);
  if (autoPlayRow)    autoPlayRow.addEventListener('click', tcm_toggleAutoPlay);

  // Outside-click or Esc-Key: close dropdown or panel
  document.addEventListener('pointerdown', e => {
    if (tcm_openDd && !tcm_openDd.contains(e.target)) tcm_closeDropdowns();
    const panel    = document.getElementById('tcmPanel');
    const pullTab  = document.getElementById('tcmPullTab');
    const backdrop = document.getElementById('tcmBackdrop');
    if (panel && panel.classList.contains('open') &&
        !panel.contains(e.target) &&
        !pullTab.contains(e.target) &&
        !backdrop.contains(e.target)) {
      tcm_collapseAll();
    }
  }, { capture: true });
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape') tcm_collapseAll();
  });
});
