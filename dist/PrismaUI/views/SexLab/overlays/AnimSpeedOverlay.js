'use strict';

const aso_overlayEl = document.getElementById('overlay_animSpeed');

// ── C++ INIT AND DESTROY

window.aso_initOverlay = function(val) {
  if (aso_overlayEl) aso_overlayEl.style.display = '';
  aso_setAnimSpeedDisplay(val)
};

window.aso_destroyOverlay = function() {
  const speedVal = document.getElementById('speedVal');
  if (speedVal) speedVal.textContent = '1.00x';
  const wrap = document.getElementById('timerWrap');
  const fill = document.getElementById('timerFill');
  if (wrap) { wrap.classList.add('disabled'); }
  if (fill) { fill.style.width = '0%'; }
  if (aso_overlayEl) aso_overlayEl.style.display = 'none';
};

// ── C++ TO JS

window.aso_setAnimSpeedDisplay = function(val) {
  const v = parseFloat(val);
  document.getElementById('speedVal').textContent = isNaN(v) ? val : v.toFixed(2) + 'x';
};

window.aso_setStageTimerDisplay = function(data) {
  const parts = data.split('^');
  const duration = parseFloat(parts[0]);
  const timer = parseFloat(parts[1]);
  
  const wrap = document.getElementById('timerWrap');
  const fill = document.getElementById('timerFill');
  
  if (isNaN(duration) || duration <= 0 || isNaN(timer) || timer <= 0) {
    wrap.classList.add('disabled');
    return;
  }
  wrap.classList.remove('disabled');
  const remainingPercent = (timer / duration) * 100;
  fill.style.width = `${remainingPercent}%`;
};

// ── JS TO C++

function aso_onSpeedChange(dir) {
  const step = 0.25;
  const delta = dir > 0 ? step : -step;
  
  if (window.aso_OnAnimSpeedChange) {
    window.aso_OnAnimSpeedChange(delta.toString());
  }
};

// ── DOM LOAD

document.addEventListener('DOMContentLoaded', () => {
  const btnDec = document.getElementById('btnDec');
  const btnInc = document.getElementById('btnInc');

  if (btnDec) btnDec.addEventListener('click', () => aso_onSpeedChange(-1));
  if (btnInc) btnInc.addEventListener('click', () => aso_onSpeedChange(1));
});