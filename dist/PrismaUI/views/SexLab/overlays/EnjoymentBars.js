'use strict';

const EBO_MAX_ACTORS         = 5;
const EBO_FEEDBACK_DURATION  = 600;
const EBO_GREEN_ZONE_DEFAULT = 0.125;
const EBO_GREEN_ZONE_MIN     = 0.02;
const EBO_GREEN_ZONE_CENTER  = 0.5;
const EBO_GAME_ENJ_THRESHOLD = 75;
const EBO_GAME_ENJ_DRAW_MIN  = 80;

const EBO_IDS = Object.freeze({
    OVERLAY : 'overlay_enjBars',
    ROOT    : 'enjRoot',
});

// ── STATE

let ebo_raf                = undefined;
let ebo_drawPending        = false;
let ebo_feedbackTimer      = null;
let ebo_actorsInfo         = []; // Array of { id, name, enj, intr, isTarget, isGameDpt, _idx, _els }
let ebo_GameClickTimeCycle = 0.0;
let ebo_GameNeedlePos      = 0.5;
let ebo_GameNeedleDir      = 1;
let ebo_GameGreenZoneStart = EBO_GREEN_ZONE_CENTER - EBO_GREEN_ZONE_DEFAULT;
let ebo_GameGreenZoneEnd   = EBO_GREEN_ZONE_CENTER + EBO_GREEN_ZONE_DEFAULT;
let ebo_lastT = 0;

// ── CACHED ELEMENTS

let ebo_el = {};

function ebo_cacheElements() {
    ebo_el = {
        overlay: document.getElementById(EBO_IDS.OVERLAY),
        root:    document.getElementById(EBO_IDS.ROOT),
    };
}

// ── C++ INIT AND DESTROY

window.ebo_initOverlay = function(payloadStr) {
    if (ebo_el.overlay) ebo_el.overlay.style.display = '';
    let data;
    try { data = JSON.parse(payloadStr); } catch(e) { return; }

    ebo_actorsInfo = (data.ebo_actorsInfo || []).slice(0, EBO_MAX_ACTORS).map((a, i) => ({
        id: Number(a.id), name: a.name, enj: 0, intr: '', isTarget: false, isGameDpt: false, _idx: i,
        _els: null, // populated by ebo_buildDOM
    }));

    ebo_GameNeedlePos = 0.5;
    ebo_GameNeedleDir = 1;
    ebo_buildDOM();
};

window.ebo_destroyOverlay = function() {
    ebo_stopNeedleLoop();
    clearTimeout(ebo_feedbackTimer);
    ebo_feedbackTimer      = null;
    ebo_drawPending        = false;
    ebo_actorsInfo         = [];
    ebo_GameClickTimeCycle = 0.0;
    ebo_GameNeedlePos      = 0.5;
    ebo_GameNeedleDir      = 1;
    ebo_GameGreenZoneStart = EBO_GREEN_ZONE_CENTER - EBO_GREEN_ZONE_DEFAULT;
    ebo_GameGreenZoneEnd   = EBO_GREEN_ZONE_CENTER + EBO_GREEN_ZONE_DEFAULT;
    ebo_lastT = 0;
    if (ebo_el.root) ebo_el.root.innerHTML = '';
    if (ebo_el.overlay) ebo_el.overlay.style.display = 'none';
};

// ── C++ TO JS

window.ebo_ClearActors = function() {
    ebo_stopNeedleLoop();
    ebo_actorsInfo = [];
    ebo_buildDOM();
};

window.ebo_UpdateSlider = function(payloadStr) {
    // format: id^enj^intr
    const sep1 = payloadStr.indexOf('^');
    const sep2 = payloadStr.indexOf('^', sep1 + 1);
    if (sep1 === -1 || sep2 === -1) return;

    const a = ebo_findActorById(payloadStr.substring(0, sep1));
    if (!a) return;

    a.enj = parseFloat(payloadStr.substring(sep1 + 1, sep2));
    a.intr = ebo_parseIntr(payloadStr.substring(sep2 + 1));

    // Use cached element ref — no getElementById on this hot path
    if (a._els && a._els.intr) a._els.intr.textContent = a.intr;

    if (a.isGameDpt && a.enj < EBO_GAME_ENJ_THRESHOLD) {
        a.isGameDpt = false;
        ebo_updateGameActorDOM(a);
    }

    ebo_scheduleDraw();
};

window.ebo_UpdateHighlightedPartner = function(partnerId) {
    const searchId = Number(partnerId);
    for (const a of ebo_actorsInfo) {
        const wasTarget = a.isTarget;
        a.isTarget = (a.id === searchId);
        if (wasTarget !== a.isTarget && a._els)
            a._els.wrap.classList.toggle('enj-highlight--target', a.isTarget);
    }
};

