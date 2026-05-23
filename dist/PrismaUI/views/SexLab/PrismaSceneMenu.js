window.slp_collapseAllPanels = function() {
  if (typeof oam_collapseAll === 'function') oam_collapseAll();
  if (typeof ssm_collapseAll === 'function') ssm_collapseAll();
  if (typeof tcm_collapseAll === 'function') tcm_collapseAll();
  if (typeof vcm_collapseAll === 'function') vcm_collapseAll();
};

// ── HELPER: TAB HEIGHT

function slp_updateTabHeight() {
  const tabs = document.querySelectorAll('.slp-pull-tab-shell');
  if (!tabs.length) return;
  tabs.forEach(t => t.style.height = '');
  const maxH = Math.max(...Array.from(tabs).map(t => t.getBoundingClientRect().height));
  tabs.forEach(t => t.style.height = maxH + 'px');
  document.documentElement.style.setProperty('--slp-tab-h', maxH + 'px');
}

document.addEventListener('DOMContentLoaded', () => requestAnimationFrame(slp_updateTabHeight));
window.addEventListener('resize', slp_updateTabHeight);

// ── HELPER: TRANSLATION

document.addEventListener('DOMContentLoaded', () => {
  const s = window.SLP_STRINGS;
  if (!s) return;
  const set = (id, val) => { const el = document.getElementById(id); if (el) el.textContent = val; };
  const placeholder = (id, val) => { const el = document.getElementById(id); if (el) el.placeholder = val; };

// OffsetAdjustMenu
  set('oamTabLabel',       s.OAM_TAB_LABEL);
  set('oamSelectActor',    s.OAM_SELECT_ACTOR);
  set('oamAdjustStage',    s.OAM_ADJUST_STAGE);
  set('resetOffsetsRow',   s.OAM_RESET_OFFSETS);

  // SceneSelectorMenu
  set('ssmTabLabel',       s.SSM_TAB_LABEL);
  set('ssmChangeActive',   s.SSM_CHANGE_ACTIVE);
  set('ssmChangeBySearch', s.SSM_CHANGE_BY_SEARCH);
  set('ssmAnnotLabel',     s.SSM_ANNOTATIONS);
  set('searchCancelBtn',   s.SSM_CANCEL);
  set('searchConfirmBtn',  s.SSM_SEARCH);
  placeholder('ssmSearchInput', s.SSM_SEARCH_PLACEHOLDER);

  // ThreadConfigMenu
  set('tcmTabLabel',       s.TCM_TAB_LABEL);
  set('tcmSectionThread',  s.TCM_SECTION_THREAD);
  set('tcmSectionActors',  s.TCM_SECTION_ACTORS);
  set('tcmRandomScene',    s.TCM_RANDOM_SCENE);
  set('tcmMoveScene',      s.TCM_MOVE_SCENE);
  set('tcmToggleAutoplay', s.TCM_TOGGLE_AUTOPLAY);

  // VisibilityControlMenu
  set('vcmTabLabel',        s.VCM_TAB_LABEL);
  set('vcmScaleHeader',     s.VCM_SCALE_HEADER);
  set('vcmScaleLabel',      s.VCM_SCALE_LABEL);
  set('vcmSectionOverlays', s.VCM_SECTION_OVERLAYS);
  set('vcmGameHud',         s.VCM_GAME_HUD);
  set('vcmAnimSpeed',       s.VCM_ANIM_SPEED);
  set('vcmEnjBars',         s.VCM_ENJ_BARS);
  set('vcmOffsetAdj',       s.VCM_OFFSET_ADJ);
  set('vcmSceneSelect',     s.VCM_SCENE_SELECT);
  set('vcmThreadCfg',       s.VCM_THREAD_CFG);
});
