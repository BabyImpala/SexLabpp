#include "PrismaUtil.h"

#include "FurnSelectionMenu.h"
#include "Overlays/AnimSpeedOverlay.h"
#include "Overlays/EnjoymentBars.h"
#include "Overlays/OffsetAdjustMenu.h"
#include "Overlays/SceneSelectorMenu.h"
#include "Overlays/ThreadConfigMenu.h"
#include "Overlays/VisibilityControlMenu.h"

namespace Thread::PrismaUI
{
    void RegisterPrismaViews()
    {
        if (!FurnSelectionMenu::Register()) {
            logger::warn("PrsimaUtil::RegisterPrismaViews >> Failed to register FurnSelectionMenu");
        }
        if (!PrismaSceneMenu::Register()) {
            logger::warn("PrsimaUtil::RegisterPrismaViews >> Failed to register PrismaSceneMenu");
        }
    };

    bool IsViewValid(PrismaView* a_view)
    {
        return a_view && PrismaAPI && PrismaAPI->IsValid(*a_view);
    }

    void ShowView(PrismaView* a_view)
    {
        if (!IsViewValid(a_view)) return;
        OverlaySuppressor::Register(*a_view);
        PrismaAPI->Show(*a_view);
    };

    void HideView(PrismaView* a_view)
    {
        if (!IsViewValid(a_view)) return;
        OverlaySuppressor::Unregister(*a_view);
        PrismaAPI->Hide(*a_view);
    };

    bool IsViewVisible(PrismaView* a_view)
    {
        return IsViewValid(a_view) && !PrismaAPI->IsHidden(*a_view);
    };

    void OverlayInit(RE::TESQuest* a_qst, PrismaOverlayIndex index)
    {
        logger::info("PrsimaUtil::OverlayInit >> Attempting init for idx {}", static_cast<int32_t>(index));
        switch (index) {
        case PrismaOverlayIndex::kPrismaSceneMenu:        PrismaSceneMenu::Init(a_qst); break;
        case PrismaOverlayIndex::kAnimSpeedOverlay:       AnimSpeedOverlay::Init(); break;
        case PrismaOverlayIndex::kEnjoymentBars:          EnjoymentBars::Init(); break;
        case PrismaOverlayIndex::kOffsetAdjustMenu:       OffsetAdjustMenu::Init(); break;
        case PrismaOverlayIndex::kSceneSelectorMenu:      SceneSelectorMenu::Init(); break;
        case PrismaOverlayIndex::kThreadConfigMenu:       ThreadConfigMenu::Init(); break;
        case PrismaOverlayIndex::kVisibilityControlMenu:  VisibilityControlMenu::Init(); break;
        default: logger::warn("PrsimaUtil::OverlayInit >> Unknown overlay index {}", static_cast<int32_t>(index)); break;
        }
    };

    void OverlayDestroy(PrismaOverlayIndex index)
    {
        logger::info("PrsimaUtil::OverlayDestroy >> Attempting destroy for idx {}", static_cast<int32_t>(index));
        switch (index) {
        case PrismaOverlayIndex::kPrismaSceneMenu:        PrismaSceneMenu::Destroy(); break;
        case PrismaOverlayIndex::kAnimSpeedOverlay:       AnimSpeedOverlay::Destroy(); break;
        case PrismaOverlayIndex::kEnjoymentBars:          EnjoymentBars::Destroy(); break;
        case PrismaOverlayIndex::kOffsetAdjustMenu:       OffsetAdjustMenu::Destroy(); break;
        case PrismaOverlayIndex::kSceneSelectorMenu:      SceneSelectorMenu::Destroy(); break;
        case PrismaOverlayIndex::kThreadConfigMenu:       ThreadConfigMenu::Destroy(); break;
        case PrismaOverlayIndex::kVisibilityControlMenu:  VisibilityControlMenu::Destroy(); break;
        default: logger::warn("PrsimaUtil::OverlayDestroy >> Unknown overlay index {}", static_cast<int32_t>(index)); break;
        }
    };

    std::string JsonEscape(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            default:   out += c;      break;
            }
        }
        return out;
    };

}  // namespace Thread::PrismaUI
