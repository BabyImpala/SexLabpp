'use strict';

// ── CONSTANTS

const TCM_ICON_ON  = 'overlays/icons/slp_toggle_on.png';
const TCM_ICON_OFF = 'overlays/icons/slp_toggle_off.png';

const TCM_CSS = Object.freeze({
    OPEN:            'open',
    ACTOR_CARD:      'tcm-actor-card',
    ACTOR_HEADER:    'tcm-actor-header slp-section-header--clickable',
    ACTOR_NAME:      'tcm-actor-name',
    ACTOR_BADGES:    'tcm-actor-badges',
    ACTOR_BODY:      'slp-section-body tcm-actor-body open',
    ACTOR_ARROW:     'tcm-arrow',
    BADGE_GREEN:     'slp-badge slp-badge--green',
    PERM_VAL:        'tcm-perm-val',
    PANEL_ROW:       'slp-panel-row tcm-actor-row',
    ROW_LABEL:       'tcm-actor-row-label',
    ALPHA_ROW:       'slp-panel-row tcm-actor-row tcm-alpha-row',
    ALPHA_WRAP:      'tcm-alpha-wrap',
    ALPHA_SLIDER:    'tcm-alpha-slider',
    ALPHA_VAL:       'tcm-alpha-val',
    DD:              'tcm-dd',
    DD_TRIGGER:      'tcm-dd-trigger',
    DD_LABEL:        'tcm-dd-label',
    DD_ARROW:        'tcm-dd-arrow',
    DD_LIST:         'tcm-dd-list',
    DD_OPTION:       'tcm-dd-option',
    DD_SELECTED:     'selected',
    ROW_CLICKABLE:   'slp-panel-row--clickable', // Added back for hover styling
});

const TCM_IDS = Object.freeze({
    OVERLAY:      'overlay_threadConfig',
    PANEL:        'tcmPanel',
    BACKDROP:     'tcmBackdrop',
    PULL_TAB:     'tcmPullTab',
    THREAD_HDR:   'thread-header',
    ACTORS_HDR:   'actors-header',
    ACTORS_BODY:  'actors-body',
    RANDOM_SCENE: 'randomSceneRow',
    MOVE_SCENE:   'moveSceneRow',
    AUTOPLAY_ROW: 'autoPlayRow',
    ICON_AUTOPLAY:'icon-autoplay',
});

// ── STATE

let tcm_stateAutoPlay = false;
let tcm_panelOpen     = false;
let tcm_actorsData    = [];
let tcm_sectionOpen   = { thread: true, actors: true };
let tcm_openDd        = null;
let tcm_alphaRafMap   = {}; // actorId → pending RAF id
const tcm_sectionRefs = {}; // key → { body, arrow }

// ── CACHES  (keyed by actor id string)

const tcm_cardMap = new Map(); // id → { card, body, arrow, alphaSlider, alphaValEl, permValEl }

// ── CACHED ELEMENTS

let tcm_el = {};

function tcm_cacheElements() {
    tcm_el = {
        overlay:        document.getElementById(TCM_IDS.OVERLAY),
        panel:          document.getElementById(TCM_IDS.PANEL),
        backdrop:       document.getElementById(TCM_IDS.BACKDROP),
        pullTab:        document.getElementById(TCM_IDS.PULL_TAB),
        threadHeader:   document.getElementById(TCM_IDS.THREAD_HDR),
        actorsHeader:   document.getElementById(TCM_IDS.ACTORS_HDR),
        actorsBody:     document.getElementById(TCM_IDS.ACTORS_BODY),
        randomSceneRow: document.getElementById(TCM_IDS.RANDOM_SCENE),
        moveSceneRow:   document.getElementById(TCM_IDS.MOVE_SCENE),
        autoPlayRow:    document.getElementById(TCM_IDS.AUTOPLAY_ROW),
        iconAutoplay:   document.getElementById(TCM_IDS.ICON_AUTOPLAY),
    };
}

// ── C++ INIT AND DESTROY

