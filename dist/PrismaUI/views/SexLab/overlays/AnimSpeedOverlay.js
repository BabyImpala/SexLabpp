'use strict';

const ASO_SPEED_STEP = 0.25;

const ASO_IDS = Object.freeze({
    OVERLAY:     'overlay_animSpeed',
    SPEED_VAL:   'speedVal',
    TIMER_WRAP:  'timerWrap',
    TIMER_FILL:  'timerFill',
    BTN_DEC:     'btnDec',
    BTN_INC:     'btnInc',
});

// ── CACHED ELEMENTS

let aso_el = {};

function aso_cacheElements() {
    aso_el = {
        overlay:     document.getElementById(ASO_IDS.OVERLAY),
        speedVal:    document.getElementById(ASO_IDS.SPEED_VAL),
        timerWrap:   document.getElementById(ASO_IDS.TIMER_WRAP),
        timerFill:   document.getElementById(ASO_IDS.TIMER_FILL),
        btnDec:      document.getElementById(ASO_IDS.BTN_DEC),
        btnInc:      document.getElementById(ASO_IDS.BTN_INC),
    };
}

// ── C++ INIT AND DESTROY

window.aso_initOverlay = function(val) {
    if (aso_el.overlay) aso_el.overlay.style.display = '';
    aso_setAnimSpeedDisplay(val);
};

window.aso_destroyOverlay = function() {
    if (aso_el.speedVal) aso_el.speedVal.textContent = '1.00x';
    if (aso_el.timerWrap) aso_el.timerWrap.classList.add('disabled');
    if (aso_el.timerFill) aso_el.timerFill.style.width = '0%';
    if (aso_el.overlay) aso_el.overlay.style.display = 'none';
};

// ── C++ TO JS

window.aso_setAnimSpeedDisplay = function(val) {
    if (!aso_el.speedVal) return;
    const v = parseFloat(val);
    aso_el.speedVal.textContent = isNaN(v) ? val : v.toFixed(2) + 'x';
};

window.aso_setStageTimerDisplay = function(data) {
    if (!aso_el.timerWrap || !aso_el.timerFill) return;
    const sep = data.indexOf('^');
    const duration = parseFloat(data);
    const timer = parseFloat(sep !== -1 ? data.substring(sep + 1) : '');

    if (isNaN(duration) || duration <= 0 || isNaN(timer) || timer <= 0) {
        aso_el.timerWrap.classList.add('disabled');
        return;
    }

    aso_el.timerWrap.classList.remove('disabled');
    aso_el.timerFill.style.width = ((timer / duration) * 100) + '%';
};

// ── JS TO C++

function aso_onSpeedChange(dir) {
    if (typeof window.aso_OnAnimSpeedChange === 'function')
        window.aso_OnAnimSpeedChange(String(dir > 0 ? ASO_SPEED_STEP : -ASO_SPEED_STEP));
}

// ── DOM LOAD

document.addEventListener('DOMContentLoaded', () => {
    aso_cacheElements();
    if (aso_el.btnDec) aso_el.btnDec.addEventListener('click', () => aso_onSpeedChange(-1));
    if (aso_el.btnInc) aso_el.btnInc.addEventListener('click', () => aso_onSpeedChange(1));
});
