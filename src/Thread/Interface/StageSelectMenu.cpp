#include "StageSelectMenu.h"
#include "SceneHUD.h"

namespace Thread::Interface
{
    using UI::DrawTextShadowed;
    using UI::SetWindowFontSize;

    StageSelectMenu& StageSelectMenu::GetSingleton()
    {
        static StageSelectMenu singleton;
        return singleton;
    }

    bool StageSelectMenu::Register()
    {
        if (!RegisterWindow(RenderCallback, false)) {
            logger::error("StageSelectMenu::Register >> AddWindow failed");
            return false;
        }
        return true;
    }

    // ──────── Native entry points

    bool StageSelectMenu::OpenStageSelectMenu(RE::TESQuest* a_quest)
    {
        if (!a_quest)
            return false;
        auto* inst = Instance::GetInstance(a_quest);
        if (!inst)
            return false;

        const auto* scene = inst->GetActiveScene();
        const auto* stage = inst->GetActiveStage();
        if (!scene || !stage)
            return false;

        const auto* adjacent = scene->GetAdjacentStages(stage);
        if (!adjacent || adjacent->size() < 2) {
            return false;
        }

        _choices.clear();
        _choices.reserve(adjacent->size());
        int choiceIdx = 1;
        for (const auto* next : *adjacent) {
            std::string base = !next->navtext.empty() ? next->navtext : next->id;
            std::string label = std::to_string(choiceIdx++) + ".  " + base;
            _choices.push_back({ next, std::move(label) });
        }
        _choiceOrigin = stage;
        _choicePending = true;
        _linkedThread = a_quest;
        SetBlocksInput(true);
        Show();
        return true;
    }

    void StageSelectMenu::SetVisibilitySceneGraph(RE::TESQuest* a_quest, bool a_open)
    {
        if (!a_quest || _graphOpen == a_open)
            return;

        if (a_open) {
            _graphOpen = true;
            _linkedThread = a_quest;
            if (auto* inst = Instance::GetInstance(a_quest))
                BuildSceneGraph(*inst);
            _graphFitPending = true;
            SetBlocksInput(true);
            Show();
        } else {
            CloseGraphView();
        }
    }

    void StageSelectMenu::RefreshSceneGraphView(RE::TESQuest* a_quest)
    {
        if (!a_quest)
            return;
        auto* inst = Instance::GetInstance(a_quest);
        if (!inst)
            return;

        if (_choicePending && inst->GetActiveStage() != _choiceOrigin)
            ClosePendingChoice();

        if (_graphOpen) {
            if (inst->GetActiveScene() != _graphScene)
                BuildSceneGraph(*inst);
            else if (const auto* activeStage = inst->GetActiveStage()) {
                const auto it = std::ranges::find(_graphNodes, activeStage, &GraphNode::stage);
                _graphCurrentIndex = it != _graphNodes.end() ? static_cast<int>(std::distance(_graphNodes.begin(), it)) : -1;
            }
        }
    }

    void StageSelectMenu::Close()
    {
        _choicePending = false;
        _choices.clear();
        _choiceOrigin = nullptr;
        if (_graphOpen) {
            _graphOpen = false;
        }
        SetBlocksInput(false);
        Hide();
    }

    // ── Internals

    namespace
    {
        constexpr float kColumnSpacing = 88.0f;
        constexpr float kRowSpacing = 36.0f;
        constexpr float kNodeRadius = 6.0f;
    }

    void StageSelectMenu::HideIfIdle()
    {
        if (_choicePending || _graphOpen)
            return;
        SetBlocksInput(false);
        Hide();
    }

    void StageSelectMenu::ClosePendingChoice()
    {
        _choicePending = false;
        _choices.clear();
        _choiceOrigin = nullptr;
        HideIfIdle();
    }

    void StageSelectMenu::CloseGraphView()
    {
        _graphOpen = false;
        HideIfIdle();
    }

    void StageSelectMenu::OnNextBranchSelected(int a_index)
    {
        const auto script = Script::GetScriptObject(_linkedThread, "sslThreadController");
        Script::DispatchMethodCall(script, "BranchTo", Script::CallbackPtr{}, static_cast<int>(a_index));
        ClosePendingChoice();
    }

