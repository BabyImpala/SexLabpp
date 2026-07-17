#include "ElementCtrlPanel.h"
#include "Papyrus/SexLabUtil.h"
#include "Thread/Interface/SceneHUD.h"

namespace Thread::Interface
{
    using UI::SetWindowFontSize;

    void ElementCtrlPanel::Open(SceneHUD& a_hud)
    {
        if (auto* inst = a_hud.GetThreadInstance()) {
            _scaleAdjustment = std::clamp(inst->GetThreadProperty<float>("VarUI_MenuScaleMult"), 0.5f, 2.5f);
            const float textScale = inst->GetThreadProperty<float>("VarUI_TextScaleMult");
            _textScaleAdjustment = textScale > 0.0f ? std::clamp(textScale, 0.75f, 2.0f) : 1.0f;
        }
    }
    void ElementCtrlPanel::Close() {}

    void ElementCtrlPanel::OnScaleChange(SceneHUD& a_hud, float a_value)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (!inst)
            return;
        const float value = std::clamp(a_value, 0.5f, 2.5f);
        _scaleAdjustment = value;
        inst->SetThreadProperty<float>("VarUI_MenuScaleMult", value);
        a_hud.GetScale().SetMultiplier(value);
    }

    void ElementCtrlPanel::OnTextScaleChange(SceneHUD& a_hud, float a_value)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (!inst)
            return;
        const float value = std::clamp(a_value, 0.75f, 2.0f);
        _textScaleAdjustment = value;
        inst->SetThreadProperty<float>("VarUI_TextScaleMult", value);
        a_hud.GetScale().SetTextMultiplier(value);
    }

    void ElementCtrlPanel::Render(SceneHUD& a_hud)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (!inst)
            return;
        auto& scale = a_hud.GetScale();

        auto* io = ImGuiMCP::GetIO();
        const float panelOffset = scale.Px(UI::Theme::Geometry::panelTabWidth + UI::Theme::Geometry::panelTabGap);
        ImGuiMCP::SetNextWindowPos(
            ImGuiMCP::ImVec2{ io->DisplaySize.x - panelOffset, io->DisplaySize.y * 0.5f },
            ImGuiMCP::ImGuiCond_Always, ImGuiMCP::ImVec2{ 1.0f, 0.5f });
        ImGuiMCP::SetNextWindowSize({ scale.Px(220.0f), 0.0f }, ImGuiMCP::ImGuiCond_Always);

        constexpr auto kFlags =
            ImGuiMCP::ImGuiWindowFlags_NoTitleBar | ImGuiMCP::ImGuiWindowFlags_NoResize |
            ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoCollapse |
            ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize;

        if (!ImGuiMCP::Begin("Elements##slpp_ECM", nullptr, kFlags)) {
            ImGuiMCP::End();
            return;
        }
        SetWindowFontSize(scale.TextPx(UI::Theme::FontSize::body));
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, UI::Theme::ToVec4(UI::Theme::Color::textSecondary));

        ImGuiMCP::SetNextItemWidth(-1.0f);
        ImGuiMCP::SliderFloat("##slpp_ecmScale", &_scaleAdjustment, 0.5f, 2.5f, "UI Scale %.2fx");
        if (ImGuiMCP::IsItemDeactivatedAfterEdit())
            OnScaleChange(a_hud, _scaleAdjustment);
        ImGuiMCP::SetNextItemWidth(-1.0f);
        ImGuiMCP::SliderFloat("##slpp_ecmTextScale", &_textScaleAdjustment, 0.5f, 2.5f, "Text Scale %.2fx");
        if (ImGuiMCP::IsItemDeactivatedAfterEdit())
            OnTextScaleChange(a_hud, _textScaleAdjustment);

        ImGuiMCP::PopStyleColor();
        ImGuiMCP::Separator();

        // Elements
        SetWindowFontSize(scale.TextPx(UI::Theme::FontSize::body));
        UI::PushCheckboxStyle(scale.Factor());
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, UI::Theme::ToVec4(UI::Theme::Color::textSecondary));
        
        const float toggleRowH = scale.Px(24.0f);
        const float rowPadH = scale.Px(12.0f);
        const float availW = ImGuiMCP::GetContentRegionAvail().x - rowPadH * 2.0f;
        const float cbSize = ImGuiMCP::GetFrameHeight();

        auto DrawToggleRow = [&](const char* label, const char* id, bool& state, auto onChange) {
            const ImGuiMCP::ImVec2 toggleRowMin = ImGuiMCP::GetCursorScreenPos();
            
            // Full row selectable button
            ImGuiMCP::SetCursorScreenPos({ toggleRowMin.x + rowPadH, toggleRowMin.y });
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_HeaderHovered, UI::Theme::ToVec4(UI::Theme::Color::transparent));
            std::string rowId = std::string("##slpp_ecp_row_") + id;
            if (UI::SelectableButton(rowId.c_str(), false, 0, ImGuiMCP::ImVec2{ availW, toggleRowH })) {
                state = !state;
                onChange(state);
            }
            ImGuiMCP::PopStyleColor();
            
            // Checkbox on the right
            const float cbX = toggleRowMin.x + rowPadH + availW - cbSize;
            const float cbY = toggleRowMin.y + (toggleRowH - cbSize) * 0.5f;
            ImGuiMCP::SetCursorScreenPos({ cbX, cbY });
            std::string cbId = std::string("##slpp_ecp_cb_") + id;
            if (ImGuiMCP::Checkbox(cbId.c_str(), &state)) {
                onChange(state);
            }
            
            // Label on the left
            const ImGuiMCP::ImVec2 labelSize = ImGuiMCP::CalcTextSize(label);
            const float labelY = toggleRowMin.y + (toggleRowH - labelSize.y) * 0.5f;
            ImGuiMCP::SetCursorScreenPos({ toggleRowMin.x + rowPadH, labelY });
            ImGuiMCP::TextUnformatted(label);

            // Advance cursor for next item
            ImGuiMCP::SetCursorScreenPos({ toggleRowMin.x, toggleRowMin.y + toggleRowH });
        };

        bool state_gameHud = inst->GetThreadProperty<bool>("ElementUI_GameHUD");
        DrawToggleRow("Game HUD", "gameHud", state_gameHud, [&](bool val) {
            inst->SetThreadProperty<bool>("ElementUI_GameHUD", val);
            Papyrus::SexLabUtil::HideElementsGameHUD(nullptr, !val);
        });
        
        bool state_AnimSpeed = inst->GetThreadProperty<bool>("ElementUI_AnimSpeed");
        DrawToggleRow("Anim Speed Overlay", "animSpeed", state_AnimSpeed, [&](bool val) {
            inst->SetThreadProperty<bool>("ElementUI_AnimSpeed", val);
        });

        bool state_EnjBars = inst->GetThreadProperty<bool>("ElementUI_EnjBars");
        DrawToggleRow("Enj Bars Overlay", "enjBars", state_EnjBars, [&](bool val) {
            inst->SetThreadProperty<bool>("ElementUI_EnjBars", val);
        });

        struct Row
        {
            const char* label;
            const char* property;
            PanelId panel;
        };
        constexpr std::array panelRows{
            Row{ "Thread Config Panel", "ElementUI_ThreadConfig", PanelId::kThreadConfig },
            Row{ "Scene Select Panel", "ElementUI_SceneSelect", PanelId::kSceneSelect },
            Row{ "Offset Adjust Panel", "ElementUI_OffsetAdjust", PanelId::kOffsetAdjust },
        };
        for (const auto& row : panelRows) {
            bool state = inst->GetThreadProperty<bool>(row.property);
            DrawToggleRow(row.label, row.property, state, [&](bool val) {
                inst->SetThreadProperty<bool>(row.property, val);
                if (!val && a_hud.IsPanelOpen(row.panel))
                    a_hud.CloseAllPanels();
            });
        }
        ImGuiMCP::PopStyleColor();
        UI::PopCheckboxStyle();
        ImGuiMCP::SetWindowFontScale(1.0f);
        ImGuiMCP::End();
    }
}