#include "LegacyNiUpdate.h"

#include "LegacyNiMath.h"
#include "Thread/Interface/SceneHUD.h"

namespace Thread::LegacyNiNode
{
    namespace
    {
        constexpr std::uint32_t PROTOTYPE_TIMING_INTERVAL{ 30 };
    }

    NiInstance::NiInstance(const std::vector<RE::Actor*>& a_positions, const Registry::Scene* a_scene) :
      positions([&]() {
          std::vector<LegacyNiNode::NiPosition> v{};
          v.reserve(a_positions.size());
          for (size_t i = 0; i < a_positions.size(); i++) {
              auto& it = a_positions[i];
              auto sex = a_scene->GetNthPosition(i)->data.GetSex().get();
              v.emplace_back(it, sex);
          }
          return v;
      }())
    {}

    bool NiInstance::VisitPositions(std::function<bool(const NiPosition&)> a_visitor) const
    {
        std::scoped_lock lk{ _m };
        for (auto&& pos : positions) {
            if (a_visitor(pos))
                return true;
        }
        return false;
    }

    void NiInstance::UpdateInteractions(float a_delta, bool a_drawCollision)
    {
        std::unique_lock lk{ _m, std::defer_lock };
        if (!lk.try_lock()) {
            return;
        }
        std::vector<NiPosition::Snapshot> snapshots{};
        snapshots.reserve(positions.size());
        for (auto&& it : positions) {
            snapshots.emplace_back(it);
        }
        if (a_drawCollision) {
            auto& debugDraw = Interface::SceneHUD::GetSingleton().GetDebugDraw();
            for (const auto& snapshot : snapshots) {
                if (snapshot.vaginalOpening) {
                    debugDraw.AddRing(snapshot.vaginalOpening->center, snapshot.vaginalOpening->right, snapshot.vaginalOpening->up, snapshot.vaginalOpening->radius);
                }
                if (snapshot.analOpening) {
                    debugDraw.AddRing(snapshot.analOpening->center, snapshot.analOpening->right, snapshot.analOpening->up, snapshot.analOpening->radius);
                }
                for (const auto& schlong : snapshot.position.nodes.schlongs) {
                    if (const auto* shaft = schlong->GetCollisionShape()) {
                        // Draw the same tapered segment chain consumed by the opening collision test.
                        for (std::size_t section = 1; section < shaft->sections.size(); ++section) {
                            debugDraw.AddTaperedCapsule(shaft->sections[section - 1].center, shaft->sections[section].center, shaft->sections[section - 1].radius,
                                shaft->sections[section].radius);
                        }
                        if (!shaft->sections.empty()) {
                            debugDraw.AddTaperedCapsule(shaft->sections.back().center, shaft->tip, shaft->sections.back().radius, 0.0f);
                        }
                    }
                }
            }
        }
        for (auto&& fst : snapshots) {
            GetInteractionsMale(snapshots, fst);
            GetInteractionsFemale(snapshots, fst);
            GetInteractionsNeutral(snapshots, fst);
        }
        for (size_t i = 0; i < positions.size(); i++) {
            auto& pos = positions[i];
            for (auto&& act : snapshots[i].interactions) {
                auto where = pos.interactions.find(act);
                if (where == pos.interactions.end()) {
                    continue;
                }
                const float delta_dist = act.distance - where->distance;
                if (a_delta != 0.0f) {
                    act.velocity = (where->velocity + (delta_dist / a_delta)) / 2;
                } else {
                    act.velocity = where->velocity;
                }
            }
            positions[i].interactions = { snapshots[i].interactions.begin(), snapshots[i].interactions.end() };
        }
    }

    void NiInstance::GetInteractionsMale(std::vector<NiPosition::Snapshot>& list, const NiPosition::Snapshot& it)
    {
        if (it.position.sex.any(Registry::Sex::Female))
            return;
        for (auto&& schlong : it.position.nodes.schlongs) {
            for (auto&& act : list) {
                if (act.GetHeadPenisInteractions(it, schlong))
                    break;
                if (act.GetHandPenisInteractions(it, schlong))
                    break;
                if (it == act)
                    continue;
                if (act.GetCrotchPenisInteractions(it, schlong)) {
                    break;
                }
                act.GetFootPenisInteractions(it, schlong);
            }
        }
    }

