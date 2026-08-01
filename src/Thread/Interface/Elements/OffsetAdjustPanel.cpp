#include "OffsetAdjustPanel.h"

#include <numbers>

namespace Thread::Interface
{
    using UI::SetWindowFontSize;

    namespace
    {
        constexpr float kTargetPickerWidth = 200.0f;
        constexpr float kAdjustmentPanelWidth = 300.0f;
        constexpr float kTargetRowHeight = 28.0f;
        constexpr float kTrackHitExtension = 10.0f;
        constexpr float kTrackValueWidth = 40.0f;
        constexpr float kTrackLabelWidth = 20.0f;
        constexpr float kTrackHeight = 4.0f;
        constexpr float kTrackNeedleWidth = 3.0f;
        constexpr float kTrackNeedleExtension = 5.0f;
        constexpr float kTrackNeedleRounding = 1.5f;
        constexpr float kTrackCenterTickExtension = 2.0f;
        constexpr float kDragDistanceUnits = 300.0f;
        constexpr float kDegreesPerRadian = 180.0f / std::numbers::pi_v<float>;
        constexpr auto kPanelWindowFlags =
            ImGuiMCP::ImGuiWindowFlags_NoTitleBar | ImGuiMCP::ImGuiWindowFlags_NoResize |
            ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoScrollbar |
            ImGuiMCP::ImGuiWindowFlags_NoCollapse | ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize;

