#include "FurnSelectionMenu.h"

#include "Thread/Thread.h"

namespace Thread::PrismaUI
{
    bool FurnSelectionMenu::Register()
    {
        return CreatePrismaView(FILEPATH, &fsmView, []() {

            PrismaAPI->RegisterJSListener(fsmView, "fsm_OnFurnItemSelected", [](const char* data) {
                HandleSelection(data ? data : "");
            });
        });
    };

    void FurnSelectionMenu::Open(RE::TESQuest* a_qst, const std::vector<Item>& a_items)
    {
        if (!PrismaAPI || !PrismaAPI->IsValid(fsmView) || !a_qst || a_items.empty()) return;

        fsm_linkedThread = a_qst;
        fsm_items = a_items;

        std::string json = "{\"items\":[";
        for (size_t i = 0; i < fsm_items.size(); ++i) {
            if (i > 0)
                json += ',';
            json += "{\"name\":\"";   json += JsonEscape(fsm_items[i].GetName());
            json += "\",\"type\":\""; json += JsonEscape(fsm_items[i].GetValue());
            json += "\",\"index\":";  json += std::to_string(i);
            json += '}';
        }
        json += "]}";

        PrismaAPI->InteropCall(fsmView, "fsm_populateItems", json.c_str());

        ShowView(&fsmView);
        PrismaAPI->Focus(fsmView, false);  // second arg -> pauseGame
    };

    // ── JS TO C++ ───────────────────────────────────────────────────────────────

    void FurnSelectionMenu::HandleSelection(const std::string& data)
    {
        HideView(&fsmView);
        if (!fsm_linkedThread) {
            logger::error("FurnSelectionMenu::HandleSelection >> fsm_linkedThread is null");
            return;
        }
        size_t index = 0;
        try {
            index = static_cast<size_t>(std::stoul(data));
            if (index >= fsm_items.size()) {
                logger::error("FurnSelectionMenu::HandleSelection >> index {} out of range (size {}), defaulting to 0", index, fsm_items.size());
                index = 0;
            }
        } catch (const std::exception& e) {
            logger::error("FurnSelectionMenu::HandleSelection >> failed to parse '{}': {}", data, e.what());
        }
        auto* qst = fsm_linkedThread;
        fsm_linkedThread = nullptr;
        fsm_items.clear();
        auto* instance = Instance::GetPendingInstance(qst);
        if (!instance) {
            logger::error("FurnSelectionMenu::HandleSelection >> instance not found");
            return;
        }
        instance->SetCenterRefSelected(index);
    };

}  // namespace Thread::PrismaUI
