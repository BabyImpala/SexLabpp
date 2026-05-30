#include "OffsetAdjustMenu.h"

namespace Thread::PrismaUI
{
    void OffsetAdjustMenu::Init()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        const std::string json = BuildInitJson();
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "oam_initOverlay", json.c_str());
        isOverlayVisible = true;
    };

    void OffsetAdjustMenu::Destroy()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "oam_destroyOverlay", "");
        isOverlayVisible = false;
        s_selectedActorId = std::nullopt;
        s_slots.clear();
    };

    // ── C++ to JS

    // payload: "actorId^x^y^z^r" (actorId=0 means scene/furniture)
    void OffsetAdjustMenu::SetOffsetsDisplay(uint32_t actorId)
    {
        if (!isOverlayVisible) return;
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;

        auto* activeScene = instance->GetActiveScene();
        auto* activeStage = instance->GetActiveStage();
        if (!activeScene || !activeStage) return;

        std::vector<float> v{ 0, 0, 0, 0 };
        if (actorId == 0) {
            v = activeScene->furnitureOffset.GetOffset().AsVector();
        } else {
            const auto* slot = FindSlot(actorId);
            if (!slot) return;
            v = activeStage->positions[slot->positionIndex].offset.GetOffset().AsVector();
        }

        const std::string payload =
            std::to_string(actorId) + "^" +
            std::to_string(v[0]) + "^" +
            std::to_string(v[1]) + "^" +
            std::to_string(v[2]) + "^" +
            std::to_string(v[3]);

        if (IsViewValid(&PrismaSceneMenu::psmView)) {
            PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "oam_setOffsetsDisplay", payload.c_str());
        }
    };

    void OffsetAdjustMenu::OnStageChanged()
    {
        if (!isOverlayVisible || !s_selectedActorId) return;
        if (s_selectedActorId.has_value())
            SetOffsetsDisplay(s_selectedActorId.value());
    };

    // ── JS TO C++

    void OffsetAdjustMenu::OnActorSelected(const std::string& actorIdStr)
    {
        uint32_t id = 0;
        try { id = static_cast<uint32_t>(std::stoul(actorIdStr)); } catch (...) { return; }
        s_selectedActorId = id;
        SetOffsetsDisplay(id);
    };

    // data = "axis^actorId^value" -- axis: X,Y,Z,R -- value: absolute float
    void OffsetAdjustMenu::OnSetOffset(const std::string& data)
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;

        const auto c1 = data.find('^');
        const auto c2 = data.find('^', c1 + 1);
        if (c1 == std::string::npos || c2 == std::string::npos) return;
        const std::string axisStr = data.substr(0, c1);
        uint32_t actorId = 0;
        float value = 0.0f;
        try {
            actorId = static_cast<uint32_t>(std::stoul(data.substr(c1 + 1, c2 - c1 - 1)));
            value = std::stof(data.substr(c2 + 1));
        } catch (...) {
            logger::warn("OffsetAdjustMenu::OnSetOffset >> parse error: {}", data);
            return;
        }
        const auto axis = axisStr == "X" ? Registry::CoordinateType::X :
                          axisStr == "Y" ? Registry::CoordinateType::Y :
                          axisStr == "Z" ? Registry::CoordinateType::Z :
                                           Registry::CoordinateType::R;
        instance->OffsetAdjustSet(actorId, axis, value);
    };

    void OffsetAdjustMenu::OnResetOffsets()
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;
        instance->OffsetAdjustReset();
        if (s_selectedActorId.has_value()) {
            SetOffsetsDisplay(s_selectedActorId.value());
        }
    };

    void OffsetAdjustMenu::OnSetAdjustStageOnly(const std::string& data)
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;
        instance->SetThreadProperty<bool>("AdjustStage", (data == "true"));
    };

    // ── HELPERS

    std::string OffsetAdjustMenu::BuildInitJson()
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return "{}";

        auto* furniture = instance->GetCenterRef();
        const bool s_hasFurniture = (furniture != nullptr);
        const bool centerIsPlayer = (s_hasFurniture && furniture->IsPlayerRef());
        const bool adjustStageOnly = instance->GetThreadProperty<bool>("AdjustStage");

        s_slots.clear();
        const auto actors = instance->GetActors();
        for (size_t i = 0; i < actors.size(); ++i) {
            if (auto* actor = actors[i]) {
                s_slots.push_back({ actor, actor->GetFormID(), i });
            }
        }
        std::sort(s_slots.begin(), s_slots.end(), [](const ActorSlot& a, const ActorSlot& b) {
            bool ap = a.actor->IsPlayerRef(), bp = b.actor->IsPlayerRef();
            if (ap != bp) return ap;
            return std::string_view{ a.actor->GetDisplayFullName() } <
                std::string_view{ b.actor->GetDisplayFullName() };
        });

        std::string json = "{";
        json += "\"oam_hasFurniture\":"    + std::string(s_hasFurniture  ? "true" : "false") + ",";
        json += "\"oam_centerIsPlayer\":"  + std::string(centerIsPlayer  ? "true" : "false") + ",";
        json += "\"oam_adjustStageOnly\":" + std::string(adjustStageOnly ? "true" : "false") + ",";
        json += "\"oam_actorsData\":[";
        
        for (size_t i = 0; i < s_slots.size(); ++i) {
            auto* actor = s_slots[i].actor;
            if (i > 0) json += ",";
            json += "{\"id\":"         + std::to_string(s_slots[i].formId) +
                    ",\"name\":\""     + JsonEscape(actor->GetDisplayFullName()) + "\"" +
                    ",\"isPlayer\":"   + (actor->IsPlayerRef() ? "true" : "false") + "}";
        }
        json += "]}";

        return json;
    };

    const OffsetAdjustMenu::ActorSlot* OffsetAdjustMenu::FindSlot(uint32_t actorId)
    {
        for (const auto& slot : s_slots)
            if (slot.formId == actorId) return &slot;
        return nullptr;
    };

}  // namespace Thread::PrismaUI