    void StageSelectMenu::BuildSceneGraph(Instance& a_inst)
    {
        _graphNodes.clear();
        _graphEdges.clear();
        _graphCurrentIndex = -1;
        _graphFitPending = true;

        const auto* scene = a_inst.GetActiveScene();
        if (!scene)
            return;
        _graphScene = scene;

        const auto allStages = scene->GetAllStages();
        if (allStages.empty())
            return;

        std::unordered_map<const Registry::Stage*, int> indexOf;
        indexOf.reserve(allStages.size());
        _graphNodes.reserve(allStages.size());
        for (const auto* s : allStages) {
            indexOf.emplace(s, static_cast<int>(_graphNodes.size()));
            _graphNodes.push_back(GraphNode{ s, 0.0f, 0.0f, -1 });
        }

        // BFS layering from the scene's start stage; determines column (x) placement.
        std::vector<int> layerOf(_graphNodes.size(), -1);
        std::vector<int> queue;
        queue.reserve(_graphNodes.size());

        const auto* root = scene->GetStageByID(RE::BSFixedString{});
        if (root) {
            if (const auto it = indexOf.find(root); it != indexOf.end()) {
                layerOf[it->second] = 0;
                queue.push_back(it->second);
            }
        }

        for (size_t head = 0; head < queue.size(); ++head) {
            const int cur = queue[head];
            const auto* curStage = _graphNodes[cur].stage;
            const auto* adjacent = scene->GetAdjacentStages(curStage);
            if (!adjacent)
                continue;
            for (const auto* next : *adjacent) {
                const auto it = indexOf.find(next);
                if (it == indexOf.end())
                    continue;
                const int nextIdx = it->second;
                _graphEdges.push_back(GraphEdge{ cur, nextIdx });
                if (layerOf[nextIdx] == -1) {
                    layerOf[nextIdx] = layerOf[cur] + 1;
                    queue.push_back(nextIdx);
                }
            }
        }

        // Anything BFS never reached (disconnected islands) still gets drawn, in its own lane.
        int maxLayer = 0;
        for (const int l : layerOf)
            maxLayer = std::max(maxLayer, l);
        for (auto& l : layerOf) {
            if (l == -1)
                l = ++maxLayer;
        }

        std::unordered_map<int, int> rowsInLayer;
        for (size_t i = 0; i < _graphNodes.size(); ++i) {
            const int layer = layerOf[i];
            const int row = rowsInLayer[layer]++;
            _graphNodes[i].layer = layer;
            _graphNodes[i].x = static_cast<float>(layer) * kColumnSpacing;
            _graphNodes[i].y = static_cast<float>(row) * kRowSpacing;
        }
        std::unordered_map<int, float> layerHeight;
        for (auto&& [layer, count] : rowsInLayer)
            layerHeight[layer] = static_cast<float>(count - 1) * kRowSpacing;
        for (auto& node : _graphNodes)
            node.y -= layerHeight[node.layer] * 0.5f;

        if (const auto* active = a_inst.GetActiveStage()) {
            if (const auto it = indexOf.find(active); it != indexOf.end())
                _graphCurrentIndex = it->second;
        }
        _graphShowLabels = _graphNodes.size() < 20;
    }

    // ── Render

    void __stdcall StageSelectMenu::RenderCallback()
    {
        GetSingleton().Render();
    }

    void StageSelectMenu::Render()
    {
        if (!IsVisible())
            return;
        if (!_choicePending && !_graphOpen)
            return;
        if (!_linkedThread)
            return;

        if (_graphOpen)
            RenderSceneGraphView();
        if (_choicePending && !_graphOpen)
            RenderStageChoiceCard();
    }

    void StageSelectMenu::RenderStageChoiceCard()
    {
        auto& scale = SceneHUD::GetSingleton().GetScale();
        auto* io = ImGuiMCP::GetIO();
        const float dh = io->DisplaySize.y;

        const float fontSize = scale.TextPx(UI::Theme::FontSize.body);
        const float padV = scale.Px(5.0f);
        const float accentW = scale.Px(3.0f);
        const float padLeft = scale.Px(10.0f);
        const float rowH = fontSize + padV * 2.0f;
        const float maxW = io->DisplaySize.x * 0.20f;   // hard 20% width cap
        const float scrollSpd = scale.Px(20.0f);        // pixels per second for marquee

        const float titleFontSize = scale.TextPx(UI::Theme::FontSize.sectionHeader);
        const float titleH = titleFontSize + padV * 2.0f;

        const int rowCount = static_cast<int>(_choices.size());
        const float totalH = titleH + rowH * rowCount;
        const float winY = (dh - totalH) * 0.5f;

        ImGuiMCP::SetNextWindowPos(ImGuiMCP::ImVec2{ 0.0f, winY }, ImGuiMCP::ImGuiCond_Always);
        ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2{ maxW, totalH }, ImGuiMCP::ImGuiCond_Always);
        ImGuiMCP::SetNextWindowBgAlpha(0.0f);