window.tcm_initOverlay = function(jsonStr) {
    if (tcm_el.overlay) tcm_el.overlay.style.display = '';
    let data;
    try { data = JSON.parse(jsonStr); } catch(e) { return; }

    tcm_stateAutoPlay = !!data.autoPlay;
    tcm_syncToggle(tcm_el.iconAutoplay, tcm_stateAutoPlay);

    tcm_actorsData = data.actors || [];
    tcm_buildActors();
};

window.tcm_destroyOverlay = function() {
    tcm_closeDropdowns();
    tcm_stateAutoPlay = false;
    tcm_panelOpen     = false;
    tcm_actorsData    = [];
    tcm_sectionOpen   = { thread: true, actors: true };
    tcm_alphaRafMap   = {};
    tcm_cardMap.clear();
    tcm_closePanel();
    if (tcm_el.actorsBody) tcm_el.actorsBody.innerHTML = '';
    tcm_syncToggle(tcm_el.iconAutoplay, false);
    if (tcm_el.overlay) tcm_el.overlay.style.display = 'none';
};

// ── C++ TO JS

// payload format: formId^cur^total
window.tcm_setPermutation = function(payload) {
    const p = payload.split('^');
    if (p.length < 3) return;
    
    const refs = tcm_cardMap.get(p[0]);
    if (refs && refs.permValEl) {
        refs.permValEl.textContent = p[1] + ' / ' + p[2];
    }
};

// ── JS TO C++

function tcm_fireRandomScene() {
    if (typeof window.tcm_OnRandomScene === 'function')
        window.tcm_OnRandomScene('');
}

function tcm_fireMoveScene() {
    if (typeof window.tcm_OnMoveScene === 'function')
        window.tcm_OnMoveScene('');
}

function tcm_fireAutoPlaySet(val) {
    if (typeof window.tcm_OnAutoPlaySet === 'function')
        window.tcm_OnAutoPlaySet(val);
}

function tcm_fireNextPermutation(actorId) {
    if (typeof window.tcm_OnNextPermutation === 'function')
        window.tcm_OnNextPermutation(actorId);
}

function tcm_fireSetExpression(actorId, exprId) {
    if (typeof window.tcm_OnSetExpression === 'function')
        window.tcm_OnSetExpression(actorId + '|' + exprId);
}

function tcm_fireSetVoice(actorId, voiceId) {
    if (typeof window.tcm_OnSetVoice === 'function')
        window.tcm_OnSetVoice(actorId + '|' + voiceId);
}

function tcm_fireSetActorAlpha(actorId, val) {
    if (typeof window.tcm_OnSetActorAlpha === 'function')
        window.tcm_OnSetActorAlpha(actorId + '^' + val);
}

// ── PANEL VISIBILITY

function tcm_openPanel() {
    slp_collapseAllPanels();
    tcm_el.backdrop.classList.add(TCM_CSS.OPEN);
    tcm_el.panel.classList.add(TCM_CSS.OPEN);
    tcm_panelOpen = true;
}

function tcm_closePanel() {
    tcm_el.panel.classList.remove(TCM_CSS.OPEN);
    tcm_el.backdrop.classList.remove(TCM_CSS.OPEN);
    tcm_panelOpen = false;
}

function tcm_collapseAll() {
    tcm_closePanel();
}

// ── SECTION TOGGLES

function tcm_toggleSection(key) {
    tcm_sectionOpen[key] = !tcm_sectionOpen[key];
    const refs = tcm_sectionRefs[key];
    if (!refs) return;
    refs.body.classList.toggle(TCM_CSS.OPEN, tcm_sectionOpen[key]);
    refs.arrow.textContent = tcm_sectionOpen[key] ? '▼' : '▲';
}

function tcm_toggleActorCard(id) {
    const refs = tcm_cardMap.get(id);
    if (!refs) return;
    const open = refs.body.classList.toggle(TCM_CSS.OPEN);
    refs.arrow.textContent = open ? '▼' : '▲';
}

// ── AUTOPLAY

function tcm_toggleAutoPlay() {
    tcm_stateAutoPlay = !tcm_stateAutoPlay;
    tcm_syncToggle(tcm_el.iconAutoplay, tcm_stateAutoPlay);
    tcm_fireAutoPlaySet(tcm_stateAutoPlay ? 'true' : 'false');
}

