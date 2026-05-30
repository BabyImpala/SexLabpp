#include "ThreadConfigMenu.h"

#include "Registry/Library.h"
#include "SKSE/Translation.h"

namespace Thread::PrismaUI
{
    void ThreadConfigMenu::Init()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        const std::string json = BuildInitJson();
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "tcm_initOverlay", json.c_str());
        isOverlayVisible = true;
    };

    void ThreadConfigMenu::Destroy()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "tcm_destroyOverlay", "");
        isOverlayVisible = false;
        s_slots.clear();
    };

    // ── JS TO C++

    void ThreadConfigMenu::OnRandomScene([[maybe_unused]] const std::string& unused)
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;

        const auto* activeScene = instance->GetActiveScene();
        const auto  scenes = instance->GetThreadScenes();

        std::vector<const Registry::Scene*> candidates;
        candidates.reserve(scenes.size());
        for (const auto* s : scenes) {
            if (s != activeScene)
                candidates.push_back(s);
        }
        if (candidates.empty()) return;

        const size_t idx = static_cast<size_t>(rand()) % candidates.size();
        const std::string sceneId{ candidates[idx]->id };

        Script::DispatchMethodCall(PrismaSceneMenu::psm_threadScript, "ResetScene", PrismaSceneMenu::psm_callbackPtr,
            RE::BSFixedString{ sceneId.c_str() });
    };

    void ThreadConfigMenu::OnMoveScene([[maybe_unused]] const std::string& unused)
    {
        if (!PrismaSceneMenu::psm_threadScript) return;
        Script::DispatchMethodCall(PrismaSceneMenu::psm_threadScript, "TogglePrismaFocus", PrismaSceneMenu::psm_callbackPtr);
        Script::DispatchMethodCall(PrismaSceneMenu::psm_threadScript, "MoveScene", PrismaSceneMenu::psm_callbackPtr);
    };

    void ThreadConfigMenu::OnAutoPlaySet(const std::string& data)
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;
        instance->SetThreadProperty<bool>("AutoAdvance", (data == "true"));
    };

    void ThreadConfigMenu::OnNextPermutation(const std::string& data)
    {
        if (!isOverlayVisible || !IsViewValid(&PrismaSceneMenu::psmView)) return;
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;

        uint32_t formId = 0;
        try { formId = static_cast<uint32_t>(std::stoul(data)); } catch (...) { return; }
        auto* actor = FindActor(formId);
        if (!actor) return;

        instance->SetNextPermutation(actor);
        const auto cur   = instance->GetCurrentPermutation(actor);
        const auto total = instance->GetUniquePermutations(actor);
        const std::string payload = std::to_string(formId) + "^" +
                                    std::to_string(cur)    + "^" +
                                    std::to_string(total);
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "tcm_setPermutation", payload.c_str());
    };

    void ThreadConfigMenu::OnSetExpression(const std::string& data)
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;

        const auto sep = data.find('|');
        if (sep == std::string::npos) return;
        uint32_t formId = 0;
        try { formId = static_cast<uint32_t>(std::stoul(data.substr(0, sep))); } catch (...) { return; }
        auto* actor = FindActor(formId);
        if (!actor) return;

        const std::string exprId = data.substr(sep + 1);
        const auto* expr = Registry::Library::GetSingleton()->GetExpressionById(exprId.c_str());
        if (!expr) return;
        instance->SetExpression(actor, expr);
    };

    void ThreadConfigMenu::OnSetVoice(const std::string& data)
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return;

        const auto sep = data.find('|');
        if (sep == std::string::npos) return;
        uint32_t formId = 0;
        try { formId = static_cast<uint32_t>(std::stoul(data.substr(0, sep))); } catch (...) { return; }
        auto* actor = FindActor(formId);
        if (!actor) return;

        const std::string voiceId = data.substr(sep + 1);
        const auto* voice = Registry::Library::GetSingleton()->GetVoiceById(voiceId.c_str());
        if (!voice) return;
        instance->SetVoice(actor, voice);
    };

    void ThreadConfigMenu::OnSetActorAlpha(const std::string& data)
    {
        const auto sep = data.find('^');
        if (sep == std::string::npos) return;
        uint32_t formId = 0;
        int alphaInt = 100;
        try {
            formId   = static_cast<uint32_t>(std::stoul(data.substr(0, sep)));
            alphaInt = std::stoi(data.substr(sep + 1));
        } catch (...) { return; }

        auto* actor = FindActor(formId);
        if (!actor) return;

        actor->SetAlpha(std::clamp(alphaInt, 0, 100) / 100.0f);
    };

    // ── HELPERS

    std::string ThreadConfigMenu::BuildInitJson()
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return "{}";

        s_slots.clear();
        for (auto* actor : instance->GetActors()) {
            if (actor)
                s_slots.push_back({ actor, actor->GetFormID(), actor->IsPlayerRef() });
        }
        std::sort(s_slots.begin(), s_slots.end(), [](const ActorSlot& a, const ActorSlot& b) {
            if (a.isPlayer != b.isPlayer) return a.isPlayer;
            return std::string_view{ a.actor->GetDisplayFullName() } < 
                std::string_view{ b.actor->GetDisplayFullName() };
        });

        auto* lib = Registry::Library::GetSingleton();

        std::string actorsJson = "[";
        for (size_t i = 0; i < s_slots.size(); ++i) {
            const auto& slot  = s_slots[i];
            RE::Actor*  actor = slot.actor;

            if (i > 0) actorsJson += ',';
            actorsJson += '{';
            actorsJson += "\"id\":"        + std::to_string(slot.formId) + ',';
            actorsJson += "\"name\":\""    + JsonEscape(actor->GetDisplayFullName()) + "\",";
            actorsJson += "\"isPlayer\":"  + std::string(slot.isPlayer ? "true" : "false") + ',';

            const auto curPerm   = instance->GetCurrentPermutation(actor);
            const auto totalPerm = instance->GetUniquePermutations(actor);
            actorsJson += "\"permCurrent\":" + std::to_string(curPerm)   + ',';
            actorsJson += "\"permTotal\":"   + std::to_string(totalPerm) + ',';

            const auto* expr = instance->GetExpression(actor);
            actorsJson += "\"expressionId\":\"" + JsonEscape(expr ? expr->GetId().c_str() : "") + "\",";

            actorsJson += "\"expressions\":[";
            if (Registry::RaceKey(actor).Is(Registry::RaceKey::Value::Human)) {
                bool firstExpr = true;
                lib->ForEachExpression([&](const auto& expression) {
                    if (!expression.enabled) return false;
                    if (!firstExpr) {
                        actorsJson += ',';
                    }
                    std::string rawId = expression.GetId().c_str();
                    std::string displayName = rawId;
                    SKSE::Translation::Translate(rawId, displayName);
                    actorsJson += "{\"id\":\""   + JsonEscape(rawId) + "\","
                                   "\"name\":\"" + JsonEscape(displayName) + "\"}";
                    firstExpr = false;
                    return false;
                });
            }
            actorsJson += "],";

            const auto* voice = instance->GetVoice(actor);
            actorsJson += "\"voiceId\":\"" + JsonEscape(voice ? voice->GetId().c_str() : "") + "\",";

            actorsJson += "\"voices\":[";
            const Registry::RaceKey actRace{ actor };
            bool firstVoice = true;
            lib->ForEachVoice([&](const auto& v) {
                if (!v.HasRace(actRace)) return false;
                if (!firstVoice) {
                    actorsJson += ',';
                }
                std::string rawId = v.GetId().c_str();
                std::string displayName = rawId;
                SKSE::Translation::Translate(rawId, displayName);
                actorsJson += "{\"id\":\""   + JsonEscape(rawId) + "\","
                               "\"name\":\"" + JsonEscape(displayName) + "\"}";
                firstVoice = false;
                return false;
            });
            actorsJson += "],";

            const int alphaInt = static_cast<int>(std::round(actor->GetAlpha() * 100.0f));
            actorsJson += "\"alpha\":" + std::to_string(alphaInt);

            actorsJson += '}';
        }
        actorsJson += ']';

        const bool autoPlay = instance->GetThreadProperty<bool>("AutoAdvance");

        std::string json = "{";
        json += "\"autoPlay\":" + std::string(autoPlay ? "true" : "false") + ',';
        json += "\"actors\":"   + actorsJson;
        json += "}";
        return json;
    };

    RE::Actor* ThreadConfigMenu::FindActor(uint32_t formId)
    {
        for (const auto& slot : s_slots)
            if (slot.formId == formId) return slot.actor;
        logger::warn("ThreadConfigMenu::FindActor >> no slot for formId {:08X}", formId);
        return nullptr;
    };

}  // namespace Thread::PrismaUI
