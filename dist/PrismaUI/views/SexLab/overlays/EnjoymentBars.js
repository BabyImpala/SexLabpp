'use strict';

let ebo_raf;
let ebo_actorsInfo          = []; /* Array of { id, name, enj, intr, isTarget, isGameDpt } */
let ebo_GamecCickTimeCycle  = 0.0;
let ebo_GameNeedlePos       = 0.5;
let ebo_GameNeedleDir       = 1;
let ebo_GameGreenZoneStart  = 0.375;
let ebo_GameGreenZoneEnd    = 0.625;
let ebo_feedbackTimer       = null;
const ebo_overlayEl         = document.getElementById('overlay_enjBars');

// ── C++ INIT AND DESTROY

window.ebo_initOverlay = function(payloadStr) {
  if (ebo_overlayEl) ebo_overlayEl.style.display = '';
  let data;
  try { data = JSON.parse(payloadStr);} catch(e) { return; }
  ebo_actorsInfo = (data.ebo_actorsInfo || []).slice(0, 5).map((a) => {
    return { id: Number(a.id), name: a.name, enj: 0, intr: '', isTarget: false, isGameDpt: false };
  });
  ebo_GameNeedlePos = 0.5;
  ebo_GameNeedleDir = 1;
  ebo_buildDOM();
  cancelAnimationFrame(ebo_raf);
  ebo_loop();
};

window.ebo_destroyOverlay = function() {
  // Cancel loops that may keep running otherwise
  cancelAnimationFrame(ebo_raf);
  ebo_raf = undefined;
  clearTimeout(ebo_feedbackTimer);
  ebo_feedbackTimer = null;
  ebo_ClearActors()
  // Reset all game/needle state
  ebo_actorsInfo         = [];
  ebo_GamecCickTimeCycle = 0.0;
  ebo_GameNeedlePos      = 0.5;
  ebo_GameNeedleDir      = 1;
  ebo_GameGreenZoneStart = 0.375;
  ebo_GameGreenZoneEnd   = 0.625;
  lastT  = 0;
  pulseT = 0;
  // Clear dynamic DOM
  const stack = document.getElementById('enjRoot');
  if (stack) stack.innerHTML = '';
  if (ebo_overlayEl) ebo_overlayEl.style.display = 'none';
};

// ── C++ TO JS

window.ebo_ClearActors = function() {
  ebo_actorsInfo = [];
  ebo_buildDOM();
  cancelAnimationFrame(ebo_raf);
};

window.ebo_UpdateSlider = function(payloadStr) {
  // payloadStr format: id^enj^intr
  const parts = payloadStr.split('^');
  if (parts.length < 3) return;
  const id = parts[0];
  const enj = parseFloat(parts[1]);
  const intrRaw = parts[2];
  const a = ebo_findActorById(id);
  if (!a) return;
  a.enj = enj;
  a.intr = ebo_parseIntr(intrRaw);
  const intrEl = document.getElementById(`ei${a._idx}`);
  if (intrEl) intrEl.textContent = a.intr;
  if (a.isGameDpt && a.enj < 75) {
    a.isGameDpt = false;
    ebo_updateGameActorDOM(a);
  }
};

window.ebo_UpdateHighlightedPartner = function(partnerId) {
  const searchId = Number(partnerId);
  ebo_actorsInfo.forEach((a, i) => {
    const wasTarget = a.isTarget;
    a.isTarget = (Number(a.id) === searchId);
    if (wasTarget !== a.isTarget) {
      const wrap = document.getElementById(`wrap${i}`);
      if (wrap) wrap.classList.toggle('enj-highlight--target', a.isTarget);
    }
  });
};