window.ebo_RaiseEnjAttempt = function(payloadStr) {
    // format: id^nextTC
    const sep = payloadStr.indexOf('^');
    if (sep === -1) return;

    const a = ebo_findActorById(payloadStr.substring(0, sep));
    if (!a) return;

    if (a.enj >= EBO_GAME_ENJ_THRESHOLD && !a.isGameDpt) {
        a.isGameDpt = true;
        ebo_updateGameActorDOM(a);
        ebo_startNeedleLoop();
    }

    const hit = ebo_GameNeedlePos >= ebo_GameGreenZoneStart &&
                ebo_GameNeedlePos <= ebo_GameGreenZoneEnd;
    ebo_showGameFeedback(a, hit ? 'hit' : 'miss');
    ebo_reportBackResult(hit);
    ebo_GameClickTimeCycle = parseFloat(payloadStr.substring(sep + 1));
};

// ── JS TO C++

function ebo_reportBackResult(isHit) {
    const cb = isHit ? window.ebo_OnTimedAttempt : window.ebo_OnMissedAttempt;
    if (typeof cb === 'function') cb('');
}

function ebo_fireSelectPartner(actorId) {
    if (typeof window.ebo_OnSelectPartner === 'function')
        window.ebo_OnSelectPartner(actorId);
}

// ── HELPERS

function ebo_findActorById(id) {
    const searchId = Number(id);
    for (const a of ebo_actorsInfo) {
        if (a.id === searchId) return a;
    }
    return null;
}

function ebo_barFillPct(enj) {
    if (enj < 0) return Math.min(Math.abs(enj) / 100, 1);
    const m = enj % 100;
    return (m === 0 && enj > 0) ? 1 : Math.min(m / 100, 1);
}

function ebo_barColorClass(enj) {
    if (enj < 0)   return 'enj-fill--neg';
    if (enj > 100) return 'enj-fill--over';
    return 'enj-fill--normal';
}

function ebo_parseIntr(raw) {
    if (!raw) return '';
    return raw.split(',').map(s => s.trim()).filter(Boolean).join(' · ');
}

function ebo_elc(tag, cls) {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    return e;
}

// ── DOM BUILD

function ebo_buildDOM() {
    if (!ebo_el.root) return;
    ebo_el.root.innerHTML = '';

    for (const a of ebo_actorsInfo) {
        const wrap = ebo_elc('div', 'enj-bar-wrap');
        wrap.style.cursor = 'pointer';
        wrap.addEventListener('click', () => {
            if (!a.isTarget) ebo_fireSelectPartner(String(a.id));
        });
        if (a.isTarget) wrap.classList.add('enj-highlight--target');

        const row = ebo_elc('div', 'enj-label-row');
        const nm = ebo_elc('span', 'enj-name');  nm.textContent = a.name;
        const intr = ebo_elc('span', 'enj-intr');  intr.textContent = a.intr;
        const value = ebo_elc('span', 'enj-value');
        row.appendChild(nm);
        row.appendChild(intr);
        row.appendChild(value);

        const frame = ebo_elc('div', 'enj-frame');
        const fill  = ebo_elc('div', 'enj-fill');
        frame.appendChild(fill);

        wrap.appendChild(row);
        wrap.appendChild(frame);
        ebo_el.root.appendChild(wrap);

        // Cache all per-actor element refs — no getElementById needed at runtime
        a._els = { wrap, intr, value, frame, fill, zone: null, needle: null, flash: null, feedback: null };
    }
}

function ebo_updateGameActorDOM(a) {
    if (!a._els) return;
    const { frame } = a._els;

    if (a.isGameDpt && !a._els.zone) {
        const zone = ebo_elc('div', 'enj-zone');
        const center = ebo_elc('div', 'enj-zone-center');
        const needle = ebo_elc('div', 'enj-needle');
        const flash = ebo_elc('div', 'enj-flash');
        const feedback = ebo_elc('div', 'enj-feedback');
        zone.appendChild(center);
        frame.appendChild(zone);
        frame.appendChild(needle);
        frame.appendChild(flash);
        frame.appendChild(feedback);
        a._els.zone = zone;
        a._els.center = center;
        a._els.needle = needle;
        a._els.flash = flash;
        a._els.feedback = feedback;

    } else if (!a.isGameDpt && a._els.zone) {
        for (const key of ['zone', 'needle', 'flash', 'feedback']) {
            a._els[key]?.remove();
            a._els[key] = null;
        }
        a._els.center = null;
    }
}

