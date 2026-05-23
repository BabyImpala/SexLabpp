'use strict';

let ssm_scenesData     = [];
let ssm_panelOpen      = false;
let ssm_sceneListOpen  = true;
let ssm_searchOpen     = true;
let ssm_infoCardScene  = null;
const ssm_overlayEl    = document.getElementById('overlay_sceneSelector');

// ── C++ INIT AND DESTROY

window.ssm_initOverlay = function(jsonStr) {
  if (ssm_overlayEl) ssm_overlayEl.style.display = '';
  ssm_populateScenes(jsonStr);
};

window.ssm_destroyOverlay = function() {
  // Flush unsaved annotation edit before teardown
  ssm_flushAnnotationEdit();
  // Reset state
  ssm_scenesData    = [];
  ssm_panelOpen     = false;
  ssm_sceneListOpen = true;
  ssm_searchOpen    = true;
  ssm_infoCardScene = null;
  // Close panel and clear dynamic DOM
  ssm_closePanel();
  const list = document.getElementById('ssmSceneList');
  if (list) list.innerHTML = '';
  if (ssm_overlayEl) ssm_overlayEl.style.display = 'none';
};

// ── C++ TO JS

window.ssm_populateScenes = function(jsonStr) {
  try { ssm_scenesData = JSON.parse(jsonStr) || []; } catch(e) { return; }
  ssm_renderSceneList();
};

// ── JS TO C++

function ssm_fireSceneSelected(sceneId) {
  if (typeof window.ssm_OnSceneSelected === 'function') {
    window.ssm_OnSceneSelected(sceneId);
  }
};

function ssm_fireSceneResetBySearch(query) {
  if (typeof window.ssm_OnSceneResetBySearch === 'function') {
    window.ssm_OnSceneResetBySearch(query);
  }
};

function ssm_fireAnnotationEdited(sceneId, value) {
  if (typeof window.ssm_OnAnnotationEdited === 'function') {
    window.ssm_OnAnnotationEdited(sceneId + '|' + value);
  }
};

// ── VISIBILITY

function ssm_openPanel() {
  slp_collapseAllPanels();
  document.getElementById('ssmBackdrop').classList.add('open');
  document.getElementById('ssmPanel').classList.add('open');
  ssm_panelOpen = true;
};

function ssm_closePanel() {
  document.getElementById('ssmPanel').classList.remove('open');
  document.getElementById('ssmBackdrop').classList.remove('open');
  document.getElementById('ssmSearchInput').value = '';
  ssm_hideInfoCard();
  ssm_panelOpen = false;
};

function ssm_collapseAll() {
  ssm_closePanel();
};

// ── ROW TOGGLES

function ssm_toggleSceneList() {
  ssm_sceneListOpen = !ssm_sceneListOpen;
  document.getElementById('sceneList-body').classList.toggle('open', ssm_sceneListOpen);
  document.getElementById('sceneList-arrow').textContent = ssm_sceneListOpen ? '▼' : '▲';
};

function ssm_toggleSearch() {
  ssm_searchOpen = !ssm_searchOpen;
  document.getElementById('search-body').classList.toggle('open', ssm_searchOpen);
  document.getElementById('search-arrow').textContent = ssm_searchOpen ? '▼' : '▲';
  if (ssm_searchOpen) setTimeout(() => { document.getElementById('ssmSearchInput').focus(); }, 60);
};

// ── SEARCH

function ssm_cancelSearch() {
  document.getElementById('ssmSearchInput').value = '';
};

function ssm_confirmSearch() {
  const val = document.getElementById('ssmSearchInput').value.trim();
  document.getElementById('ssmSearchInput').value = '';
  if (val) {
    ssm_fireSceneResetBySearch(val);
    ssm_closePanel();
  }
};

// ── SCENE LIST

function ssm_renderSceneList() {
  const container = document.getElementById('ssmSceneList');
  container.innerHTML = '';
  ssm_scenesData.forEach(scene => {
    const row = document.createElement('div');
    row.className = 'slp-panel-row slp-panel-row--clickable ssm-scene-row' +
                    (scene.isActive ? ' ssm-scene-row--active' : '');

    const label = document.createElement('span');
    label.className = 'slp-panel-row-name ssm-scene-name';
    label.textContent = scene.isActive ? '>>> ' + scene.name : scene.name;
    row.appendChild(label);

    row.addEventListener('mouseenter', () => { ssm_showInfoCard(scene, row); });
    row.addEventListener('mouseleave', e => {
      const card = document.getElementById('ssmInfoCard');
      if (card.contains(e.relatedTarget) || card === e.relatedTarget) return;
      ssm_hideInfoCard();
    });
    row.addEventListener('click', () => {
      if (!scene.isActive) {
        ssm_fireSceneSelected(scene.id);
      }
    });

    container.appendChild(row);
  });
};