        constexpr auto kFlags =
            ImGuiMCP::ImGuiWindowFlags_NoTitleBar | ImGuiMCP::ImGuiWindowFlags_NoResize |
            ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoCollapse |
            ImGuiMCP::ImGuiWindowFlags_NoScrollbar | ImGuiMCP::ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiMCP::ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiMCP::ImGuiWindowFlags_NoDecoration;

        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_WindowPadding, ImGuiMCP::ImVec2{ 0.0f, 0.0f });
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_WindowBg, ImGuiMCP::ImVec4{ 0, 0, 0, 0 });
        if (!ImGuiMCP::Begin("##slpp_StageMenuChoice", nullptr, kFlags)) {
            ImGuiMCP::End();
            ImGuiMCP::PopStyleColor(1);
            ImGuiMCP::PopStyleVar(2);
            return;
        }

        auto* dl = ImGuiMCP::GetWindowDrawList();
        const ImGuiMCP::ImVec2 winPos = ImGuiMCP::GetWindowPos();
        const float time = static_cast<float>(ImGuiMCP::GetTime());

        SetWindowFontSize(titleFontSize);
        DrawTextShadowed(dl,
            ImGuiMCP::ImVec2{ winPos.x + padLeft, winPos.y + (titleH - titleFontSize) * 0.5f },
            UI::Theme::Color.textSecondary, "Choose Next Stage");
        SetWindowFontSize(fontSize);

        std::optional<int> clickedIndex;
        for (int i = 0; i < rowCount; ++i) {
            const auto& choice = _choices[i];
            ImGuiMCP::PushID(i);

            const float rowY = winPos.y + titleH + rowH * i;
            const ImGuiMCP::ImVec2 rowMin{ winPos.x, rowY };
            const ImGuiMCP::ImVec2 rowMax{ winPos.x + maxW, rowY + rowH };

            // Hit-test
            ImGuiMCP::SetCursorScreenPos(rowMin);
            ImGuiMCP::InvisibleButton("##slpp_ssmRow", ImGuiMCP::ImVec2{ maxW, rowH });
            const bool hov = ImGuiMCP::IsItemHovered();
            if (ImGuiMCP::IsItemClicked())
                clickedIndex = i;

            // Left accent bar that fades to the right
            const ImGuiMCP::ImU32 accentCol = hov ? UI::Theme::Color.accent : UI::Theme::Color.borderSubtle;
            const ImGuiMCP::ImU32 accentFade = (accentCol & 0x00FFFFFFu); // same color, alpha=0
            ImGuiMCP::ImDrawListManager::AddRectFilledMultiColor(dl,
                rowMin, ImGuiMCP::ImVec2{ rowMin.x + accentW, rowMax.y },
                accentCol, accentFade, accentFade, accentCol);

            // Clip text to row width
            ImGuiMCP::ImDrawListManager::PushClipRect(dl, rowMin, rowMax, true);

            // Marquee scroll when hovered and text overflows
            const std::string& lbl = choice.label;
            const float textW = ImGuiMCP::CalcTextSize(lbl.c_str()).x;
            const float textX_base = rowMin.x + accentW + padLeft;
            const float availW = maxW - accentW - padLeft;
            float textX = textX_base;
            if (hov && textW > availW) {
                // pause 1s then scroll, reset when no longer hovered
                float scroll = std::max(0.0f, (time - 1.0f)) * scrollSpd;
                scroll = std::fmod(scroll, textW + availW * 0.5f);
                textX = textX_base - scroll;
            }
            const float textY = rowY + (rowH - fontSize) * 0.5f;
            DrawTextShadowed(dl,
                ImGuiMCP::ImVec2{ textX, textY },
                hov ? UI::Theme::Color.textPrimary : UI::Theme::Color.textSecondary,
                lbl.c_str());

            ImGuiMCP::ImDrawListManager::PopClipRect(dl);
            ImGuiMCP::PopID();
        }

        // Number-key shortcuts (1-9) + Esc selects first option
        if (!clickedIndex) {
            for (int i = 0; i < rowCount && i < 9; ++i) {
                const auto key = static_cast<ImGuiMCP::ImGuiKey>(static_cast<int>(ImGuiMCP::ImGuiKey_1) + i);
                if (ImGuiMCP::IsKeyPressed(key, false)) {
                    clickedIndex = i;
                    break;
                }
            }
        }
        if (!clickedIndex && rowCount > 0 && ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false))
            clickedIndex = 0;

        ImGuiMCP::SetWindowFontScale(1.0f);
        ImGuiMCP::End();
        ImGuiMCP::PopStyleColor(1);
        ImGuiMCP::PopStyleVar(2);

        if (clickedIndex)
            OnNextBranchSelected(*clickedIndex);
    }

    void StageSelectMenu::RenderSceneGraphView()
    {
        auto& scale = SceneHUD::GetSingleton().GetScale();
        auto* io = ImGuiMCP::GetIO();
        const float dw = io->DisplaySize.x;
        const float dh = io->DisplaySize.y;

        const float margin = scale.Px(100.0f);
        const float rawW = dw - margin * 2.0f;
        const float rawH = dh - margin * 2.0f;
        constexpr float kPanelShrink = 0.9f;
        const float viewW = rawW * kPanelShrink;
        const float viewH = rawH * kPanelShrink;
        const float originX = margin + (rawW - viewW) * 0.5f;
        const float originY = margin + (rawH - viewH) * 0.5f;

        ImGuiMCP::SetNextWindowPos(ImGuiMCP::ImVec2{ originX, originY }, ImGuiMCP::ImGuiCond_Always);
        ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2{ viewW, viewH }, ImGuiMCP::ImGuiCond_Always);
        ImGuiMCP::SetNextWindowBgAlpha(0.0f);

        constexpr auto kFlags =
            ImGuiMCP::ImGuiWindowFlags_NoTitleBar | ImGuiMCP::ImGuiWindowFlags_NoResize |
            ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoScrollbar |
            ImGuiMCP::ImGuiWindowFlags_NoCollapse | ImGuiMCP::ImGuiWindowFlags_NoBackground |
            ImGuiMCP::ImGuiWindowFlags_NoFocusOnAppearing;

        if (!ImGuiMCP::Begin("##slpp_StageMenuGraph", nullptr, kFlags)) {
            ImGuiMCP::End();
            return;
        }

        auto* dl = ImGuiMCP::GetWindowDrawList();
        const ImGuiMCP::ImVec2 winPos = ImGuiMCP::GetWindowPos();

        const float rounding = scale.Px(UI::Theme::Geometry.roundingPanel + 4.0f);
        ImGuiMCP::ImDrawListManager::AddRectFilled(dl, winPos, ImGuiMCP::ImVec2{ winPos.x + viewW, winPos.y + viewH }, UI::Theme::Color.panelBackground, rounding, 0);
        ImGuiMCP::ImDrawListManager::AddRect(dl, winPos, ImGuiMCP::ImVec2{ winPos.x + viewW, winPos.y + viewH }, UI::Theme::Color.panelBorder, rounding, 0, scale.Px(UI::Theme::Geometry.borderThin));

        // Header row: labels toggle top-left, close top-right, scene name centered.
        const float headerH = scale.Px(28.0f);

        const float btnFontSize = scale.TextPx(UI::Theme::FontSize.caption);
        SetWindowFontSize(btnFontSize);

        const char* closeLabel = "Close";
        const ImGuiMCP::ImVec2 closeSz = ImGuiMCP::CalcTextSize(closeLabel);
        const float closeBtnW = closeSz.x + scale.Px(10.0f);
        const float btnPadTop = scale.Px(4.0f);
        const float btnRounding = scale.Px(UI::Theme::Geometry.roundingSmall + 2.0f);
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FrameRounding, btnRounding);

        const char* lblToggleLabel = _graphShowLabels ? "Labels: On" : "Labels: Off";
        const float lblToggleW = ImGuiMCP::CalcTextSize(lblToggleLabel).x + scale.Px(10.0f);
        ImGuiMCP::SetCursorScreenPos(ImGuiMCP::ImVec2{ winPos.x + scale.Px(8.0f), winPos.y + btnPadTop });
        if (UI::ActionButton(lblToggleLabel, lblToggleW))
            _graphShowLabels = !_graphShowLabels;

        ImGuiMCP::SetCursorScreenPos(ImGuiMCP::ImVec2{ winPos.x + viewW - closeBtnW - scale.Px(8.0f), winPos.y + btnPadTop });
        const bool closeClicked = UI::ActionButton(closeLabel, closeBtnW);

        ImGuiMCP::PopStyleVar();

        SetWindowFontSize(scale.TextPx(UI::Theme::FontSize.caption));
        const char* sceneName = _graphScene ? _graphScene->name.c_str() : "Scene Graph";
        const ImGuiMCP::ImVec2 nameSz = ImGuiMCP::CalcTextSize(sceneName);
        DrawTextShadowed(dl, ImGuiMCP::ImVec2{ winPos.x + (viewW - nameSz.x) * 0.5f, winPos.y + (headerH - nameSz.y) * 0.5f },
            UI::Theme::Color.textSecondary, sceneName);

        // Graph canvas occupies the remainder; pan with left-drag, zoom with the wheel.
        const ImGuiMCP::ImVec2 canvasMin{ winPos.x, winPos.y + headerH };
        const ImGuiMCP::ImVec2 canvasMax{ winPos.x + viewW, winPos.y + viewH };
        const ImGuiMCP::ImVec2 canvasSize{ canvasMax.x - canvasMin.x, canvasMax.y - canvasMin.y };

        ImGuiMCP::SetCursorScreenPos(canvasMin);
        ImGuiMCP::InvisibleButton("##slpp_graphCanvas", canvasSize);
        const bool canvasHovered = ImGuiMCP::IsItemHovered();
        const bool canvasActive = ImGuiMCP::IsItemActive();

        if (canvasHovered && io->MouseWheel != 0.0f)
            _graphZoom = std::clamp(_graphZoom + io->MouseWheel * 0.1f, 0.3f, 2.5f);
        if (canvasActive && ImGuiMCP::IsMouseDown(ImGuiMCP::ImGuiMouseButton_Left)) {
            _graphPanX += io->MouseDelta.x;
            _graphPanY += io->MouseDelta.y;
            _graphFitPending = false;
        }

        // Fit-to-view on first open: center the bounding box of all nodes in the canvas.
        if (_graphFitPending && !_graphNodes.empty()) {
            float minX = _graphNodes[0].x, maxX = _graphNodes[0].x;
            float minY = _graphNodes[0].y, maxY = _graphNodes[0].y;
            for (const auto& n : _graphNodes) {
                minX = std::min(minX, n.x);
                maxX = std::max(maxX, n.x);
                minY = std::min(minY, n.y);
                maxY = std::max(maxY, n.y);
            }
            const float graphW = std::max(1.0f, maxX - minX);
            const float graphH = std::max(1.0f, maxY - minY);
            _graphZoom = std::clamp(std::min(canvasSize.x / (graphW + kColumnSpacing), canvasSize.y / (graphH + kRowSpacing * 2.0f)), 0.3f, 1.5f);
            _graphPanX = canvasSize.x * 0.5f - (minX + graphW * 0.5f) * _graphZoom;
            _graphPanY = canvasSize.y * 0.5f - (minY + graphH * 0.5f) * _graphZoom;  // centers in canvas
            _graphFitPending = false;
        }

        ImGuiMCP::ImDrawListManager::PushClipRect(dl, canvasMin, canvasMax, true);

        const auto worldToScreen = [&](float a_x, float a_y) {
            return ImGuiMCP::ImVec2{ canvasMin.x + _graphPanX + a_x * _graphZoom, canvasMin.y + _graphPanY + a_y * _graphZoom };
        };

        const float radius = kNodeRadius * scale.Factor() * std::clamp(_graphZoom, 0.5f, 1.0f);
        // Edges first, underneath nodes. Simple straight lines with a small arrowhead.
        for (const auto& edge : _graphEdges) {
            const auto& from = _graphNodes[edge.from];
            const auto& to = _graphNodes[edge.to];
            const ImGuiMCP::ImVec2 a = worldToScreen(from.x, from.y);
            const ImGuiMCP::ImVec2 b = worldToScreen(to.x, to.y);
            ImGuiMCP::ImDrawListManager::AddLine(dl, a, b, UI::Theme::Color.borderSubtle, scale.Px(1.2f));

            const float dx = b.x - a.x, dy = b.y - a.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > 1.0f) {
                const float ux = dx / len, uy = dy / len;
                const ImGuiMCP::ImVec2 tip{ b.x - ux * radius, b.y - uy * radius };
                const float wingSize = scale.Px(4.0f);
                const ImGuiMCP::ImVec2 left{ tip.x - ux * wingSize - uy * wingSize * 0.6f, tip.y - uy * wingSize + ux * wingSize * 0.6f };
                const ImGuiMCP::ImVec2 right{ tip.x - ux * wingSize + uy * wingSize * 0.6f, tip.y - uy * wingSize - ux * wingSize * 0.6f };
                ImGuiMCP::ImDrawListManager::AddTriangleFilled(dl, tip, left, right, UI::Theme::Color.borderHovered);
            }
        }

        int graphClickedIndex = -1;
        const ImGuiMCP::ImVec2 mouse = ImGuiMCP::GetMousePos();
        for (int i = 0; i < static_cast<int>(_graphNodes.size()); ++i) {
            const auto& node = _graphNodes[i];
            const ImGuiMCP::ImVec2 p = worldToScreen(node.x, node.y);
            if (p.x < canvasMin.x - radius || p.x > canvasMax.x + radius || p.y < canvasMin.y - radius || p.y > canvasMax.y + radius)
                continue;

            const float dx = mouse.x - p.x, dy = mouse.y - p.y;
            const bool hovered = canvasHovered && (dx * dx + dy * dy) <= (radius + scale.Px(4.0f)) * (radius + scale.Px(4.0f));
            if (hovered && ImGuiMCP::IsMouseClicked(ImGuiMCP::ImGuiMouseButton_Left))
                graphClickedIndex = i;

            const bool isCurrent = i == _graphCurrentIndex;
            const auto nodeType = _graphScene ? _graphScene->GetStageNodeType(node.stage) : Registry::Scene::NodeType::None;
            const auto fill = isCurrent ? UI::Theme::Color.accent : nodeType == Registry::Scene::NodeType::Sink ? UI::Theme::Color.buttonSelected : UI::Theme::Color.buttonIdle;
            const auto ring = isCurrent ? UI::Theme::Color.accent : hovered ? UI::Theme::Color.borderHovered : UI::Theme::Color.borderSubtle;

            if (isCurrent) {
                ImGuiMCP::ImDrawListManager::AddCircle(dl, p, radius + scale.Px(4.0f), UI::Theme::Color.accent, 24, scale.Px(1.5f));
            }

            ImGuiMCP::ImDrawListManager::AddCircleFilled(dl, p, radius, fill, 16);
            ImGuiMCP::ImDrawListManager::AddCircle(dl, p, radius, ring, 16, scale.Px(UI::Theme::Geometry.borderThin));

            // Show labels when toggle is on (always current + hovered regardless).
            const bool showLabel = _graphShowLabels || isCurrent || hovered;
            if (showLabel) {
                std::string label = !node.stage->navtext.empty() ? node.stage->navtext : node.stage->id;
                SetWindowFontSize(scale.TextPx(UI::Theme::FontSize.metadata) * 0.88f);
                const ImGuiMCP::ImVec2 labelSz = ImGuiMCP::CalcTextSize(label.c_str());
                DrawTextShadowed(dl, ImGuiMCP::ImVec2{ p.x - labelSz.x * 0.5f, p.y + radius + scale.Px(2.0f) },
                    isCurrent ? UI::Theme::Color.textPrimary : UI::Theme::Color.textSecondary, label.c_str());
            }
        }

        ImGuiMCP::ImDrawListManager::PopClipRect(dl);

        ImGuiMCP::SetWindowFontScale(1.0f);
        ImGuiMCP::End();

        // Node click
        const auto script = Script::GetScriptObject(_linkedThread, "sslThreadController");
        if (graphClickedIndex >= 0 && graphClickedIndex < static_cast<int>(_graphNodes.size())) {
            const auto* clickedStage = _graphNodes[graphClickedIndex].stage;
            if (_graphScene && clickedStage) {
                Script::DispatchMethodCall(script, "ToggleVisibilitySceneGraph", Script::CallbackPtr{}, -1);
                Script::DispatchMethodCall(script, "SkipTo", Script::CallbackPtr{}, std::string{ clickedStage->id });
                return;
            }
        }

        const bool escapePressed = ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false);
        if (closeClicked || escapePressed) {
            Script::DispatchMethodCall(script, "ToggleVisibilitySceneGraph", Script::CallbackPtr{}, -1);
        }
    }
}
