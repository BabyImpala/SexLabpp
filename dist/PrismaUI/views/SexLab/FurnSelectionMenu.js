'use strict';

// ── C++ TO JS

window.fsm_populateItems = function(payloadStr) {
  let payload;
  try { payload = JSON.parse(payloadStr); } catch(e) { return; }
  const items = Array.isArray(payload.items) ? payload.items : [];
  const list  = document.getElementById('fsmList');
  const empty = document.getElementById('fsmEmpty');
  const popup = document.getElementById('fsmPopup');

  if (popup) popup.classList.add('open');

  list.querySelectorAll('.fsm-row').forEach(row => row.remove());

  if (!items.length) {
    if (empty) empty.style.display = 'block';
    return;
  }
  if (empty) empty.style.display = 'none';

  const frag = document.createDocumentFragment();
  items.forEach(item => {
    const row = document.createElement('div');
    row.className = 'slp-panel-row slp-panel-row--clickable fsm-row';
    row.dataset.index = item.index;
    row.innerHTML =
      '<span class="slp-panel-row-name">' + fsm_esc(item.name) + '</span>' +
      (item.type ? '<span class="fsm-row-type">' + fsm_esc(item.type) + '</span>' : '');
    row.addEventListener('click', () => fsm_selectItem(item.index));
    frag.appendChild(row);
  });
  list.appendChild(frag);
};

// ── JS TO C++

function fsm_fireItemSelected(indexStr) {
  if (typeof window.fsm_OnFurnItemSelected === 'function') {
    window.fsm_OnFurnItemSelected(indexStr);
    return;
  }
}

function fsm_closeMenu() {
  const popup = document.getElementById('fsmPopup');
  if (popup) popup.classList.remove('open');
}

// ── INTERNAL

function fsm_highlightRow(index) {
  const list = document.getElementById('fsmList');
  list.querySelectorAll('.fsm-row').forEach(r => r.classList.remove('active'));
  const target = Array.from(list.querySelectorAll('.fsm-row'))
    .find(r => r.dataset.index == index);
  if (target) {
    target.classList.add('active');
    target.scrollIntoView({ block: 'nearest' });
  }
}

function fsm_selectItem(index) {
  fsm_highlightRow(index);
  setTimeout(() => {
    const popup = document.getElementById('fsmPopup');
    if (popup) popup.classList.remove('open');
    fsm_fireItemSelected(String(index));
  }, 80);
}

// ── HELPERS

function fsm_esc(str) {
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

// ── DOM LOAD

document.addEventListener('DOMContentLoaded', () => {
  document.addEventListener('keydown', e => {
    const list = document.getElementById('fsmList');
    const popup = document.getElementById('fsmPopup');
    if (!popup || !popup.classList.contains('open')) return;

    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      const rows = Array.from(list.querySelectorAll('.fsm-row'));
      if (!rows.length) return;
      const cur  = list.querySelector('.fsm-row.active');
      const idx  = cur ? rows.indexOf(cur) : -1;
      const next = e.key === 'ArrowDown'
        ? rows[Math.min(idx + 1, rows.length - 1)]
        : rows[Math.max(idx - 1, 0)];
      if (next) fsm_highlightRow(next.dataset.index);
      return;
    }

    if (e.key === 'Enter') {
      const activeRow = list.querySelector('.fsm-row.active');
      if (activeRow) fsm_selectItem(activeRow.dataset.index);
    }
  });

});
