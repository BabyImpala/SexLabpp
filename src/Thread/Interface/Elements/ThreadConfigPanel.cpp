#include "ThreadConfigPanel.h"
#include "Registry/Library.h"
#include "SKSE/Translation.h"

namespace Thread::Interface
{
    using UI::SetWindowFontSize;

    void ThreadConfigPanel::Open(SceneHUD& a_hud)
    {
        _actorStates.clear();
        auto* inst = a_hud.GetThreadInstance();
        if (!inst)
            return;
        for (auto* actor : inst->GetActors()) {
            if (!actor)
                continue;
            _actorStates.push_back({ actor->GetFormID(), actor->IsPlayerRef() });
        }
        // Render() preserves this actor ordering for the lifetime of the panel.
        _sortedActors.clear();
        for (auto* actor : inst->GetActors())
            if (actor)
                _sortedActors.push_back(actor);
        std::ranges::sort(_sortedActors, [](RE::Actor* a, RE::Actor* b) {
            if (a->IsPlayerRef() != b->IsPlayerRef())
                return a->IsPlayerRef();
            return std::string_view{ a->GetDisplayFullName() } < std::string_view{ b->GetDisplayFullName() };
        });
    }

    void ThreadConfigPanel::Close()
    {
        _actorStates.clear();
        _sortedActors.clear();
    }

    // ── Handlers ────────────────────────────────────────────────────────────