window.ebo_RaiseEnjAttempt = function(payloadStr) {
  // payloadStr format: id^nextTC
  const parts = payloadStr.split('^');
  if (parts.length < 2) return;
  const id = parts[0];
  const nextTC = parseFloat(parts[1]);
  const a = ebo_findActorById(id);
  if (!a) return;
  if (a.enj >= 75 && !a.isGameDpt) {
    a.isGameDpt = true;
    ebo_updateGameActorDOM(a);
  }
  const hit = (ebo_GameNeedlePos >= ebo_GameGreenZoneStart && ebo_GameNeedlePos <= ebo_GameGreenZoneEnd);
  if (hit) {
    ebo_showGameFeedback(a._idx, 'hit');
    ebo_reportBackResult(true);
  } else {
    ebo_showGameFeedback(a._idx, 'miss');
    ebo_reportBackResult(false);
  }
  ebo_GamecCickTimeCycle = nextTC;
};

// ── JS TO C++

function ebo_reportBackResult(isHit) {
  const callback = isHit ? window.ebo_OnTimedAttempt : window.ebo_OnMissedAttempt;
  if (callback) {
    callback("");
  }
};

// ── HELPERS

function ebo_findActorById(id) {
  const searchId = Number(id);
  for (let i = 0; i < ebo_actorsInfo.length; i++) {
    if (Number(ebo_actorsInfo[i].id) === searchId) {
      ebo_actorsInfo[i]._idx = i;
      return ebo_actorsInfo[i];
    }
  }
  return null;
};

function ebo_barFillPct(enj) {
  if (enj < 0) return Math.min(Math.abs(enj) / 100, 1);
  const m = enj % 100;
  return (m === 0 && enj > 0) ? 1 : Math.min(m / 100, 1);
};

function ebo_barColorClass(enj) {
  if (enj < 0) return 'enj-fill--neg';
  if (enj > 100) return 'enj-fill--over';
  return 'enj-fill--normal';
};

function ebo_parseIntr(raw) {
  if (!raw || !raw.length) return '';
  return raw.split(',').map(s => s.trim()).filter(Boolean).join(' · ');
};

// ── DOM GEN & BAR ANIMATION CORE

function ebo_buildDOM() {
  const stack = document.getElementById('enjRoot');
  if (!stack) return;
  stack.innerHTML = '';
  ebo_actorsInfo.forEach((a, i) => {
    a._idx = i;

    const wrap = ebo_elc('div', 'enj-bar-wrap'); 
    wrap.id = `wrap${i}`;
    if (a.isTarget) wrap.classList.add('enj-highlight--target');

    const row = ebo_elc('div', 'enj-label-row');
    const nm = ebo_elc('span', 'enj-name'); nm.textContent = a.name;
    const intr = ebo_elc('span', 'enj-intr'); intr.id = `ei${i}`; intr.textContent = a.intr;
    const vl = ebo_elc('span', 'enj-value'); vl.id = `ev${i}`;
    row.appendChild(nm);
    row.appendChild(intr);
    row.appendChild(vl);

    const frame = ebo_elc('div', 'enj-frame'); frame.id = `ef${i}`;
    const fill = ebo_elc('div', 'enj-fill'); fill.id = `eb${i}`;
    frame.appendChild(fill);
    wrap.appendChild(row);
    wrap.appendChild(frame);
    stack.appendChild(wrap);
  });
};

function ebo_updateGameActorDOM(a) {
  const idx = a._idx;
  const frame = document.getElementById(`ef${idx}`);
  if (!frame) return;
  const hasGameDOM = !!document.getElementById(`ez${idx}`);
  if (a.isGameDpt && !hasGameDOM) {
    const zone = ebo_elc('div', 'enj-zone'); zone.id = `ez${idx}`;
    const center = ebo_elc('div', 'enj-zone-center'); center.id = `ec${idx}`;
    zone.appendChild(center);
    frame.appendChild(zone);

    const needle = ebo_elc('div', 'enj-needle'); needle.id = `en${idx}`;
    frame.appendChild(needle);

    const flash = ebo_elc('div', 'enj-flash'); flash.id = `efl${idx}`;
    const fbtext = ebo_elc('div', 'enj-feedback'); fbtext.id = `efb${idx}`;
    
    frame.appendChild(flash);
    frame.appendChild(fbtext);
  } else if (!a.isGameDpt && hasGameDOM) {
    ['ez', 'en', 'efl', 'efb'].forEach((prefix) => {
      const node = document.getElementById(prefix + idx);
      if (node) node.parentNode.removeChild(node);
    });
  }
};