// ── BUILD ACTORS

function tcm_buildActors() {
    if (!tcm_el.actorsBody) return;

    const seen = new Set();

    for (const actor of tcm_actorsData) {
        const id = String(actor.id);
        seen.add(id);

        if (tcm_cardMap.has(id)) {
            tcm_syncActorCard(id, actor);
        } else {
            const card = tcm_makeActorCard(actor);
            tcm_el.actorsBody.appendChild(card);
        }
    }

    for (const [id, refs] of tcm_cardMap) {
        if (!seen.has(id)) {
            refs.card.remove();
            tcm_cardMap.delete(id);
        }
    }
}

function tcm_syncActorCard(id, actor) {
    const refs = tcm_cardMap.get(id);
    if (!refs) return;

    tcm_el.actorsBody.appendChild(refs.card);

    if (refs.permValEl) {
        refs.permValEl.textContent = actor.permCurrent + ' / ' + actor.permTotal;
    }

    if (refs.alphaSlider && actor.alpha !== undefined) {
        refs.alphaSlider.value       = String(actor.alpha);
        refs.alphaValEl.textContent  = actor.alpha + '%';
    }
}

function tcm_makeActorCard(actor) {
    const id = String(actor.id);

    const card = document.createElement('div');
    card.className = TCM_CSS.ACTOR_CARD;

    // ── Header
    const header = document.createElement('div');
    header.className = TCM_CSS.ACTOR_HEADER;
    header.addEventListener('click', () => tcm_toggleActorCard(id));

    const nameSpan = document.createElement('span');
    nameSpan.className   = TCM_CSS.ACTOR_NAME;
    nameSpan.textContent = actor.name;

    const badges = document.createElement('span');
    badges.className = TCM_CSS.ACTOR_BADGES;
    if (actor.isPlayer) {
        const b = document.createElement('span');
        b.className   = TCM_CSS.BADGE_GREEN;
        b.textContent = SLP_STRINGS.OAM_BADGE_PLAYER;
        badges.appendChild(b);
    }

    const arrow = document.createElement('span');
    arrow.className   = TCM_CSS.ACTOR_ARROW;
    arrow.textContent = '▼';

    header.appendChild(nameSpan);
    header.appendChild(badges);
    header.appendChild(arrow);
    card.appendChild(header);

    // ── Body
    const body = document.createElement('div');
    body.className = TCM_CSS.ACTOR_BODY;

    const permRow = tcm_makeSimpleRow(SLP_STRINGS.TCM_PERMUTATION);
    permRow.classList.add(TCM_CSS.ROW_CLICKABLE);
    permRow.addEventListener('click', e => { e.stopPropagation(); tcm_fireNextPermutation(id); });

    const permValEl = document.createElement('span');
    permValEl.className = TCM_CSS.PERM_VAL;
    permValEl.textContent = actor.permCurrent + ' / ' + actor.permTotal;
    permRow.appendChild(permValEl);
    body.appendChild(permRow);

    if (actor.expressions && actor.expressions.length > 0) {
        const exprRow = tcm_makeSimpleRow(SLP_STRINGS.TCM_EXPRESSION);
        exprRow.appendChild(tcm_makeSelect(actor.expressions, actor.expressionId, newId => tcm_fireSetExpression(id, newId)));
        body.appendChild(exprRow);
    }

    if (actor.voices && actor.voices.length > 0) {
        const voiceRow = tcm_makeSimpleRow(SLP_STRINGS.TCM_VOICE);
        voiceRow.appendChild(tcm_makeSelect(actor.voices, actor.voiceId, newId => tcm_fireSetVoice(id, newId)));
        body.appendChild(voiceRow);
    }

    const { row: alphaRow, slider: alphaSlider, valEl: alphaValEl } = tcm_makeAlphaRow(actor);
    body.appendChild(alphaRow);
    card.appendChild(body);

    // Store all refs needed for syncing or mutations later
    tcm_cardMap.set(id, { card, body, arrow, alphaSlider, alphaValEl, permValEl });

    return card;
}

