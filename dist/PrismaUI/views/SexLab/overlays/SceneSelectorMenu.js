'use strict';

const SSM_CARD_MARGIN_TOP  = 4;
const SSM_CARD_MARGIN_BOT  = 8;
const SSM_CARD_OFFSET_LEFT = 6;

const SSM_ACTIVE_SCENE_PREFIX = '>>> ';
const SSM_FALLBACK_VALUE      = '—';
const SSM_SEPARATOR           = '|';
const SSM_SEARCH_FOCUS_DELAY  = 60;

const SSM_CSS = Object.freeze({
    OPEN:             'open',
    VISIBLE:          'visible',
    SCENE_ROW:        'ssm-scene-row',
    SCENE_ROW_ACTIVE: 'ssm-scene-row--active',
    ROW_BASE:         'slp-panel-row slp-panel-row--clickable ssm-scene-row',
    LABEL:            'slp-panel-row-name ssm-scene-name',
    INFO_KEY:         'ssm-info-key',
    INFO_VAL:         'ssm-info-val',
    INFO_VAL_TAGS:    'ssm-info-val ssm-info-tag-text',
    SECTION_HEADER:   '.slp-section-header',
});

const SSM_IDS = Object.freeze({
    OVERLAY:           'overlay_sceneSelector',
    PANEL:             'ssmPanel',
    BACKDROP:          'ssmBackdrop',
    PULL_TAB:          'ssmPullTab',
    SCENE_LIST:        'ssmSceneList',
    SCENE_LIST_BODY:   'sceneList-body',
    SCENE_LIST_ARROW:  'sceneList-arrow',
    SEARCH_BODY:       'search-body',
    SEARCH_ARROW:      'search-arrow',
    SEARCH_INPUT:      'ssmSearchInput',
    SEARCH_CANCEL:     'searchCancelBtn',
    SEARCH_CONFIRM:    'searchConfirmBtn',
    INFO_CARD:         'ssmInfoCard',
    INFO_PACKAGE:      'infoPackage',
    INFO_AUTHOR:       'infoAuthor',
    INFO_TAGS:         'infoTags',
    INFO_ANNOTATIONS:  'infoAnnotations',
    SCENE_LIST_HDR:    'sceneListHeader',
    SEARCH_HDR:        'searchHeader'
});