    void NiInstance::GetInteractionsFemale(std::vector<NiPosition::Snapshot>& list, const NiPosition::Snapshot& it)
    {
        if (it.position.sex.any(Registry::Sex::Male))
            return;
        for (auto&& snd : list) {
            if (it != snd) {
                snd.GetVaginaVaginaInteractions(it);
            }
            snd.GetHeadVaginaInteractions(it);
            snd.GetVaginaLimbInteractions(it);
        }
    }

    void NiInstance::GetInteractionsNeutral(std::vector<NiPosition::Snapshot>& list, const NiPosition::Snapshot& it)
    {
        for (auto&& snd : list) {
            if (it != snd) {
                snd.GetHeadHeadInteractions(it);
            }
            snd.GetHeadFootInteractions(it);
            snd.GetHeadAnimObjInteractions(it);
        }
    }

    void NiUpdate::Install()
    {
        // UpdateThirdPerson
        if (REL::Module::IsVR()) {
            stl::write_thunk_call<NiUpdate>(REL::Offset(0x6c6a7d).address());
        } else {
            REL::Relocation<std::uintptr_t> addr{ RELOCATION_ID(39446, 40522), 0x94 };
            stl::write_thunk_call<NiUpdate>(addr.address());
        }
        logger::info("Registered Functions");
    }

    void NiUpdate::thunk(RE::NiAVObject* a_obj, RE::NiUpdateData* updateData)
    {
        func(a_obj, updateData);
        static const auto gameDaysPassed = RE::Calendar::GetSingleton()->gameDaysPassed;
        if (!gameDaysPassed) {
            return;
        }
        std::scoped_lock lk{ _m };
        static std::uint32_t prototypeFrame = 0;
        const auto ms_passed = gameDaysPassed->value * 24 * 60'000;
        static float ms_passed_last = ms_passed;
        const auto delta = ms_passed - ms_passed_last;
        const bool logPrototypeTiming = prototypeFrame++ % PROTOTYPE_TIMING_INTERVAL == 1;
        const auto start = std::chrono::high_resolution_clock::now();
        ms_passed_last = ms_passed;
        auto& debugDraw = Interface::SceneHUD::GetSingleton().GetDebugDraw();
        debugDraw.BeginFrame();
        const auto* linkedThread = Interface::SceneHUD::GetSingleton().GetLinkedThread();
        for (auto&& [id, process] : processes) {
            process->UpdateInteractions(delta, linkedThread && id == linkedThread->GetFormID());
        }
        debugDraw.Publish();
        if (logPrototypeTiming && !processes.empty()) {
            const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start);
            logger::info("Legacy surface collision frame: {:.2f}ms", elapsed.count());
        }
    }

    std::shared_ptr<NiInstance> NiUpdate::Register(RE::FormID a_id, std::vector<RE::Actor*> a_positions, const Registry::Scene* a_scene) noexcept
    {
        try {
            const auto where = std::ranges::find(processes, a_id, [](auto& it) { return it.first; });
            if (where != processes.end()) {
                logger::info("Object with ID {:X} already registered. Resetting NiInstance.", a_id);
                std::swap(*where, processes.back());
                processes.pop_back();
            }
            const auto start = std::chrono::high_resolution_clock::now();
            auto process = std::make_shared<NiInstance>(a_positions, a_scene);
            const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start);
            logger::info("Legacy surface collision initialization: {:.2f}ms", elapsed.count());
            return processes.emplace_back(a_id, process).second;
        } catch (const std::exception& e) {
            logger::error("Failed to register NiInstance: {}", e.what());
            return nullptr;
        } catch (...) {
            logger::error("Failed to register NiInstance: Unknown error");
            return nullptr;
        }
    }

    void NiUpdate::Unregister(RE::FormID a_id) noexcept
    {
        const auto where = std::ranges::find(processes, a_id, [](auto& it) { return it.first; });
        if (where == processes.end()) {
            logger::error("No object registered using ID {:X}", a_id);
            return;
        }
        processes.erase(where);
    }

}  // namespace Thread::LegacyNiNode