// ── BAR DRAW

function ebo_scheduleDraw() {
    if (ebo_drawPending) return;
    ebo_drawPending = true;
    requestAnimationFrame(() => {
        ebo_drawPending = false;
        ebo_drawBars();
    });
}

function ebo_drawBars() {
    for (const a of ebo_actorsInfo) {
        const { value, fill } = a._els;
        if (!value || !fill) continue;

        const enj = a.enj;
        value.textContent = Math.round(enj);
        value.className = 'enj-value' +
            (enj > 100 ? ' enj-value--over' : enj < 0 ? ' enj-value--neg' : '');

        fill.className = 'enj-fill ' + ebo_barColorClass(enj) + (enj < 0 ? ' enj-fill--rtl' : '');
        fill.style.width = (ebo_barFillPct(enj) * 100) + '%';
        fill.classList.toggle('enj-fill--pulse', enj >= 90);
    }
}

// ── NEEDLE LOOP

function ebo_startNeedleLoop() {
    if (ebo_raf !== undefined) return;
    ebo_lastT = performance.now();
    ebo_needleLoop();
}

function ebo_stopNeedleLoop() {
    cancelAnimationFrame(ebo_raf);
    ebo_raf = undefined;
}

function ebo_needleLoop() {
    ebo_raf = requestAnimationFrame(t => {
        const dt = Math.min((t - ebo_lastT) / 1000, 0.1);
        ebo_lastT = t;

        if (ebo_GameClickTimeCycle > 0) {
            ebo_GameNeedlePos += ebo_GameNeedleDir * (1 / ebo_GameClickTimeCycle) * dt;
            if (ebo_GameNeedlePos >= 1) { ebo_GameNeedlePos = 1; ebo_GameNeedleDir = -1; }
            if (ebo_GameNeedlePos <= 0) { ebo_GameNeedlePos = 0; ebo_GameNeedleDir =  1; }
        }

        ebo_drawNeedles();

        if (ebo_actorsInfo.some(a => a.isGameDpt && a.enj >= EBO_GAME_ENJ_DRAW_MIN)) {
            ebo_needleLoop();
        } else {
            ebo_stopNeedleLoop();
        }
    });
}

function ebo_drawNeedles() {
    for (const a of ebo_actorsInfo) {
        if (!a.isGameDpt || a.enj < EBO_GAME_ENJ_DRAW_MIN || !a._els.zone) continue;

        const { zone, center, needle } = a._els;

        const doff = Math.max(EBO_GREEN_ZONE_MIN, EBO_GREEN_ZONE_DEFAULT - ((a.enj - EBO_GAME_ENJ_DRAW_MIN) * 0.00375));
        ebo_GameGreenZoneStart = EBO_GREEN_ZONE_CENTER - doff;
        ebo_GameGreenZoneEnd = EBO_GREEN_ZONE_CENTER + doff;

        zone.style.left = (ebo_GameGreenZoneStart * 100) + '%';
        zone.style.width = ((ebo_GameGreenZoneEnd - ebo_GameGreenZoneStart) * 100) + '%';

        const inZone = ebo_GameNeedlePos >= ebo_GameGreenZoneStart &&
                       ebo_GameNeedlePos <= ebo_GameGreenZoneEnd;
        zone.className = 'enj-zone' + (inZone ? ' enj-zone--active' : '');
        center.className = 'enj-zone-center' + (inZone ? ' enj-zone-center--active' : '');
        needle.style.left = (ebo_GameNeedlePos * 100) + '%';
        needle.className = 'enj-needle' + (inZone ? ' enj-needle--active' : '');
    }
}

// ── FEEDBACK

function ebo_showGameFeedback(a, result) {
    if (!a._els) return;
    const { flash, feedback } = a._els;
    if (!flash || !feedback) return;

    const isHit = result === 'hit';
    flash.className = 'enj-flash ' + (isHit ? 'enj-flash--hit' : 'enj-flash--miss');
    flash.classList.add('enj-flash--active');
    feedback.textContent = isHit ? 'HIT' : 'MISS';
    feedback.style.color = isHit ? 'var(--enj-col-hit)' : 'var(--enj-col-miss)';
    feedback.classList.add('enj-feedback--visible');

    clearTimeout(ebo_feedbackTimer);
    ebo_feedbackTimer = setTimeout(() => {
        flash.classList.remove('enj-flash--active');
        feedback.classList.remove('enj-feedback--visible');
    }, EBO_FEEDBACK_DURATION);
}

// ── DOM LOAD

document.addEventListener('DOMContentLoaded', () => {
    ebo_cacheElements();
});