function ebo_elc(tag, cls) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  return e;
};

let lastT = 0, pulseT = 0;
function ebo_loop() {
  ebo_raf = requestAnimationFrame((t) => {
    const dt = Math.min((t - lastT) / 1000, 0.1);
    lastT = t;

    if (ebo_GamecCickTimeCycle > 0 &&
        ebo_actorsInfo.some(a => a.isGameDpt && a.enj >= 80)) {
      ebo_GameNeedlePos += ebo_GameNeedleDir * (1 / ebo_GamecCickTimeCycle) * dt;
      if (ebo_GameNeedlePos >= 1) { ebo_GameNeedlePos = 1; ebo_GameNeedleDir = -1; }
      if (ebo_GameNeedlePos <= 0) { ebo_GameNeedlePos = 0; ebo_GameNeedleDir =  1; }
      pulseT += dt;
    }

    ebo_draw();
    ebo_loop();
  });
};

function ebo_draw() {
  ebo_actorsInfo.forEach((a, i) => {
    const vl = document.getElementById(`ev${i}`);
    const bl = document.getElementById(`eb${i}`);
    if (!vl || !bl) return;

    const enj = a.enj;
    vl.textContent = Math.round(enj);
    vl.className = 'enj-value' + (enj > 100 ? ' enj-value--over' : enj < 0 ? ' enj-value--neg' : '');

    bl.className = 'enj-fill ' + ebo_barColorClass(enj) + (enj < 0 ? ' enj-fill--rtl' : '');
    bl.style.width = `${ebo_barFillPct(enj) * 100}%`;
    bl.classList.toggle('enj-fill--pulse', enj >= 90);

    if (!a.isGameDpt || enj < 80) return;

    const ze = document.getElementById(`ez${i}`);
    const ne = document.getElementById(`en${i}`);
    const ce = document.getElementById(`ec${i}`);
    if (!ze || !ne || !ce) return;

    const doff = Math.max(0.02, 0.125 - ((enj - 80) * 0.00375));
    ebo_GameGreenZoneStart = 0.5 - doff;
    ebo_GameGreenZoneEnd   = 0.5 + doff;

    ze.style.left  = `${ebo_GameGreenZoneStart * 100}%`;
    ze.style.width = `${(ebo_GameGreenZoneEnd - ebo_GameGreenZoneStart) * 100}%`;

    const inZone = (ebo_GameNeedlePos >= ebo_GameGreenZoneStart && ebo_GameNeedlePos <= ebo_GameGreenZoneEnd);
    ze.className = 'enj-zone' + (inZone ? ' enj-zone--active' : '');
    ce.className = 'enj-zone-center' + (inZone ? ' enj-zone-center--active' : '');

    ne.style.left = `${ebo_GameNeedlePos * 100}%`;
    ne.className  = 'enj-needle' + (inZone ? ' enj-needle--active' : '');
  });
};

function ebo_showGameFeedback(idx, result) {
  const fbEl = document.getElementById(`efb${idx}`);
  const flEl = document.getElementById(`efl${idx}`);
  if (!fbEl || !flEl) return;

  const isHit = result === 'hit';
  flEl.className = 'enj-flash ' + (isHit ? 'enj-flash--hit' : 'enj-flash--miss');
  flEl.classList.add('enj-flash--active');
  fbEl.textContent = isHit ? 'HIT' : 'MISS';
  fbEl.style.color = isHit ? 'var(--enj-col-hit)' : 'var(--enj-col-miss)';
  fbEl.classList.add('enj-feedback--visible');

  clearTimeout(ebo_feedbackTimer);
  ebo_feedbackTimer = setTimeout(() => {
    flEl.classList.remove('enj-flash--active');
    fbEl.classList.remove('enj-feedback--visible');
  }, 600);
};