const SSM_ESCAPE_MAP = Object.freeze({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' });
const SSM_ESCAPE_RE  = /[&<>"]/g;

// ── STATE
let ssm_scenesData    = [];
let ssm_panelOpen     = false;
let ssm_sceneListOpen = true;
let ssm_searchOpen    = true;
let ssm_infoCardScene = null;
let ssm_panelLeft     = null;
let ssm_rowMap        = new Map(); // sceneId → { row, label }

// ── CACHED ELEMENTS
let ssm_el = {};

function ssm_cacheElements() {
    ssm_el = {
        overlay:          document.getElementById(SSM_IDS.OVERLAY),
        panel:            document.getElementById(SSM_IDS.PANEL),
        backdrop:         document.getElementById(SSM_IDS.BACKDROP),
        pullTab:          document.getElementById(SSM_IDS.PULL_TAB),
        sceneList:        document.getElementById(SSM_IDS.SCENE_LIST),
        sceneListBody:    document.getElementById(SSM_IDS.SCENE_LIST_BODY),
        sceneListArrow:   document.getElementById(SSM_IDS.SCENE_LIST_ARROW),
        searchBody:       document.getElementById(SSM_IDS.SEARCH_BODY),
        searchArrow:      document.getElementById(SSM_IDS.SEARCH_ARROW),
        searchInput:      document.getElementById(SSM_IDS.SEARCH_INPUT),
        searchCancelBtn:  document.getElementById(SSM_IDS.SEARCH_CANCEL),
        searchConfirmBtn: document.getElementById(SSM_IDS.SEARCH_CONFIRM),
        infoCard:         document.getElementById(SSM_IDS.INFO_CARD),
        infoPackage:      document.getElementById(SSM_IDS.INFO_PACKAGE),
        infoAuthor:       document.getElementById(SSM_IDS.INFO_AUTHOR),
        infoTags:         document.getElementById(SSM_IDS.INFO_TAGS),
        infoAnnotations:  document.getElementById(SSM_IDS.INFO_ANNOTATIONS),
        sceneListHeader:  document.getElementById(SSM_IDS.SCENE_LIST_HDR),
        searchHeader:     document.getElementById(SSM_IDS.SEARCH_HDR)
    };
}

// ── C++ INIT AND DESTROY
window.ssm_initOverlay = function(jsonStr) {
    if (ssm_el.overlay) ssm_el.overlay.style.display = '';
    ssm_populateScenes(jsonStr);
};

window.ssm_destroyOverlay = function() {
    ssm_flushAnnotationEdit();
    ssm_scenesData    = [];
    ssm_panelOpen     = false;
    ssm_sceneListOpen = true;
    ssm_searchOpen    = true;
    ssm_infoCardScene = null;
    ssm_panelLeft     = null;
    ssm_rowMap.clear(); 
    ssm_closePanel();
    if (ssm_el.sceneList) ssm_el.sceneList.innerHTML = '';
    if (ssm_el.overlay)   ssm_el.overlay.style.display = 'none';
};

// ── C++ TO JS
window.ssm_populateScenes = function(jsonStr) {
    try { ssm_scenesData = JSON.parse(jsonStr) || []; } catch(e) { return; }
    ssm_renderSceneList();
};

// ── JS TO C++
function ssm_fireSceneSelected(sceneId) {
    if (typeof window.ssm_OnSceneSelected === 'function')
        window.ssm_OnSceneSelected(sceneId);
}

function ssm_fireSceneResetBySearch(query) {
    if (typeof window.ssm_OnSceneResetBySearch === 'function')
        window.ssm_OnSceneResetBySearch(query);
}

function ssm_fireAnnotationEdited(sceneId, value) {
    if (typeof window.ssm_OnAnnotationEdited === 'function')
        window.ssm_OnAnnotationEdited(sceneId + SSM_SEPARATOR + value);
}

// ── PANEL VISIBILITY
function ssm_openPanel() {
    if (typeof slp_collapseAllPanels === 'function') slp_collapseAllPanels();
    if (ssm_el.backdrop) ssm_el.backdrop.classList.add(SSM_CSS.OPEN);
    if (ssm_el.panel)    ssm_el.panel.classList.add(SSM_CSS.OPEN);
    ssm_panelLeft = null;
    ssm_panelOpen = true;
}

function ssm_closePanel() {
    if (ssm_el.panel)       ssm_el.panel.classList.remove(SSM_CSS.OPEN);
    if (ssm_el.backdrop)    ssm_el.backdrop.classList.remove(SSM_CSS.OPEN);
    if (ssm_el.searchInput) ssm_el.searchInput.value = '';
    ssm_hideInfoCard();
    ssm_panelOpen = false;
}

function ssm_collapseAll() {
    ssm_closePanel();
}

// ── SECTION TOGGLES
function ssm_toggleSceneList() {
    ssm_sceneListOpen = !ssm_sceneListOpen;
    if (ssm_el.sceneListBody)  ssm_el.sceneListBody.classList.toggle(SSM_CSS.OPEN, ssm_sceneListOpen);
    if (ssm_el.sceneListArrow) ssm_el.sceneListArrow.textContent = ssm_sceneListOpen ? '▼' : '▲';
}

function ssm_toggleSearch() {
    ssm_searchOpen = !ssm_searchOpen;
    if (ssm_el.searchBody)  ssm_el.searchBody.classList.toggle(SSM_CSS.OPEN, ssm_searchOpen);
    if (ssm_el.searchArrow) ssm_el.searchArrow.textContent = ssm_searchOpen ? '▼' : '▲';
    if (ssm_searchOpen) setTimeout(() => ssm_el.searchInput?.focus(), SSM_SEARCH_FOCUS_DELAY);
}

// ── SEARCH
function ssm_cancelSearch() {
    if (ssm_el.searchInput) ssm_el.searchInput.value = '';
}

function ssm_confirmSearch() {
    if (!ssm_el.searchInput) return;
    const query = ssm_el.searchInput.value.trim();
    ssm_el.searchInput.value = '';
    if (query) {
        ssm_fireSceneResetBySearch(query);
        ssm_closePanel();
    }
}

// ── SCENE LIST
function ssm_createSceneRow(scene) {
    const row   = document.createElement('div');
    const label = document.createElement('span');
    label.className     = SSM_CSS.LABEL;
    row.className       = SSM_CSS.ROW_BASE;
    row.dataset.sceneId = scene.id;
    row.appendChild(label);
    return { row, label };
}

function ssm_syncSceneRow(cached, scene) {
    const { row, label } = cached;
    const isActive = scene.isActive;
    row.className = SSM_CSS.ROW_BASE + (isActive ? ' ' + SSM_CSS.SCENE_ROW_ACTIVE : '');
    label.textContent = isActive ? SSM_ACTIVE_SCENE_PREFIX + scene.name : scene.name;
}

function ssm_renderSceneList() {
    if (!ssm_el.sceneList) return;

    const seen = new Set();

    for (const scene of ssm_scenesData) {
        seen.add(scene.id);
        let cached = ssm_rowMap.get(scene.id);
        if (!cached) {
            cached = ssm_createSceneRow(scene);
            ssm_rowMap.set(scene.id, cached);
        }
        ssm_syncSceneRow(cached, scene);
        ssm_el.sceneList.appendChild(cached.row);
    }

    for (const [id, cached] of ssm_rowMap) {
        if (!seen.has(id)) {
            cached.row.remove();
            ssm_rowMap.delete(id);
        }
    }
}

// ── DELEGATED LIST EVENTS
function ssm_onListMouseOver(e) {
    const row = e.target.closest('.' + SSM_CSS.SCENE_ROW);
    if (!row) return;
    if (e.relatedTarget && row.contains(e.relatedTarget)) return;

    const scene = ssm_scenesData.find(s => s.id === row.dataset.sceneId);
    if (scene) ssm_showInfoCard(scene, row);
}

function ssm_onListMouseOut(e) {
    const row = e.target.closest('.' + SSM_CSS.SCENE_ROW);
    if (!row) return;
    
    const { relatedTarget } = e;
    if (relatedTarget && (row.contains(relatedTarget) || 
        ssm_el.infoCard === relatedTarget || ssm_el.infoCard?.contains(relatedTarget))) return;
        
    ssm_hideInfoCard();
}

function ssm_onListClick(e) {
    const row = e.target.closest('.' + SSM_CSS.SCENE_ROW);
    if (!row) return;
    const scene = ssm_scenesData.find(s => s.id === row.dataset.sceneId);
    if (scene && !scene.isActive) ssm_fireSceneSelected(scene.id);
}

// ── INFO CARD
function ssm_setInfoField(el, keyClass, valClass, labelText, value) {
    if (!el) return;
    if (!el.firstChild) {
        const key = document.createElement('span');
        const val = document.createElement('span');
        key.className = keyClass;
        val.className = valClass;
        el.appendChild(key);
        el.appendChild(val);
    }
    el.firstChild.textContent = labelText;
    el.lastChild.textContent = value || SSM_FALLBACK_VALUE;
}

function ssm_showInfoCard(scene, rowEl) {
    if (!ssm_el.infoCard) return;

    if (ssm_infoCardScene === scene.id && ssm_el.infoCard.classList.contains(SSM_CSS.VISIBLE)) {
        ssm_repositionInfoCard(rowEl);
        return;
    }

    ssm_infoCardScene = scene.id;

    ssm_setInfoField(ssm_el.infoPackage, SSM_CSS.INFO_KEY, SSM_CSS.INFO_VAL,      'Package', scene.package);
    ssm_setInfoField(ssm_el.infoAuthor,  SSM_CSS.INFO_KEY, SSM_CSS.INFO_VAL,      'Author',  scene.author);
    ssm_setInfoField(ssm_el.infoTags,    SSM_CSS.INFO_KEY, SSM_CSS.INFO_VAL_TAGS, 'Tags',    scene.tags);

    if (ssm_el.infoAnnotations) {
        ssm_el.infoAnnotations.textContent      = scene.annotations || '';
        ssm_el.infoAnnotations.dataset.sceneId  = scene.id;
        ssm_el.infoAnnotations.dataset.original = scene.annotations || '';
    }

    ssm_el.infoCard.classList.add(SSM_CSS.VISIBLE);
    ssm_repositionInfoCard(rowEl);
}

function ssm_repositionInfoCard(rowEl) {
    requestAnimationFrame(() => {
        if (!ssm_el.infoCard || !ssm_el.infoCard.classList.contains(SSM_CSS.VISIBLE)) return;

        if (ssm_panelLeft === null && ssm_el.panel)
            ssm_panelLeft = ssm_el.panel.getBoundingClientRect().left;

        const rowRect = rowEl.getBoundingClientRect();
        const cardH   = ssm_el.infoCard.offsetHeight;
        const cardW   = ssm_el.infoCard.offsetWidth;
        const maxTop  = window.innerHeight - SSM_CARD_MARGIN_BOT - cardH;
        const top     = Math.max(SSM_CARD_MARGIN_TOP, Math.min(rowRect.top, maxTop));

        ssm_el.infoCard.style.top  = top + 'px';
        ssm_el.infoCard.style.left = ((ssm_panelLeft || 0) - cardW - SSM_CARD_OFFSET_LEFT) + 'px';
    });
}

function ssm_hideInfoCard() {
    ssm_flushAnnotationEdit();
    ssm_infoCardScene = null;
    if (ssm_el.infoCard) ssm_el.infoCard.classList.remove(SSM_CSS.VISIBLE);
}

// ── ANNOTATIONS
function ssm_flushAnnotationEdit() {
    if (!ssm_infoCardScene || !ssm_el.infoAnnotations) return;
    const el  = ssm_el.infoAnnotations;
    const val = el.textContent.trim();
    if (val === (el.dataset.original || '')) return;
    el.dataset.original = val;
    const entry = ssm_scenesData.find(s => s.id === ssm_infoCardScene);
    if (entry) entry.annotations = val;
    ssm_fireAnnotationEdited(ssm_infoCardScene, val);
}

// ── HELPERS
function ssm_esc(str) {
    return String(str).replace(SSM_ESCAPE_RE, c => SSM_ESCAPE_MAP[c]);
}

// ── DOM LOAD
document.addEventListener('DOMContentLoaded', () => {
    ssm_cacheElements();

    if (ssm_el.backdrop)         ssm_el.backdrop.addEventListener('click', ssm_collapseAll);
    if (ssm_el.sceneListHeader)  ssm_el.sceneListHeader.addEventListener('click', ssm_toggleSceneList);
    if (ssm_el.searchHeader)     ssm_el.searchHeader.addEventListener('click', ssm_toggleSearch);
    if (ssm_el.searchCancelBtn)  ssm_el.searchCancelBtn.addEventListener('click', ssm_closePanel);
    if (ssm_el.searchConfirmBtn) ssm_el.searchConfirmBtn.addEventListener('click', ssm_confirmSearch);

    if (ssm_el.pullTab) { ssm_el.pullTab.addEventListener('click', (e) => { e.stopPropagation();
        if (ssm_panelOpen) { ssm_closePanel(); } else { ssm_openPanel(); } }); }

    if (ssm_el.sceneList) {
        ssm_el.sceneList.addEventListener('mouseover', ssm_onListMouseOver);
        ssm_el.sceneList.addEventListener('mouseout',  ssm_onListMouseOut);
        ssm_el.sceneList.addEventListener('click',     ssm_onListClick);
    }

    if (ssm_el.searchInput) {
        ssm_el.searchInput.addEventListener('keydown', e => {
            e.stopPropagation();
            if (e.key === 'Enter')  ssm_confirmSearch();
            if (e.key === 'Escape') ssm_cancelSearch();
        });
    }

    if (ssm_el.infoAnnotations) {
        const annotEl = ssm_el.infoAnnotations;
        annotEl.addEventListener('keydown', e => {
            e.stopPropagation();
            if (e.key === 'Enter')  { e.preventDefault(); ssm_flushAnnotationEdit(); annotEl.blur(); }
            if (e.key === 'Escape') { annotEl.textContent = annotEl.dataset.original || ''; annotEl.blur(); }
        });
        annotEl.addEventListener('blur',        ssm_flushAnnotationEdit);
        annotEl.addEventListener('pointerdown', e => e.stopPropagation());
    }

    if (ssm_el.infoCard) {
        ssm_el.infoCard.addEventListener('mouseleave', e => {
            if (ssm_el.panel && !ssm_el.panel.contains(e.relatedTarget)) ssm_hideInfoCard();
        });
    }

    document.addEventListener('pointerdown', e => {
        if (!ssm_panelOpen) return;
        const inPanel = ssm_el.panel    && ssm_el.panel.contains(e.target);
        const inCard  = ssm_el.infoCard && ssm_el.infoCard.contains(e.target);
        const inTab   = ssm_el.pullTab  && ssm_el.pullTab.contains(e.target);
        if (!inPanel && !inCard && !inTab) ssm_collapseAll();
    }, { capture: true });

    document.addEventListener('keydown', e => {
        if (document.activeElement &&
            (document.activeElement.tagName === 'INPUT' || document.activeElement.isContentEditable)) return;
        if (e.key === 'Escape') ssm_collapseAll();
    });
});