// ── HELPERS — ROWS

function tcm_makeSimpleRow(label) {
    const row = document.createElement('div');
    row.className = TCM_CSS.PANEL_ROW;
    const lbl = document.createElement('span');
    lbl.className   = TCM_CSS.ROW_LABEL;
    lbl.textContent = label;
    row.appendChild(lbl);
    return row;
}

function tcm_makeAlphaRow(actor) {
    const id           = String(actor.id);
    const currentAlpha = actor.alpha !== undefined ? actor.alpha : 100;

    const row = document.createElement('div');
    row.className = TCM_CSS.ALPHA_ROW;

    const lbl = document.createElement('span');
    lbl.className   = TCM_CSS.ROW_LABEL;
    lbl.textContent = SLP_STRINGS.TCM_ALPHA;

    const sliderWrap = document.createElement('span');
    sliderWrap.className = TCM_CSS.ALPHA_WRAP;

    const slider = document.createElement('input');
    slider.type      = 'range';
    slider.min       = '0';
    slider.max       = '100';
    slider.step      = '1';
    slider.value     = String(currentAlpha);
    slider.className = TCM_CSS.ALPHA_SLIDER;

    const valEl = document.createElement('span');
    valEl.className   = TCM_CSS.ALPHA_VAL;
    valEl.textContent = currentAlpha + '%';

    slider.addEventListener('input',       e => { e.stopPropagation(); valEl.textContent = slider.value + '%'; });
    slider.addEventListener('change',      e => { e.stopPropagation(); tcm_fireSetActorAlpha(id, slider.value); });
    slider.addEventListener('pointerdown', e => e.stopPropagation());
    slider.addEventListener('keydown',     e => e.stopPropagation());

    slider.addEventListener('wheel', e => {
        e.preventDefault();
        e.stopPropagation();
        const v = Math.min(100, Math.max(0, parseInt(slider.value) + (e.deltaY < 0 ? 1 : -1)));
        slider.value = String(v);
        valEl.textContent = v + '%';
        if (!tcm_alphaRafMap[id]) {
            tcm_alphaRafMap[id] = requestAnimationFrame(() => {
                tcm_alphaRafMap[id] = 0;
                tcm_fireSetActorAlpha(id, slider.value);
            });
        }
    }, { passive: false });

    sliderWrap.appendChild(slider);
    sliderWrap.appendChild(valEl);
    row.appendChild(lbl);
    row.appendChild(sliderWrap);

    return { row, slider, valEl };
}

// ── HELPERS — DROPDOWN

function tcm_closeDropdowns() {
    if (!tcm_openDd) return;
    tcm_openDd.classList.remove(TCM_CSS.OPEN);
    if (tcm_openDd._list && tcm_openDd._list.parentNode)
        tcm_openDd._list.parentNode.removeChild(tcm_openDd._list);
    tcm_openDd = null;
}