// ── INFO CARD

function ssm_showInfoCard(scene, rowEl) {
  ssm_infoCardScene = scene.id;
  document.getElementById('infoPackage').innerHTML =
    '<span class="ssm-info-key">Package</span><span class="ssm-info-val">' + ssm_esc(scene.package || '—') + '</span>';
  document.getElementById('infoAuthor').innerHTML =
    '<span class="ssm-info-key">Author</span><span class="ssm-info-val">' + ssm_esc(scene.author || '—') + '</span>';
  document.getElementById('infoTags').innerHTML =
    '<span class="ssm-info-key">Tags</span><span class="ssm-info-val ssm-info-tag-text">' + ssm_esc(scene.tags || '—') + '</span>';

  const annotEl = document.getElementById('infoAnnotations');
  annotEl.textContent      = scene.annotations || '';
  annotEl.dataset.sceneId  = scene.id;
  annotEl.dataset.original = scene.annotations || '';

  const card      = document.getElementById('ssmInfoCard');
  const panelRect = document.getElementById('ssmPanel').getBoundingClientRect();
  const rowRect   = rowEl.getBoundingClientRect();
  card.style.display = 'block';
  requestAnimationFrame(() => {
    card.style.top  = Math.max(4, rowRect.top) + 'px';
    card.style.left = (panelRect.left - card.offsetWidth - 6) + 'px';
    const cr = card.getBoundingClientRect();
    if (cr.bottom > window.innerHeight - 8) {
      card.style.top = Math.max(4, window.innerHeight - 8 - cr.height) + 'px';
    }
  });
};

function ssm_hideInfoCard() {
  ssm_flushAnnotationEdit();
  ssm_infoCardScene = null;
  document.getElementById('ssmInfoCard').style.display = 'none';
};

// ── ANNOTATIONS

function ssm_flushAnnotationEdit() {
  if (!ssm_infoCardScene) return;
  const el  = document.getElementById('infoAnnotations');
  const val = el.textContent.trim();
  if (val === (el.dataset.original || '')) return;
  el.dataset.original = val;
  const entry = ssm_scenesData.find(s => s.id === ssm_infoCardScene);
  if (entry) entry.annotations = val;
  ssm_fireAnnotationEdited(ssm_infoCardScene, val);
};

// ── HELPERS

function ssm_esc(str) {
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
};

// ── DOM LOAD

document.addEventListener('DOMContentLoaded', () => {
  const pullTab          = document.getElementById('ssmPullTab');
  const backdrop         = document.getElementById('ssmBackdrop');
  const sceneListHeader  = document.getElementById('sceneListHeader');
  const searchHeader     = document.getElementById('searchHeader');
  const searchCancelBtn  = document.getElementById('searchCancelBtn');
  const searchConfirmBtn = document.getElementById('searchConfirmBtn');
  const searchInput      = document.getElementById('ssmSearchInput');
  const annotEl          = document.getElementById('infoAnnotations');
  const infoCard         = document.getElementById('ssmInfoCard');

  if (pullTab)         pullTab.addEventListener('click', ssm_openPanel);
  if (backdrop)        backdrop.addEventListener('click', ssm_collapseAll);
  if (sceneListHeader) sceneListHeader.addEventListener('click', ssm_toggleSceneList);
  if (searchHeader)    searchHeader.addEventListener('click', ssm_toggleSearch);
  if (searchCancelBtn) searchCancelBtn.addEventListener('click', ssm_closePanel);
  if (searchConfirmBtn) searchConfirmBtn.addEventListener('click', ssm_confirmSearch);

  if (searchInput) {
    searchInput.addEventListener('keydown', e => {
      e.stopPropagation();
      if (e.key === 'Enter')  ssm_confirmSearch();
      if (e.key === 'Escape') ssm_cancelSearch();
    });
  }

  if (annotEl) {
    annotEl.addEventListener('keydown', e => {
      e.stopPropagation();
      if (e.key === 'Enter')  { e.preventDefault(); ssm_flushAnnotationEdit(); annotEl.blur(); }
      if (e.key === 'Escape') { annotEl.textContent = annotEl.dataset.original || ''; annotEl.blur(); }
    });
    annotEl.addEventListener('blur', ssm_flushAnnotationEdit);
    annotEl.addEventListener('pointerdown', e => { e.stopPropagation(); });
  }

  if (infoCard) {
    infoCard.addEventListener('mouseleave', e => {
      if (!document.getElementById('ssmPanel').contains(e.relatedTarget)) ssm_hideInfoCard();
    });
  }

  document.addEventListener('keydown', e => {
    // skip if focus is in an input or contenteditable
    if (document.activeElement && (document.activeElement.tagName === 'INPUT' || document.activeElement.isContentEditable)) return;
    if (e.key === 'Escape') ssm_collapseAll();
  });
});
