#include "OffsetAdjustPanel.h"

namespace Thread::Interface
{
    using UI::SetWindowFontSize;

    void OffsetAdjustPanel::Open(SceneHUD& a_hud)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (!inst)
            return;
        _axes.clear();
        auto* ctr = inst->GetCenterRef();

        _hasFurniture = ctr && !ctr->IsPlayerRef();
        _adjustStageOnly = inst->GetThreadProperty<bool>("VarUI_AdjustStage");
        _selectedId.reset();
        _pickerOpen = false;
        _panelOpen = false;
        _draggingAxis = -1;
        _draggingId = 0;

        RefreshSlots(a_hud);

        if (_items.size() == 1) {
            OnActorSelected(a_hud, _items.front());
        } else if (!_items.empty()) {
            _pickerOpen = true;
        }
    }

    void OffsetAdjustPanel::Close()
    {
        _axes.clear();
        _items.clear();
        _selectedId.reset();
        _pickerOpen = false;
        _panelOpen = false;
        _draggingAxis = -1;
        _draggingId = 0;
    }

    void OffsetAdjustPanel::RefreshSlots(SceneHUD& a_hud)
    {
        _items.clear();
        auto* inst = a_hud.GetThreadInstance();
        if (!inst)
            return;

        if (_hasFurniture) {
            ActorItem fi;
            fi.actor = nullptr;
            fi.formId = 0;
            fi.label = "Center";
            fi.isScene = true;
            _items.push_back(fi);
        }

        const auto actors = inst->GetActors();
        for (size_t i = 0; i < actors.size(); ++i) {
            auto* a = actors[i];
            if (!a)
                continue;
            ActorItem item;
            item.actor = a;
            item.formId = a->GetFormID();
            item.posIdx = i;
            item.label = a->GetDisplayFullName();
            item.isScene = false;
            _items.push_back(item);
        }

        std::ranges::sort(_items, [](const ActorItem& a, const ActorItem& b) {
            if (a.isScene != b.isScene)
                return a.isScene;  // sort center/player first
            if (!a.actor || !b.actor)
                return !a.actor;
            if (a.actor->IsPlayerRef() != b.actor->IsPlayerRef())
                return a.actor->IsPlayerRef();
            return a.label < b.label;
        });
    }

    // ── RefreshValues ────────────────────────────────────────────────────────

    void OffsetAdjustPanel::RefreshValues(SceneHUD& a_hud, uint32_t actorId)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (!inst)
            return;
        auto* sc = inst->GetActiveScene();
        auto* st = inst->GetActiveStage();
        if (!sc || !st)
            return;

        std::array<float, 4> raw{};
        if (actorId == 0) {
            const auto v = sc->furnitureOffset.GetOffset().AsVector();
            std::copy_n(v.begin(), 4, raw.begin());
        } else {
            for (const auto& item : _items) {
                if (item.formId == actorId) {
                    const auto v = st->positions[item.posIdx].offset.GetOffset().AsVector();
                    std::copy_n(v.begin(), 4, raw.begin());
                    break;
                }
            }
        }

        auto& axes = _axes[actorId];
        for (int i = 0; i < 4; ++i) {
            const float val = (i == 3) ? raw[i] * kRadToDeg : raw[i];
            if (!axes[i].hasBaseline) {
                axes[i].baseline = val;
                axes[i].hasBaseline = true;
            }
            // Don't overwrite value while user is actively dragging this axis — that would fight the drag.
            if (_draggingAxis == i && _draggingId == actorId)
                continue;
            axes[i].value = val;
        }
    }

    void OffsetAdjustPanel::OnStageChanged(SceneHUD& a_hud)
    {
        if (!_selectedId)
            return;
        RefreshValues(a_hud, *_selectedId);
    }

    // ── Handlers ──────────────────────────────────────────────────────────────

    void OffsetAdjustPanel::OnActorSelected(SceneHUD& a_hud, const ActorItem& item)
    {
        _selectedId = item.formId;
        _pickerOpen = false;
        _panelOpen = true;
        if (!_axes.contains(item.formId) || !_axes[item.formId][0].hasBaseline)
            RefreshValues(a_hud, item.formId);
    }

    void OffsetAdjustPanel::OnSetOffset(SceneHUD& a_hud, Registry::CoordinateType axis, uint32_t actorId, float value)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (inst)
            inst->OffsetAdjustSet(actorId, axis, value);
    }

    void OffsetAdjustPanel::OnResetOffsets(SceneHUD& a_hud)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (!inst || !_selectedId)
            return;
        inst->OffsetAdjustReset(_hasFurniture);
        _axes.clear();
        RefreshValues(a_hud, *_selectedId);
    }

    void OffsetAdjustPanel::OnSetAdjustStageOnly(SceneHUD& a_hud, bool state)
    {
        _adjustStageOnly = state;
        auto* inst = a_hud.GetThreadInstance();
        if (inst)
            inst->SetThreadProperty<bool>("VarUI_AdjustStage", state);
    }

    // ── The offset track widget ───────────────────────────────────────────────
    bool OffsetAdjustPanel::OffsetTrack(UI::Scale& a_scale, const char* axisLabel, AxisState& state, float range, bool& draggingOut)
    {
        bool changed = false;
        draggingOut = false;

        const float rowPadH = a_scale.Px(12.0f);
        const float trackH = a_scale.Px(4.0f);   // track thickness
        const float needleW = a_scale.Px(3.0f);  // needle thickness
        const float hitExt = a_scale.Px(10.0f);  // extends clickable/draggable area above and below visible track
        const float valW = a_scale.Px(40.0f);    // width of the numeric value field
        const float labelW = a_scale.Px(20.0f);  // width for axis label
        const float labelFt = a_scale.TextPx(UI::Theme::FontSize.body);
        const float valFt = a_scale.TextPx(UI::Theme::FontSize.body);
        const float availW = ImGuiMCP::GetContentRegionAvail().x - rowPadH * 2.0f;
        const float trackW = availW - labelW - valW - rowPadH * 2.0f;  // space between label and value

        const ImGuiMCP::ImVec2 rowOrigin = ImGuiMCP::GetCursorScreenPos();
        const float rowH = hitExt * 2.0f + trackH + a_scale.Px(8.0f);
        const float rowCenterY = rowOrigin.y + rowH * 0.5f;

        // ────── All on one line: Label | Track | Value

        // Reserve full row height so ImGui advances the cursor correctly at the end
        ImGuiMCP::Dummy(ImGuiMCP::ImVec2{ availW, rowH });
        ImGuiMCP::SetCursorScreenPos(rowOrigin);  // rewind; we draw manually below
        SetWindowFontSize(labelFt);

        // Label (double-click resets to baseline)
        const float labelTextH = ImGuiMCP::CalcTextSize(axisLabel).y;
        const ImGuiMCP::ImVec2 lblMin = { rowOrigin.x + rowPadH, rowCenterY - labelTextH * 0.5f };
        ImGuiMCP::SetCursorScreenPos(lblMin);
        ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color.textSecondary), "%s", axisLabel);
        const ImGuiMCP::ImVec2 lblMax = ImGuiMCP::ImVec2{ lblMin.x + ImGuiMCP::CalcTextSize(axisLabel).x, lblMin.y + labelTextH };
        if (ImGuiMCP::IsMouseHoveringRect(lblMin, lblMax) &&
            ImGuiMCP::IsMouseDoubleClicked(ImGuiMCP::ImGuiMouseButton_Left)) {
            state.value = state.baseline;
            changed = true;
        }

        // Track (+ needle + fill)
        const ImGuiMCP::ImVec2 trackMin = { rowOrigin.x + rowPadH + labelW + rowPadH, rowCenterY - trackH * 0.5f };
        const ImGuiMCP::ImVec2 trackMax = ImGuiMCP::ImVec2{ trackMin.x + trackW, trackMin.y + trackH };

        // Invisible button with extended hit area
        const ImGuiMCP::ImVec2 hitMin = ImGuiMCP::ImVec2{ trackMin.x, trackMin.y - hitExt };
        const ImGuiMCP::ImVec2 hitMax = ImGuiMCP::ImVec2{ trackMax.x, trackMax.y + hitExt };
        ImGuiMCP::SetCursorScreenPos(hitMin);
        ImGuiMCP::InvisibleButton("##slpp_oamTrack", ImGuiMCP::ImVec2{ trackW, trackH + hitExt * 2.0f });
        const bool hovered = ImGuiMCP::IsItemHovered();
        const bool active = ImGuiMCP::IsItemActive();

        // Value (input field)
        const float inputH = ImGuiMCP::GetFrameHeight();
        ImGuiMCP::SetCursorScreenPos({ rowOrigin.x + rowPadH + labelW + rowPadH + trackW + rowPadH, rowCenterY - inputH * 0.5f });
        SetWindowFontSize(valFt);
        char valBuf[16];
        std::snprintf(valBuf, sizeof(valBuf),
            "%d", static_cast<int>(std::round(state.value)));
        ImGuiMCP::SetNextItemWidth(valW);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBg, UI::Theme::Color.transparent);
        const ImGuiMCP::ImU32 valTextCol = UI::Theme::Color.textMuted;
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, valTextCol);
        if (ImGuiMCP::InputText("##slpp_oamVal", valBuf, sizeof(valBuf),
                ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue | ImGuiMCP::ImGuiInputTextFlags_CharsDecimal)) {
            float v = std::round(std::strtof(valBuf, nullptr));
            if (std::isnan(v))
                v = state.value;
            if (range == kRangeR)
                v = std::clamp(v, -kRangeR, kRangeR);
            state.value = v;
            changed = true;
        }
        if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
            float v = std::round(std::strtof(valBuf, nullptr));
            if (!std::isnan(v)) {
                if (range == kRangeR)
                    v = std::clamp(v, -kRangeR, kRangeR);
                state.value = v;
                changed = true;
            }
        }
        ImGuiMCP::PopStyleColor(2);

        // Drag (activated on first frame, captures start value)
        if (ImGuiMCP::IsItemActivated()) {
            state.dragStartValue = state.value;
            _draggingAxis = -1;  // will be set by caller
            draggingOut = true;
        }
        if (active) {
            const float dx = ImGuiMCP::GetMouseDragDelta(ImGuiMCP::ImGuiMouseButton_Left).x;
            float v = std::round(state.dragStartValue + dx * (range / kDragScale));
            if (range == kRangeR)
                v = std::clamp(v, -kRangeR, kRangeR);
            state.value = v;
            draggingOut = true;  // Notify on every frame while dragging
            changed = true;
        }

        // Drag released: final notification
        if (ImGuiMCP::IsItemDeactivated() && !active) {
            changed = true;
        }

        // Arrow keys nudge the value by 1 while the slider is hovered.
        if (hovered) {
            float delta = 0.0f;
            if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_RightArrow))
                delta = 1.0f;
            else if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_LeftArrow))
                delta = -1.0f;
            if (delta != 0.0f) {
                float v = state.value + delta;
                if (range == kRangeR)
                    v = std::clamp(v, -kRangeR, kRangeR);
                state.value = v;
                changed = true;
            }
        }

        // ── Draw track ─────────────────────────────────────────────────────────
        auto* dl = ImGuiMCP::GetWindowDrawList();

        // Track background
        ImGuiMCP::ImDrawListManager::AddRectFilled(dl, trackMin, trackMax, UI::Theme::Offset.track, a_scale.Px(2.0f), 0);
        ImGuiMCP::ImDrawListManager::AddRect(dl, trackMin, trackMax, UI::Theme::Offset.trackBorder, a_scale.Px(2.0f), 0, 1.0f);

        // Center tick
        const float cx = trackMin.x + trackW * 0.5f;
        ImGuiMCP::ImDrawListManager::AddLine(dl,
            ImGuiMCP::ImVec2{ cx, trackMin.y - a_scale.Px(2.0f) }, ImGuiMCP::ImVec2{ cx, trackMax.y + a_scale.Px(2.0f) },
            UI::Theme::Offset.centerTick, 1.0f);

        // Fill grows from the center out toward the needle, in either direction.
        const float pct = std::clamp(0.5f + (state.value / range) * 0.5f, 0.0f, 1.0f);
        const float needleX = trackMin.x + pct * trackW;
        if (state.value != 0.0f) {
            const float fillL = std::min(cx, needleX);
            const float fillR = std::max(cx, needleX);
            ImGuiMCP::ImDrawListManager::AddRectFilled(dl,
                ImGuiMCP::ImVec2{ fillL, trackMin.y }, ImGuiMCP::ImVec2{ fillR, trackMax.y },
                UI::Theme::Offset.fill, 0.0f, 0);
        }

        // Needle extends slightly above and below the track so it stays visible
        const float nTop = trackMin.y - a_scale.Px(5.0f);
        const float nBot = trackMax.y + a_scale.Px(5.0f);
        const ImGuiMCP::ImU32 nCol = active ? UI::Theme::Offset.needleActive : UI::Theme::Offset.needle;
        ImGuiMCP::ImDrawListManager::AddRectFilled(dl,
            ImGuiMCP::ImVec2{ needleX - needleW * 0.5f, nTop },
            ImGuiMCP::ImVec2{ needleX + needleW * 0.5f, nBot },
            nCol, a_scale.Px(1.5f), 0);

        // Separator
        const ImGuiMCP::ImVec2 sepPos = ImGuiMCP::GetCursorScreenPos();
        ImGuiMCP::ImDrawListManager::AddLine(dl,
            ImGuiMCP::ImVec2{ rowOrigin.x + rowPadH, sepPos.y },
            ImGuiMCP::ImVec2{ rowOrigin.x + rowPadH + availW, sepPos.y },
            UI::Theme::Offset.separator, 1.0f);

        return changed;
    }

    // ── Render ────────────────────────────────────────────────────────────────

    void OffsetAdjustPanel::Render(SceneHUD& a_hud)
    {
        auto& scale = a_hud.GetScale();

        auto* io = ImGuiMCP::GetIO();
        const float dw = io->DisplaySize.x;
        const float dh = io->DisplaySize.y;

        const float offset = scale.Px(UI::Theme::Geometry.panelTabWidth + UI::Theme::Geometry.panelTabGap);
        const float pickerW = scale.Px(200.0f);
        const float panelW = scale.Px(300.0f);

        // ── Target picker
        if (!_panelOpen && !_items.empty()) {
            ImGuiMCP::SetNextWindowPos(
                ImGuiMCP::ImVec2{ dw - offset, dh * 0.5f }, ImGuiMCP::ImGuiCond_Always, ImGuiMCP::ImVec2{ 1.0f, 0.5f });
            ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2{ pickerW, 0.0f }, ImGuiMCP::ImGuiCond_Always);
            constexpr auto pFlags =
                ImGuiMCP::ImGuiWindowFlags_NoTitleBar | ImGuiMCP::ImGuiWindowFlags_NoResize |
                ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoScrollbar |
                ImGuiMCP::ImGuiWindowFlags_NoCollapse | ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize;

            if (ImGuiMCP::Begin("##slpp_OAMPicker", nullptr, pFlags)) {
                SetWindowFontSize(scale.TextPx(UI::Theme::FontSize.sectionHeader));
                const float titleW = ImGuiMCP::CalcTextSize("PICK TARGET").x;
                ImGuiMCP::SetCursorPosX((pickerW - titleW) * 0.5f);
                ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color.textPrimary), "PICK TARGET");
                ImGuiMCP::Dummy(ImGuiMCP::ImVec2{ 0.0f, scale.Px(4.0f) });
                ImGuiMCP::Separator();

                if (_pickerOpen) {
                    SetWindowFontSize(scale.TextPx(UI::Theme::FontSize.body));
                    for (const auto& item : _items) {
                        ImGuiMCP::PushID(static_cast<int>(item.formId));
                        const bool isSel = _selectedId && *_selectedId == item.formId;

                        std::string lbl = item.label;
                        if (item.isScene)
                            lbl += " [SCENE]";

                        if (isSel)
                            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Header, UI::Theme::Color.selectionFill);
                        if (UI::SelectableButton(lbl.c_str(), isSel, 0, ImGuiMCP::ImVec2{ 0, scale.Px(28.0f) }))
                            OnActorSelected(a_hud, item);
                        if (isSel)
                            ImGuiMCP::PopStyleColor();
                        ImGuiMCP::PopID();
                    }
                }
                ImGuiMCP::SetWindowFontScale(1.0f);
            }
            ImGuiMCP::End();
        }

        // ── Adjustment panel
        if (_panelOpen && _selectedId) {
            const uint32_t actorId = *_selectedId;
            auto& axes = _axes[actorId];
            if (!axes[0].hasBaseline)
                RefreshValues(a_hud, actorId);

            std::string panelTitle;
            for (const auto& item : _items) {
                if (item.formId == actorId) {
                    panelTitle = item.label;
                    if (item.isScene)
                        panelTitle += " [SCENE]";
                    break;
                }
            }

            ImGuiMCP::SetNextWindowPos(
                ImGuiMCP::ImVec2{ dw - offset, dh * 0.5f }, ImGuiMCP::ImGuiCond_Always, ImGuiMCP::ImVec2{ 1.0f, 0.5f });
            ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2{ panelW, 0.0f }, ImGuiMCP::ImGuiCond_Always);
            constexpr auto panelFlags =
                ImGuiMCP::ImGuiWindowFlags_NoTitleBar | ImGuiMCP::ImGuiWindowFlags_NoResize |
                ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoScrollbar |
                ImGuiMCP::ImGuiWindowFlags_NoCollapse | ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize;

            if (ImGuiMCP::Begin("##slpp_OAMPanel", nullptr, panelFlags)) {
                // ────── Title
                SetWindowFontSize(scale.TextPx(UI::Theme::FontSize.sectionHeader));
                const float titleW = ImGuiMCP::CalcTextSize(panelTitle.c_str()).x;
                ImGuiMCP::SetCursorPosX((panelW - titleW) * 0.5f);
                ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color.textPrimary), "%s", panelTitle.c_str());
                ImGuiMCP::Dummy(ImGuiMCP::ImVec2{ 0.0f, scale.Px(4.0f) });
                ImGuiMCP::Separator();

                // ────── Stage Only
                ImGuiMCP::Dummy(ImGuiMCP::ImVec2{ 0.0f, scale.Px(6.0f) });
                // toggle row
                SetWindowFontSize(scale.TextPx(UI::Theme::FontSize.body));
                const float toggleRowH = scale.Px(24.0f);
                const float rowPadH = scale.Px(12.0f);
                const float availW = ImGuiMCP::GetContentRegionAvail().x - rowPadH * 2.0f;
                const ImGuiMCP::ImVec2 toggleRowMin = ImGuiMCP::GetCursorScreenPos();
                ImGuiMCP::SetCursorScreenPos({ toggleRowMin.x + rowPadH, toggleRowMin.y });
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_HeaderHovered, UI::Theme::ToVec4(UI::Theme::Color.transparent));
                if (UI::SelectableButton("##slpp_stageOnlyRow", false, 0, ImGuiMCP::ImVec2{ availW, toggleRowH })) {
                    OnSetAdjustStageOnly(a_hud, !_adjustStageOnly);
                }
                ImGuiMCP::PopStyleColor();
                // checkbox
                bool stageOnly = _adjustStageOnly;
                const float cbSize = ImGuiMCP::GetFrameHeight();
                const float cbX = toggleRowMin.x + rowPadH + availW - cbSize;
                const float cbY = toggleRowMin.y + (toggleRowH - cbSize) * 0.5f + scale.Px(3.0f);
                ImGuiMCP::SetCursorScreenPos({ cbX, cbY });
                UI::PushCheckboxStyle(scale.Factor());
                bool cbChanged = ImGuiMCP::Checkbox("##slpp_oamStageOnly", &stageOnly);
                UI::PopCheckboxStyle();
                if (cbChanged)
                    OnSetAdjustStageOnly(a_hud, stageOnly);
                // label
                const ImGuiMCP::ImVec2 labelSize = ImGuiMCP::CalcTextSize("Adjust Stage Only");
                const float labelY = toggleRowMin.y + (toggleRowH - labelSize.y) * 0.5f;
                ImGuiMCP::SetCursorScreenPos({ toggleRowMin.x + rowPadH, labelY });
                ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color.textSecondary), "Adjust Stage Only");
                ImGuiMCP::SetCursorScreenPos({ toggleRowMin.x, toggleRowMin.y + toggleRowH });

                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Separator, UI::Theme::Offset.separator);
                ImGuiMCP::Dummy(ImGuiMCP::ImVec2{ 0.0f, scale.Px(6.0f) });
                ImGuiMCP::Separator();
                ImGuiMCP::PopStyleColor();

                // ────── Axis Sliders
                static constexpr const char* kLabels[4] = { "X", "Y", "Z", "R" };
                static constexpr Registry::CoordinateType kAxisEnums[4] = {
                    Registry::CoordinateType::X, Registry::CoordinateType::Y,
                    Registry::CoordinateType::Z, Registry::CoordinateType::R
                };

                for (int i = 0; i < 4; ++i) {
                    ImGuiMCP::PushID(i);
                    const float range = (i == 3) ? kRangeR : kRangeXYZ;
                    bool drag = false;

                    if (OffsetTrack(scale, kLabels[i], axes[i], range, drag)) {
                        OnSetOffset(a_hud, kAxisEnums[i], actorId, axes[i].value);
                    }

                    if (drag) {
                        _draggingAxis = i;
                        _draggingId = actorId;
                    } else if (_draggingAxis == i && _draggingId == actorId && !ImGuiMCP::IsMouseDown(ImGuiMCP::ImGuiMouseButton_Left)) {
                        _draggingAxis = -1;
                    }
                    ImGuiMCP::PopID();
                }

                // ────── Reset offsets button
                ImGuiMCP::Dummy(ImGuiMCP::ImVec2{ 0.0f, scale.Px(6.0f) });
                const float btnW = panelW - scale.Px(24.0f);
                ImGuiMCP::SetCursorPosX(scale.Px(12.0f));
                if (UI::ActionButton("Reset Offsets", btnW))
                    OnResetOffsets(a_hud);
                ImGuiMCP::Dummy(ImGuiMCP::ImVec2{ 0.0f, scale.Px(6.0f) });

                ImGuiMCP::SetWindowFontScale(1.0f);
            }
            ImGuiMCP::End();
        }
    }
}