function tcm_makeSelect(options, selectedId, onChange) {
    const optMap = new Map(options.map(o => [o.id, o]));
    let currentId      = selectedId;
    let selectedItemEl = null;

    const dd = document.createElement('div');
    dd.className = TCM_CSS.DD;

    const trigger = document.createElement('div');
    trigger.className = TCM_CSS.DD_TRIGGER;

    const labelEl = document.createElement('span');
    labelEl.className = TCM_CSS.DD_LABEL;
    const initOpt = optMap.get(selectedId);
    labelEl.textContent = initOpt ? initOpt.name : (options[0] ? options[0].name : '');

    const arrowEl = document.createElement('span');
    arrowEl.className = TCM_CSS.DD_ARROW;
    arrowEl.textContent = '▼';

    trigger.appendChild(labelEl);
    trigger.appendChild(arrowEl);

    const list = document.createElement('div');
    list.className = TCM_CSS.DD_LIST;

    for (const opt of options) {
        const item = document.createElement('div');
        item.className = TCM_CSS.DD_OPTION + (opt.id === selectedId ? ' ' + TCM_CSS.DD_SELECTED : '');
        item.textContent = opt.name;
        item.dataset.optId = opt.id;
        if (opt.id === selectedId) selectedItemEl = item;
        list.appendChild(item);
    }

    list.addEventListener('pointerdown', e => {
        e.stopPropagation();
        const item = e.target.closest('[data-opt-id]');
        if (!item) return;
        const optId = item.dataset.optId;
        if (optId === currentId) { tcm_closeDropdowns(); return; }
        if (selectedItemEl) selectedItemEl.classList.remove(TCM_CSS.DD_SELECTED);
        item.classList.add(TCM_CSS.DD_SELECTED);
        selectedItemEl = item;
        currentId = optId;
        labelEl.textContent = optMap.get(optId)?.name ?? '';
        tcm_closeDropdowns();
        if (onChange) onChange(optId);
    });

    trigger.addEventListener('pointerdown', e => {
        e.stopPropagation();
        const isOpen = dd.classList.contains(TCM_CSS.OPEN);
        tcm_closeDropdowns();
        if (!isOpen) {
            const rect = trigger.getBoundingClientRect();
            list.style.left = rect.left   + 'px';
            list.style.top = (rect.bottom + 2) + 'px';
            list.style.width = rect.width  + 'px';
            document.body.appendChild(list);
            dd.classList.add(TCM_CSS.OPEN);
            tcm_openDd = dd;
            tcm_openDd._list = list;
            if (selectedItemEl) selectedItemEl.scrollIntoView({ block: 'nearest' });
        }
    });

    dd.appendChild(trigger);
    return dd;
}

// ── HELPERS — TOGGLE ICON

function tcm_syncToggle(el, state) {
    if (!el) return;
    if (el.tagName === 'IMG') {
        el.src = state ? TCM_ICON_ON : TCM_ICON_OFF;
        el.alt = state ? '⬤' : '◯';
    } else {
        el.textContent = state ? '⬤' : '◯';
        el.style.color = state ? 'var(--hub-accent,#7a9060)' : 'var(--text-muted)';
    }
}

// ── DOM LOAD

document.addEventListener('DOMContentLoaded', () => {
    tcm_cacheElements();

    for (const key of ['thread', 'actors']) {
        tcm_sectionRefs[key] = {
            body: document.getElementById(key + '-body'),
            arrow: document.getElementById(key + '-arrow'),
        };
    }

    if (tcm_el.backdrop)       tcm_el.backdrop.addEventListener('click', tcm_collapseAll);
    if (tcm_el.threadHeader)   tcm_el.threadHeader.addEventListener('click', () => tcm_toggleSection('thread'));
    if (tcm_el.actorsHeader)   tcm_el.actorsHeader.addEventListener('click', () => tcm_toggleSection('actors'));
    if (tcm_el.randomSceneRow) tcm_el.randomSceneRow.addEventListener('click', tcm_fireRandomScene);
    if (tcm_el.moveSceneRow)   tcm_el.moveSceneRow.addEventListener('click', tcm_fireMoveScene);
    if (tcm_el.autoPlayRow)    tcm_el.autoPlayRow.addEventListener('click', tcm_toggleAutoPlay);

    if (tcm_el.pullTab) { tcm_el.pullTab.addEventListener('click', (e) => { e.stopPropagation();
        if (tcm_panelOpen) { tcm_closePanel(); } else { tcm_openPanel(); } }); } 

    document.addEventListener('pointerdown', e => {
        if (tcm_openDd && !tcm_openDd.contains(e.target))
            tcm_closeDropdowns();
        const clickedOnDropdownList = e.target.closest('.' + TCM_CSS.DD_LIST);
        if (tcm_el.panel && tcm_el.panel.classList.contains(TCM_CSS.OPEN) &&
            !tcm_el.panel.contains(e.target)   &&
            !tcm_el.pullTab.contains(e.target) &&
            !tcm_el.backdrop.contains(e.target) &&
            !clickedOnDropdownList) {
            tcm_collapseAll();
        }
    }, { capture: true });

    document.addEventListener('keydown', e => {
        if (e.key === 'Escape') tcm_collapseAll();
    });
});