    void ThreadConfigPanel::OnRandomScene(SceneHUD& a_hud)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (!inst)
            return;
        const auto* cur = inst->GetActiveScene();
        const auto scenes = inst->GetThreadScenes();
        std::vector<const Registry::Scene*> pool;
        pool.reserve(scenes.size());
        for (auto* s : scenes)
            if (s != cur)
                pool.push_back(s);
        if (pool.empty())
            return;
        const std::string id{ Random::draw(pool)->id };
        Script::DispatchMethodCall(a_hud.GetThreadScript(), "ResetScene",
            a_hud.GetCallback(), RE::BSFixedString{ id.c_str() });
    }

    void ThreadConfigPanel::OnMoveScene(SceneHUD& a_hud)
    {
        if (!a_hud.GetThreadScript())
            return;
        Script::DispatchMethodCall(a_hud.GetThreadScript(), "MoveScene", a_hud.GetCallback());
    }

    void ThreadConfigPanel::OnAutoPlaySet(SceneHUD& a_hud, bool state)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (inst)
            inst->SetThreadProperty<bool>("AutoAdvance", state);
    }

    void ThreadConfigPanel::OnNextPosition(SceneHUD& a_hud, RE::Actor* actor)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (inst && actor)
            inst->SetNextPermutation(actor);
    }

    void ThreadConfigPanel::OnSetExpression(SceneHUD& a_hud, RE::Actor* actor, const Registry::Expression* expr)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (inst && actor && expr)
            inst->SetExpression(actor, expr);
    }

    void ThreadConfigPanel::OnSetVoice(SceneHUD& a_hud, RE::Actor* actor, const Registry::Voice* voice)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (inst && actor && voice)
            inst->SetVoice(actor, voice);
    }

    void ThreadConfigPanel::OnSetActorAlpha(RE::Actor* actor, int alphaInt)
    {
        if (actor)
            actor->SetAlpha(std::clamp(alphaInt, 0, 100) / 100.0f);
    }

    // ── Actor card ──────────────────────────────────────────────────────────

    void ThreadConfigPanel::RenderActorCard(SceneHUD& a_hud, RE::Actor* actor, ActorState& state)
    {
        if (!actor)
            return;
        auto* lib = Registry::Library::GetSingleton();
        auto& scale = a_hud.GetScale();

        constexpr float nestedScale = UI::Theme::Geometry::nestedMenuScale;
        const float contentX = ImGuiMCP::GetCursorPosX();
        const float availableW = ImGuiMCP::GetContentRegionAvail().x;
        const float cardW = availableW * nestedScale;
        const float cardX = contentX + (availableW - cardW) * 0.5f;
        const float rowPadV = scale.Px(6.0f) * nestedScale;
        const float rowPadH = scale.Px(12.0f) * nestedScale;
        const float hdrPadV = scale.Px(5.0f) * nestedScale;
        const float hdrPadH = scale.Px(10.0f) * nestedScale;
        const float fieldGap = scale.Px(UI::Theme::Spacing::sm) * nestedScale;
        const float captionSize = scale.TextPx(UI::Theme::FontSize::caption) * nestedScale;
        const float subsectionHeaderSize = scale.TextPx(UI::Theme::FontSize::subsectionHeader) * nestedScale;

        SetWindowFontSize(captionSize);
        const float labelW = ImGuiMCP::CalcTextSize("Scene Position").x + fieldGap;
        const float fieldX = cardX + rowPadH + labelW;
        const float fieldW = cardW - rowPadH * 2.0f - labelW;
        const float alphaValueW = ImGuiMCP::CalcTextSize("100%").x;
        const float alphaW = fieldW - fieldGap - alphaValueW;

        const auto* style = ImGuiMCP::GetStyle();
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding,
            ImGuiMCP::ImVec2{ style->FramePadding.x * nestedScale, style->FramePadding.y * nestedScale });
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_ItemSpacing,
            ImGuiMCP::ImVec2{ style->ItemSpacing.x * nestedScale, style->ItemSpacing.y * nestedScale });
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_ItemInnerSpacing,
            ImGuiMCP::ImVec2{ style->ItemInnerSpacing.x * nestedScale, style->ItemInnerSpacing.y * nestedScale });
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_GrabMinSize, style->GrabMinSize * nestedScale);

        ImGuiMCP::PushID(static_cast<int>(actor->GetFormID()));

        // ── Card header ─────────────────────────────────────────────────────
        // An invisible-label Selectable provides the real user interaction here.
        // The header's actual content (toggle, name, badge) is drawn on top of it afterward.
        ImGuiMCP::SetCursorPosX(cardX);
        const ImGuiMCP::ImVec2 hdrMin = ImGuiMCP::GetCursorScreenPos();
        const float hdrH = subsectionHeaderSize + hdrPadV * 2.0f;

        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Header, UI::Theme::Color::nestedHeader);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_HeaderHovered, UI::Theme::Color::nestedHeaderHovered);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_HeaderActive, UI::Theme::Color::nestedControlActive);
        if (ImGuiMCP::Selectable("##slpp_tcmCardHdr", false, 0, ImGuiMCP::ImVec2{ cardW, hdrH }))
            state.cardOpen = !state.cardOpen;
        ImGuiMCP::PopStyleColor(3);
        const bool hdrHov = ImGuiMCP::IsItemHovered();

        // Selectable only paints interaction states, so supply the idle header surface.
        auto* dl = ImGuiMCP::GetWindowDrawList();
        if (!hdrHov) {
            const ImGuiMCP::ImVec2 hdrMax{ hdrMin.x + cardW, hdrMin.y + hdrH };
            ImGuiMCP::ImDrawListManager::AddRectFilled(dl, hdrMin, hdrMax, UI::Theme::Color::nestedHeader, 0.0f, 0);
        }

        ImGuiMCP::SetCursorScreenPos(hdrMin);
        SetWindowFontSize(subsectionHeaderSize);

        // Left accent bar to distinguish actor cards from section headers
        const float accentBarW = scale.Px(2.0f) * nestedScale;
        ImGuiMCP::ImDrawListManager::AddRectFilled(dl,
            ImGuiMCP::ImVec2{ hdrMin.x, hdrMin.y },
            ImGuiMCP::ImVec2{ hdrMin.x + accentBarW, hdrMin.y + hdrH },
            hdrHov ? UI::Theme::Color::accent : UI::Theme::Color::borderSubtle, 0.0f, 0);

        // Name at left with padding
        ImGuiMCP::SetCursorScreenPos(ImGuiMCP::ImVec2{ hdrMin.x + hdrPadH, hdrMin.y + hdrPadV });
        ImGuiMCP::TextColored(UI::Theme::ToVec4(hdrHov ? UI::Theme::Color::textSecondary : UI::Theme::Color::textMuted),
            "%s", actor->GetDisplayFullName());

        // Right side: PLAYER badge then toggle icon, both flush-right
        SKSEMenuFramework::PushFont(UI::Theme::Icon::solidFont);
        const char* toggleIcon = state.cardOpen ? UI::Theme::Icon::minus : UI::Theme::Icon::plus;
        const ImGuiMCP::ImVec2 toggleIconSize = ImGuiMCP::CalcTextSize(toggleIcon);
        FontAwesome::Pop();

        const float toggleIconX = hdrMin.x + cardW - hdrPadH - toggleIconSize.x;
        if (actor->IsPlayerRef()) {
            const ImGuiMCP::ImVec2 badgeSize = ImGuiMCP::CalcTextSize("PLAYER");
            const float badgeX = toggleIconX - fieldGap - badgeSize.x;
            ImGuiMCP::SetCursorScreenPos(ImGuiMCP::ImVec2{ badgeX, hdrMin.y + hdrPadV });
            ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color::accent), "PLAYER");
        }
        SKSEMenuFramework::PushFont(UI::Theme::Icon::solidFont);
        ImGuiMCP::SetCursorScreenPos(ImGuiMCP::ImVec2{ toggleIconX, hdrMin.y + hdrPadV });
        ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color::textSecondary), "%s", toggleIcon);
        FontAwesome::Pop();

        ImGuiMCP::SetCursorScreenPos(ImGuiMCP::ImVec2{ hdrMin.x, hdrMin.y + hdrH });

        // ── Card body (collapsible) ─────────────────────────────────────────
        if (!state.cardOpen) {
            ImGuiMCP::SetCursorPosX(contentX);
            ImGuiMCP::PopStyleVar(4);
            ImGuiMCP::PopID();
            return;
        }
        auto* inst = a_hud.GetThreadInstance();
        const ImGuiMCP::ImVec2 bodyMin{ hdrMin.x, hdrMin.y + hdrH };
        ImGuiMCP::ImDrawListManager::ChannelsSplit(dl, 2);
        ImGuiMCP::ImDrawListManager::ChannelsSetCurrent(dl, 1);

        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBg, UI::Theme::Color::nestedControl);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBgHovered, UI::Theme::Color::nestedControlHovered);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBgActive, UI::Theme::Color::nestedControlActive);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, UI::Theme::Color::nestedControl);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, UI::Theme::Color::nestedControlHovered);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, UI::Theme::Color::nestedControlActive);
        SetWindowFontSize(scale.TextPx(UI::Theme::FontSize::compact) * nestedScale);

        // ── Expression combo
        if (Registry::RaceKey(actor).Is(Registry::RaceKey::Value::Human)) {
            const auto* curExpr = inst->GetExpression(actor);
            std::string curLabel = curExpr ? curExpr->GetId().c_str() : "(none)";
            SKSE::Translation::Translate(curLabel, curLabel);

            ImGuiMCP::SetCursorPosX(cardX + rowPadH);
            SetWindowFontSize(captionSize);
            ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color::textMuted), "Expression");
            ImGuiMCP::SameLine(fieldX);

            SetWindowFontSize(captionSize);
            ImGuiMCP::SetNextItemWidth(fieldW);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PopupBg, UI::Theme::Color::nestedPopup);
            if (ImGuiMCP::BeginCombo("##slpp_tcmExpr", curLabel.c_str())) {
                lib->ForEachExpression([&](const auto& expr) {
                    if (!expr.enabled)
                        return false;
                    std::string label{ expr.GetId().c_str() };
                    SKSE::Translation::Translate(label, label);
                    const bool sel = curExpr && curExpr->GetId() == expr.GetId();
                    if (sel)
                        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, UI::Theme::Color::selectionText);
                    SetWindowFontSize(captionSize);
                    if (ImGuiMCP::Selectable(label.c_str(), sel)) {
                        OnSetExpression(a_hud, actor, &expr);
                    }
                    if (sel)
                        ImGuiMCP::PopStyleColor();
                    return false;
                });
                ImGuiMCP::EndCombo();
            }
            ImGuiMCP::PopStyleColor();
            ImGuiMCP::SetCursorPosY(ImGuiMCP::GetCursorPosY() + rowPadV);
        }

        // ── Voice combo
        {
            const auto* curVoice = inst->GetVoice(actor);
            std::string curLabel = curVoice ? curVoice->GetId().c_str() : "(none)";
            SKSE::Translation::Translate(curLabel, curLabel);
            const Registry::RaceKey raceKey{ actor };

            ImGuiMCP::SetCursorPosX(cardX + rowPadH);
            SetWindowFontSize(captionSize);
            ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color::textMuted), "Voice");
            ImGuiMCP::SameLine(fieldX);

            SetWindowFontSize(captionSize);
            ImGuiMCP::SetNextItemWidth(fieldW);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PopupBg, UI::Theme::Color::nestedPopup);
            if (ImGuiMCP::BeginCombo("##slpp_tcmVoice", curLabel.c_str())) {
                lib->ForEachVoice([&](const auto& v) {
                    if (!v.HasRace(raceKey))
                        return false;
                    std::string label{ v.GetId().c_str() };
                    SKSE::Translation::Translate(label, label);
                    const bool sel = curVoice && curVoice->GetId() == v.GetId();
                    if (sel)
                        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, UI::Theme::Color::selectionText);
                    SetWindowFontSize(captionSize);
                    if (ImGuiMCP::Selectable(label.c_str(), sel)) {
                        OnSetVoice(a_hud, actor, &v);
                    }
                    if (sel)
                        ImGuiMCP::PopStyleColor();
                    return false;
                });
                ImGuiMCP::EndCombo();
            }
            ImGuiMCP::PopStyleColor();
            ImGuiMCP::SetCursorPosY(ImGuiMCP::GetCursorPosY() + rowPadV);
        }

        // ── Alpha slider
        {
            int alphaInt = static_cast<int>(std::round(actor->GetAlpha() * 100.0f));

            ImGuiMCP::SetCursorPosX(cardX + rowPadH);
            SetWindowFontSize(captionSize);
            ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color::textMuted), "Alpha");
            ImGuiMCP::SameLine(fieldX);

            ImGuiMCP::SetNextItemWidth(alphaW);
            ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding,
                ImGuiMCP::ImVec2{ 0.0f, scale.Px(1.5f) * nestedScale });
            if (ImGuiMCP::SliderInt("##slpp_tcmAlpha", &alphaInt, 0, 100, ""))
                OnSetActorAlpha(actor, alphaInt);  // actor's opacity updates live while dragging
            ImGuiMCP::PopStyleVar();

            ImGuiMCP::SameLine(0.0f, fieldGap);
            SetWindowFontSize(captionSize);
            ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color::textMuted), "%d%%", alphaInt);
        }

        // ── Scene position row
        {
            const int32_t current = inst->GetCurrentPermutation(actor);
            const int32_t total = inst->GetUniquePermutations(actor);
            const bool canCycle = total > 1;

            if (canCycle) {
                ImGuiMCP::SetCursorPosY(ImGuiMCP::GetCursorPosY() + rowPadV);
                ImGuiMCP::SetCursorPosX(cardX + rowPadH);
                SetWindowFontSize(captionSize);
                ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color::textMuted), "Scene Position");
                ImGuiMCP::SameLine(fieldX);

                const float btnSize = std::max(scale.Px(18.0f) * nestedScale,
                    captionSize + scale.Px(UI::Theme::Spacing::xxs) * nestedScale);
                SetWindowFontSize(captionSize);
                char permBuf[24];
                std::snprintf(permBuf, sizeof(permBuf), "%d of %d", current, total);
                ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color::textSecondary), "%s", permBuf);

                ImGuiMCP::SameLine(0.0f, scale.Px(UI::Theme::Spacing::lg) * nestedScale);
                SKSEMenuFramework::PushFont(UI::Theme::Icon::solidFont);
                const bool nextPosition = ImGuiMCP::Button(UI::Theme::Icon::nextPerm, ImGuiMCP::ImVec2{ btnSize, btnSize });
                FontAwesome::Pop();
                if (ImGuiMCP::IsItemHovered())
                    ImGuiMCP::SetTooltip("Move actor to the next compatible scene position");
                if (nextPosition)
                    OnNextPosition(a_hud, actor);
            }
        }

        ImGuiMCP::SetCursorPosY(ImGuiMCP::GetCursorPosY() + rowPadV);
        ImGuiMCP::PopStyleColor(6);

        const ImGuiMCP::ImVec2 bodyMax{ hdrMin.x + cardW, ImGuiMCP::GetCursorScreenPos().y };
        ImGuiMCP::ImDrawListManager::ChannelsSetCurrent(dl, 0);
        ImGuiMCP::ImDrawListManager::AddRectFilled(
            dl, bodyMin, bodyMax, UI::Theme::Color::nestedSurface, 0.0f, 0);
        ImGuiMCP::ImDrawListManager::ChannelsMerge(dl);

        ImGuiMCP::SetCursorPosX(contentX);
        ImGuiMCP::PopStyleVar(4);
        ImGuiMCP::PopID();
    }

    // ── Render ──────────────────────────────────────────────────────────────

    void ThreadConfigPanel::Render(SceneHUD& a_hud)
    {
        auto* inst = a_hud.GetThreadInstance();
        if (!inst)
            return;

        auto& scale = a_hud.GetScale();
        auto* io = ImGuiMCP::GetIO();
        const float panelW = scale.Px(280.0f);
        const float offset = scale.Px(UI::Theme::Geometry::panelTabWidth + UI::Theme::Geometry::panelTabGap);
        const float rowMinH = scale.Px(28.0f);
        const float rowPadV = scale.Px(6.0f);
        const float rowPadH = scale.Px(12.0f);
        const float maxBodyH = scale.Px(340.0f);  // before scrolling
        const float sectionH = std::max(scale.Px(20.0f),
            scale.TextPx(UI::Theme::FontSize::sectionHeader) + scale.Px(UI::Theme::Spacing::xs));

        ImGuiMCP::SetNextWindowPos(
            ImGuiMCP::ImVec2{ io->DisplaySize.x - offset, io->DisplaySize.y * 0.5f },
            ImGuiMCP::ImGuiCond_Always, ImGuiMCP::ImVec2{ 1.0f, 0.5f });
        ImGuiMCP::SetNextWindowSizeConstraints(
            ImGuiMCP::ImVec2{ panelW, rowMinH },
            ImGuiMCP::ImVec2{ panelW, io->DisplaySize.y * 0.8f });
        ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2{ panelW, 0.0f }, ImGuiMCP::ImGuiCond_Always);

        constexpr auto kFlags =
            ImGuiMCP::ImGuiWindowFlags_NoTitleBar | ImGuiMCP::ImGuiWindowFlags_NoResize |
            ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoScrollbar |
            ImGuiMCP::ImGuiWindowFlags_NoCollapse | ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize;

        if (!ImGuiMCP::Begin("##slpp_TCM", nullptr, kFlags)) {
            ImGuiMCP::End();
            return;
        }

        // ── THREAD section
        SetWindowFontSize(scale.TextPx(UI::Theme::FontSize::sectionHeader));
        if (UI::CollapsibleSectionHeader(
                "THREAD", "##slpp_tcmThreadSection", _threadSectionOpen, { 0.0f, sectionH }))
            _threadSectionOpen = !_threadSectionOpen;
        ImGuiMCP::Separator();

        if (_threadSectionOpen) {
            ImGuiMCP::SetCursorPosY(ImGuiMCP::GetCursorPosY() + scale.Px(UI::Theme::Spacing::xs));
            SetWindowFontSize(scale.TextPx(UI::Theme::FontSize::body));
            const float actionGap = scale.Px(UI::Theme::Spacing::sm);
            const float actionW = (panelW - rowPadH * 2.0f - actionGap) * 0.5f;
            ImGuiMCP::SetCursorPosX(rowPadH);
            if (UI::ActionButton("Random Scene", actionW))
                OnRandomScene(a_hud);
            ImGuiMCP::SameLine(0.0f, actionGap);
            if (UI::ActionButton("Move Scene", actionW))
                OnMoveScene(a_hud);
            ImGuiMCP::Dummy(ImGuiMCP::ImVec2{ 0.0f, scale.Px(UI::Theme::Spacing::sm) });

            ImGuiMCP::SetCursorPosX(rowPadH);
            ImGuiMCP::TextColored(UI::Theme::ToVec4(UI::Theme::Color::textMuted), "Auto Advance");
            ImGuiMCP::SameLine(panelW - rowPadH - scale.Px(20.0f));
            bool autoPlay = inst->GetThreadProperty<bool>("AutoAdvance");
            UI::PushCheckboxStyle(scale.Factor());
            if (ImGuiMCP::Checkbox("##slpp_tcmAutoplay", &autoPlay)) {
                OnAutoPlaySet(a_hud, autoPlay);
            }
            UI::PopCheckboxStyle();

            ImGuiMCP::Dummy(ImGuiMCP::ImVec2{ 0.0f, rowPadV });
        }

        ImGuiMCP::Separator();

        // ── ACTORS section
        SetWindowFontSize(scale.TextPx(UI::Theme::FontSize::sectionHeader));
        if (UI::CollapsibleSectionHeader(
                "ACTORS", "##slpp_tcmActorsSection", _actorsSectionOpen, { 0.0f, sectionH }))
            _actorsSectionOpen = !_actorsSectionOpen;
        ImGuiMCP::Separator();

        if (_actorsSectionOpen) {
            ImGuiMCP::SetNextWindowSizeConstraints(
                ImGuiMCP::ImVec2{ 0.0f, 0.0f }, ImGuiMCP::ImVec2{ FLT_MAX, maxBodyH });
            ImGuiMCP::BeginChild("##slpp_tcmActors", ImGuiMCP::ImVec2{ -FLT_MIN, 0.0f },
                ImGuiMCP::ImGuiChildFlags_AutoResizeY, ImGuiMCP::ImGuiWindowFlags_None);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Separator, UI::Theme::Color::nestedSeparator);

            bool first = true;
            for (auto* actor : _sortedActors) {
                const uint32_t fid = actor->GetFormID();
                ActorState* st = nullptr;
                for (auto& state : _actorStates)
                    if (state.formId == fid) {
                        st = &state;
                        break;
                    }
                if (!st) {
                    _actorStates.push_back({ fid, false });
                    st = &_actorStates.back();
                }
                if (!first)
                    ImGuiMCP::Separator();
                first = false;
                RenderActorCard(a_hud, actor, *st);
            }

            ImGuiMCP::PopStyleColor();
            ImGuiMCP::EndChild();
        }

        ImGuiMCP::SetWindowFontScale(1.0f);
        ImGuiMCP::End();
    }
}