        void DrawPanelHeader(UI::Scale& a_scale, const char* a_title)
        {
            SetWindowFontSize(a_scale.TextPx(UI::Theme::FontSize.sectionHeader));
            const float titleWidth = ImGuiMCP::CalcTextSize(a_title).x;
            ImGuiMCP::SetCursorPosX(ImGuiMCP::GetCursorPosX() + std::max((ImGuiMCP::GetContentRegionAvail().x - titleWidth) * 0.5f, 0.0f));
            ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color.textPrimary), "%s", a_title);
            ImGuiMCP::Dummy({ 0.0f, a_scale.Px(UI::Theme::Spacing.xs) });
            ImGuiMCP::Separator();
        }
    }

    void OffsetAdjustPanel::Open(SceneHUD& a_hud)
    {
        Close();
        auto* instance = a_hud.GetThreadInstance();
        if (!instance)
            return;

        const auto* center = instance->GetCenterRef();
        _hasFurnitureCenter = center && !center->IsPlayerRef();
        _adjustStageOnly = instance->GetThreadProperty<bool>("VarUI_AdjustStage");
        RefreshTargets(a_hud);

        if (_targets.size() == 1)
            OnTargetSelected(a_hud, 0);
    }

    void OffsetAdjustPanel::Close()
    {
        _targets.clear();
        _axes = {};
        _selectedTarget.reset();
        _draggingAxis.reset();
        _hasFurnitureCenter = false;
        _adjustStageOnly = false;
    }

    void OffsetAdjustPanel::RefreshTargets(SceneHUD& a_hud)
    {
        _targets.clear();
        auto* instance = a_hud.GetThreadInstance();
        if (!instance)
            return;

        if (_hasFurnitureCenter)
            _targets.push_back({ nullptr, 0, 0, "Center", true });

        const auto actors = instance->GetActors();
        for (std::size_t positionIndex = 0; positionIndex < actors.size(); ++positionIndex) {
            auto* actor = actors[positionIndex];
            if (!actor)
                continue;
            _targets.push_back({ actor, actor->GetFormID(), positionIndex, actor->GetDisplayFullName(), false });
        }

        std::ranges::sort(_targets, [](const TargetItem& a_left, const TargetItem& a_right) {
            if (a_left.isCenter != a_right.isCenter)
                return a_left.isCenter;
            if (!a_left.actor || !a_right.actor)
                return !a_left.actor && a_right.actor;
            if (a_left.actor->IsPlayerRef() != a_right.actor->IsPlayerRef())
                return a_left.actor->IsPlayerRef();
            return a_left.label < a_right.label;
        });
    }

    void OffsetAdjustPanel::RefreshValues(SceneHUD& a_hud)
    {
        if (!_selectedTarget)
            return;

        auto* instance = a_hud.GetThreadInstance();
        if (!instance)
            return;
        const auto* scene = instance->GetActiveScene();
        const auto* stage = instance->GetActiveStage();
        if (!scene || !stage)
            return;

        const auto& target = _targets[*_selectedTarget];
        const Registry::Coordinate* offset = nullptr;
        if (target.isCenter) {
            offset = &scene->furnitureOffset.GetOffset();
        } else if (target.positionIndex < stage->positions.size()) {
            offset = &stage->positions[target.positionIndex].offset.GetOffset();
        }
        if (!offset)
            return;

        const std::array rawValues{ offset->location.x, offset->location.y, offset->location.z, offset->rotation };
        for (std::size_t axisIndex = 0; axisIndex < kAxisDefinitions.size(); ++axisIndex) {
            auto& state = _axes[axisIndex];
            const float value = kAxisDefinitions[axisIndex].coordinate == Registry::CoordinateType::R ?
                                    rawValues[axisIndex] * kDegreesPerRadian :
                                    rawValues[axisIndex];
            if (!state.hasBaseline) {
                state.baseline = value;
                state.hasBaseline = true;
            }
            if (_draggingAxis != axisIndex)
                state.value = value;
        }
    }

    void OffsetAdjustPanel::RefreshStageOffsets(SceneHUD& a_hud)
    {
        RefreshValues(a_hud);
    }

    void OffsetAdjustPanel::OnTargetSelected(SceneHUD& a_hud, std::size_t a_targetIndex)
    {
        _selectedTarget = a_targetIndex;
        _axes = {};
        _draggingAxis.reset();
        RefreshValues(a_hud);
    }

    void OffsetAdjustPanel::OnSetOffset(SceneHUD& a_hud, Registry::CoordinateType a_axis, std::uint32_t a_targetId, float a_value)
    {
        if (auto* instance = a_hud.GetThreadInstance())
            instance->OffsetAdjustSet(a_targetId, a_axis, a_value);
    }

    void OffsetAdjustPanel::OnResetOffsets(SceneHUD& a_hud)
    {
        auto* instance = a_hud.GetThreadInstance();
        if (!instance || !_selectedTarget)
            return;
        instance->OffsetAdjustReset(_hasFurnitureCenter);
        _axes = {};
        _draggingAxis.reset();
        RefreshValues(a_hud);
    }

    void OffsetAdjustPanel::OnSetAdjustStageOnly(SceneHUD& a_hud, bool a_state)
    {
        _adjustStageOnly = a_state;
        if (auto* instance = a_hud.GetThreadInstance())
            instance->SetThreadProperty<bool>("VarUI_AdjustStage", a_state);
    }

    bool OffsetAdjustPanel::OffsetTrack(UI::Scale& a_scale, const AxisDefinition& a_axis, AxisState& a_state, bool& a_draggingOut)
    {
        bool changed = false;
        a_draggingOut = false;

        const float horizontalPadding = a_scale.Px(UI::Theme::Spacing.lg);
        const float trackHeight = a_scale.Px(kTrackHeight);
        const float needleWidth = a_scale.Px(kTrackNeedleWidth);
        const float hitExtension = a_scale.Px(kTrackHitExtension);
        const float valueWidth = a_scale.Px(kTrackValueWidth);
        const float labelWidth = a_scale.Px(kTrackLabelWidth);
        const float availableWidth = ImGuiMCP::GetContentRegionAvail().x - horizontalPadding * 2.0f;
        const float trackWidth = availableWidth - labelWidth - valueWidth - horizontalPadding * 2.0f;
        const float rowHeight = hitExtension * 2.0f + trackHeight + a_scale.Px(UI::Theme::Spacing.md);
        const ImGuiMCP::ImVec2 rowOrigin = ImGuiMCP::GetCursorScreenPos();
        const float rowCenterY = rowOrigin.y + rowHeight * 0.5f;
        const float rowEndY = rowOrigin.y + rowHeight;

        ImGuiMCP::Dummy({ availableWidth, rowHeight });
        ImGuiMCP::SetCursorScreenPos(rowOrigin);
        SetWindowFontSize(a_scale.TextPx(UI::Theme::FontSize.body));

        const ImGuiMCP::ImVec2 labelSize = ImGuiMCP::CalcTextSize(a_axis.label);
        const ImGuiMCP::ImVec2 labelMin{ rowOrigin.x + horizontalPadding, rowCenterY - labelSize.y * 0.5f };
        const ImGuiMCP::ImVec2 labelMax{ labelMin.x + labelSize.x, labelMin.y + labelSize.y };
        ImGuiMCP::SetCursorScreenPos(labelMin);
        ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color.textSecondary), "%s", a_axis.label);
        if (ImGuiMCP::IsMouseHoveringRect(labelMin, labelMax) && ImGuiMCP::IsMouseDoubleClicked(ImGuiMCP::ImGuiMouseButton_Left)) {
            a_state.value = a_state.baseline;
            changed = true;
        }

        const ImGuiMCP::ImVec2 trackMin{ rowOrigin.x + horizontalPadding * 2.0f + labelWidth, rowCenterY - trackHeight * 0.5f };
        const ImGuiMCP::ImVec2 trackMax{ trackMin.x + trackWidth, trackMin.y + trackHeight };
        const ImGuiMCP::ImVec2 hitMin{ trackMin.x, trackMin.y - hitExtension };
        ImGuiMCP::SetCursorScreenPos(hitMin);
        ImGuiMCP::InvisibleButton("##slpp_oamTrack", { trackWidth, trackHeight + hitExtension * 2.0f });
        const bool trackHovered = ImGuiMCP::IsItemHovered();
        const bool trackActive = ImGuiMCP::IsItemActive();
        const bool trackActivated = ImGuiMCP::IsItemActivated();

        if (trackActivated)
            a_state.dragStartValue = a_state.value;
        if (trackActive) {
            const float dragDelta = ImGuiMCP::GetMouseDragDelta(ImGuiMCP::ImGuiMouseButton_Left).x;
            float value = std::round(a_state.dragStartValue + dragDelta * (a_axis.displayRange / a_scale.Px(kDragDistanceUnits)));
            if (a_axis.coordinate == Registry::CoordinateType::R)
                value = std::clamp(value, -kRotationDisplayRange, kRotationDisplayRange);
            a_state.value = value;
            a_draggingOut = true;
            changed = true;
        }

        if (trackHovered) {
            float delta = 0.0f;
            if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_RightArrow))
                delta = 1.0f;
            else if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_LeftArrow))
                delta = -1.0f;
            if (delta != 0.0f) {
                a_state.value += delta;
                if (a_axis.coordinate == Registry::CoordinateType::R)
                    a_state.value = std::clamp(a_state.value, -kRotationDisplayRange, kRotationDisplayRange);
                changed = true;
            }
        }

        const float inputHeight = ImGuiMCP::GetFrameHeight();
        ImGuiMCP::SetCursorScreenPos({ trackMax.x + horizontalPadding, rowCenterY - inputHeight * 0.5f });
        int inputValue = static_cast<int>(std::round(a_state.value));
        ImGuiMCP::SetNextItemWidth(valueWidth);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBg, UI::Theme::Color.transparent);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, UI::Theme::Color.textMuted);
        const bool inputSubmitted = ImGuiMCP::InputInt("##slpp_oamValue", &inputValue, 0, 0, ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue);
        const bool inputDeactivated = ImGuiMCP::IsItemDeactivatedAfterEdit();
        ImGuiMCP::PopStyleColor(2);
        if (inputSubmitted || inputDeactivated) {
            a_state.value = static_cast<float>(inputValue);
            if (a_axis.coordinate == Registry::CoordinateType::R)
                a_state.value = std::clamp(a_state.value, -kRotationDisplayRange, kRotationDisplayRange);
            changed = true;
        }

        auto* drawList = ImGuiMCP::GetWindowDrawList();
        const float trackRounding = a_scale.Px(UI::Theme::Geometry.roundingSmall);
        ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, trackMin, trackMax, UI::Theme::Offset.track, trackRounding, 0);
        ImGuiMCP::ImDrawListManager::AddRect(drawList, trackMin, trackMax, UI::Theme::Color.borderSubtle,
            trackRounding, 0, a_scale.Px(UI::Theme::Geometry.borderThin));

        const float centerX = trackMin.x + trackWidth * 0.5f;
        const float centerTickExtension = a_scale.Px(kTrackCenterTickExtension);
        ImGuiMCP::ImDrawListManager::AddLine(drawList,
            { centerX, trackMin.y - centerTickExtension }, { centerX, trackMax.y + centerTickExtension },
            UI::Theme::Offset.centerTick, a_scale.Px(UI::Theme::Geometry.borderThin));

        const float trackPercent = std::clamp(0.5f + (a_state.value / a_axis.displayRange) * 0.5f, 0.0f, 1.0f);
        const float needleX = trackMin.x + trackPercent * trackWidth;
        if (a_state.value != 0.0f) {
            ImGuiMCP::ImDrawListManager::AddRectFilled(drawList,
                { std::min(centerX, needleX), trackMin.y }, { std::max(centerX, needleX), trackMax.y },
                UI::Theme::Offset.fill, 0.0f, 0);
        }

        const float needleExtension = a_scale.Px(kTrackNeedleExtension);
        ImGuiMCP::ImDrawListManager::AddRectFilled(drawList,
            { needleX - needleWidth * 0.5f, trackMin.y - needleExtension },
            { needleX + needleWidth * 0.5f, trackMax.y + needleExtension },
            trackActive ? UI::Theme::Color.textPrimary : UI::Theme::Color.textSecondary,
            a_scale.Px(kTrackNeedleRounding), 0);

        ImGuiMCP::ImDrawListManager::AddLine(drawList,
            { rowOrigin.x + horizontalPadding, rowEndY },
            { rowOrigin.x + horizontalPadding + availableWidth, rowEndY },
            UI::Theme::Color.nestedSeparator, a_scale.Px(UI::Theme::Geometry.borderThin));
        ImGuiMCP::SetCursorScreenPos({ rowOrigin.x, rowEndY });

        return changed;
    }

    void OffsetAdjustPanel::Render(SceneHUD& a_hud)
    {
        if (_targets.empty())
            return;
        if (_selectedTarget)
            RenderAdjustmentPanel(a_hud);
        else
            RenderTargetPicker(a_hud);
    }

    void OffsetAdjustPanel::RenderTargetPicker(SceneHUD& a_hud)
    {
        auto& scale = a_hud.GetScale();
        const auto* io = ImGuiMCP::GetIO();
        const float panelOffset = scale.Px(UI::Theme::Geometry.panelTabWidth + UI::Theme::Geometry.panelTabGap);
        const float panelWidth = scale.Px(kTargetPickerWidth);
        ImGuiMCP::SetNextWindowPos({ io->DisplaySize.x - panelOffset, io->DisplaySize.y * 0.5f }, ImGuiMCP::ImGuiCond_Always, { 1.0f, 0.5f });
        ImGuiMCP::SetNextWindowSize({ panelWidth, 0.0f }, ImGuiMCP::ImGuiCond_Always);

        if (!ImGuiMCP::Begin("##slpp_OAMPicker", nullptr, kPanelWindowFlags)) {
            ImGuiMCP::End();
            return;
        }

        DrawPanelHeader(scale, "PICK TARGET");
        SetWindowFontSize(scale.TextPx(UI::Theme::FontSize.body));
        for (std::size_t targetIndex = 0; targetIndex < _targets.size(); ++targetIndex) {
            const auto& target = _targets[targetIndex];
            const std::string label = target.label + (target.isCenter ? " [SCENE]" : "");
            ImGuiMCP::PushID(static_cast<int>(targetIndex));
            if (UI::SelectableButton(label.c_str(), false, 0, { 0.0f, scale.Px(kTargetRowHeight) }))
                OnTargetSelected(a_hud, targetIndex);
            ImGuiMCP::PopID();
        }

        ImGuiMCP::SetWindowFontScale(1.0f);
        ImGuiMCP::End();
    }

    void OffsetAdjustPanel::RenderAdjustmentPanel(SceneHUD& a_hud)
    {
        auto& scale = a_hud.GetScale();
        const auto* io = ImGuiMCP::GetIO();
        const auto& target = _targets[*_selectedTarget];
        const float panelOffset = scale.Px(UI::Theme::Geometry.panelTabWidth + UI::Theme::Geometry.panelTabGap);
        const float panelWidth = scale.Px(kAdjustmentPanelWidth);
        ImGuiMCP::SetNextWindowPos({ io->DisplaySize.x - panelOffset, io->DisplaySize.y * 0.5f }, ImGuiMCP::ImGuiCond_Always, { 1.0f, 0.5f });
        ImGuiMCP::SetNextWindowSize({ panelWidth, 0.0f }, ImGuiMCP::ImGuiCond_Always);

        if (!ImGuiMCP::Begin("##slpp_OAMPanel", nullptr, kPanelWindowFlags)) {
            ImGuiMCP::End();
            return;
        }

        const std::string title = target.label + (target.isCenter ? " [SCENE]" : "");
        DrawPanelHeader(scale, title.c_str());
        ImGuiMCP::Dummy({ 0.0f, scale.Px(UI::Theme::Spacing.sm) });
        SetWindowFontSize(scale.TextPx(UI::Theme::FontSize.body));
        if (UI::CheckboxRow("Adjust Stage Only", "slpp_oamStageOnly", _adjustStageOnly, scale.Factor()))
            OnSetAdjustStageOnly(a_hud, _adjustStageOnly);

        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Separator, UI::Theme::Color.nestedSeparator);
        ImGuiMCP::Dummy({ 0.0f, scale.Px(UI::Theme::Spacing.sm) });
        ImGuiMCP::Separator();
        ImGuiMCP::PopStyleColor();

        for (std::size_t axisIndex = 0; axisIndex < kAxisDefinitions.size(); ++axisIndex) {
            ImGuiMCP::PushID(static_cast<int>(axisIndex));
            bool dragging = false;
            if (OffsetTrack(scale, kAxisDefinitions[axisIndex], _axes[axisIndex], dragging))
                OnSetOffset(a_hud, kAxisDefinitions[axisIndex].coordinate, target.formId, _axes[axisIndex].value);
            if (dragging)
                _draggingAxis = axisIndex;
            else if (_draggingAxis == axisIndex && !ImGuiMCP::IsMouseDown(ImGuiMCP::ImGuiMouseButton_Left))
                _draggingAxis.reset();
            ImGuiMCP::PopID();
        }

        ImGuiMCP::Dummy({ 0.0f, scale.Px(UI::Theme::Spacing.sm) });
        const float horizontalPadding = scale.Px(UI::Theme::Spacing.lg);
        const float availableWidth = ImGuiMCP::GetContentRegionAvail().x;
        ImGuiMCP::SetCursorPosX(ImGuiMCP::GetCursorPosX() + horizontalPadding);
        if (UI::ActionButton("Reset Offsets", availableWidth - horizontalPadding * 2.0f))
            OnResetOffsets(a_hud);
        ImGuiMCP::Dummy({ 0.0f, scale.Px(UI::Theme::Spacing.sm) });

        ImGuiMCP::SetWindowFontScale(1.0f);
        ImGuiMCP::End();
    }
}
