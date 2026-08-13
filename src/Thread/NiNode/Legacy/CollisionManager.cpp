#include "CollisionManager.h"

#include "Thread/Interface/SceneHUD.h"

namespace Thread::NiNode::Surface
{
    namespace
    {
        constexpr std::uint32_t TIMING_LOG_INTERVAL{ 30 };
    }

    Scene::Scene(const std::vector<RE::Actor*>& a_positions, const Registry::Scene* a_scene)
    {
        positions.reserve(a_positions.size());
        for (std::size_t i = 0; i < a_positions.size(); ++i) {
            positions.emplace_back(a_positions[i], a_scene->GetNthPosition(i)->data.GetSex().get());
        }
    }

    bool Scene::VisitPositions(const std::function<bool(const ActorState&)>& a_visitor) const
    {
        std::scoped_lock lock{ _mutex };
        for (const auto& position : positions) {
            if (a_visitor(position)) {
                return true;
            }
        }
        return false;
    }

    void Scene::UpdateInteractions(float a_delta, bool a_drawCollision)
    {
        std::unique_lock lock{ _mutex, std::defer_lock };
        if (!lock.try_lock()) {
            return;
        }
        std::vector<ActorState::Frame> frames;
        frames.reserve(positions.size());
        for (auto& position : positions) {
            frames.emplace_back(position);
        }
        if (a_drawCollision) {
            auto& debugDraw = Interface::SceneHUD::GetSingleton().GetDebugDraw();
            for (const auto& frame : frames) {
                if (frame.mouthOpening) {
                    debugDraw.AddRing(frame.mouthOpening->center, frame.mouthOpening->right, frame.mouthOpening->up, frame.mouthOpening->radius);
                }
                if (frame.vaginalOpening) {
                    debugDraw.AddRing(frame.vaginalOpening->center, frame.vaginalOpening->right, frame.vaginalOpening->up, frame.vaginalOpening->radius);
                }
                if (frame.analOpening) {
                    debugDraw.AddRing(frame.analOpening->center, frame.analOpening->right, frame.analOpening->up, frame.analOpening->radius);
                }
                for (const auto& shaft : frame.state.geometry.shafts) {
                    if (const auto* collisionShape = shaft.GetCollisionShape()) {
                        // Draw the same tapered segment chain consumed by the opening collision test.
                        for (std::size_t section = 1; section < collisionShape->sections.size(); ++section) {
                            debugDraw.AddTaperedCapsule(collisionShape->sections[section - 1].center, collisionShape->sections[section].center,
                                collisionShape->sections[section - 1].radius, collisionShape->sections[section].radius);
                        }
                        if (!collisionShape->sections.empty()) {
                            debugDraw.AddTaperedCapsule(collisionShape->sections.back().center, collisionShape->tip, collisionShape->sections.back().radius, 0.0f);
                        }
                    }
                }
            }
        }
        for (const auto& source : frames) {
            DetectShaftInteractions(frames, source);
            DetectVaginalInteractions(frames, source);
            DetectGeneralInteractions(frames, source);
        }
        for (std::size_t i = 0; i < positions.size(); ++i) {
            auto& position = positions[i];
            for (auto& interaction : frames[i].interactions) {
                const auto previous = position.interactions.find(interaction);
                if (previous == position.interactions.end()) {
                    continue;
                }
                const float distanceDelta = interaction.distance - previous->distance;
                if (a_delta != 0.0f) {
                    interaction.velocity = (previous->velocity + distanceDelta / a_delta) * 0.5f;
                } else {
                    interaction.velocity = previous->velocity;
                }
            }
            position.interactions = { frames[i].interactions.begin(), frames[i].interactions.end() };

            // Temporary interaction validation; remove after collision behavior is verified.
            for (const auto& interaction : positions[i].interactions) {
                logger::info("Surface interaction: actor={}, partner={}, action={}, distance={:.2f}, velocity={:.2f}", position.actor->GetName(),
                    interaction.partner->GetName(), magic_enum::enum_name(interaction.action), interaction.distance, interaction.velocity);
            }
        }
    }

    void Scene::DetectShaftInteractions(std::vector<ActorState::Frame>& a_frames, const ActorState::Frame& a_source)
    {
        if (a_source.state.sex.any(Registry::Sex::Female)) {
            return;
        }
        for (const auto& shaft : a_source.state.geometry.shafts) {
            for (auto& target : a_frames) {
                if (target.DetectShaftHead(a_source, shaft)) {
                    break;
                }
                if (target.DetectShaftHand(a_source, shaft)) {
                    break;
                }
                if (a_source == target) {
                    continue;
                }
                if (target.DetectShaftCrotch(a_source, shaft)) {
                    break;
                }
                target.DetectShaftFoot(a_source, shaft);
            }
        }
    }

    void Scene::DetectVaginalInteractions(std::vector<ActorState::Frame>& a_frames, const ActorState::Frame& a_source)
    {
        if (a_source.state.sex.any(Registry::Sex::Male)) {
            return;
        }
        for (auto& target : a_frames) {
            if (a_source != target) {
                target.DetectVaginalContact(a_source);
            }
            target.DetectVaginalOral(a_source);
            target.DetectVaginalLimb(a_source);
        }
    }

    void Scene::DetectGeneralInteractions(std::vector<ActorState::Frame>& a_frames, const ActorState::Frame& a_source)
    {
        for (auto& target : a_frames) {
            if (a_source != target) {
                target.DetectKissing(a_source);
            }
            target.DetectToeSucking(a_source);
            target.DetectAnimObjectFace(a_source);
        }
    }

    void Manager::OnFrameUpdate(float a_delta)
    {
        std::scoped_lock lock{ _mutex };
        static std::uint32_t frame = 0;
        const bool logTiming = frame++ % TIMING_LOG_INTERVAL == 1;
        const auto start = std::chrono::high_resolution_clock::now();
        auto& debugDraw = Interface::SceneHUD::GetSingleton().GetDebugDraw();
        debugDraw.BeginFrame();
        const auto* linkedThread = Interface::SceneHUD::GetSingleton().GetLinkedThread();
        for (auto&& [id, scene] : scenes) {
            scene->UpdateInteractions(a_delta, linkedThread && id == linkedThread->GetFormID());
        }
        debugDraw.Publish();
        if (logTiming && !scenes.empty()) {
            const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start);
            logger::info("Surface collision frame: {:.2f}ms", elapsed.count());
        }
    }

    std::shared_ptr<Scene> Manager::Register(RE::FormID a_id, std::vector<RE::Actor*> a_positions, const Registry::Scene* a_scene) noexcept
    {
        try {
            const auto where = std::ranges::find(scenes, a_id, [](const auto& a_entry) { return a_entry.first; });
            if (where != scenes.end()) {
                logger::info("Object with ID {:X} already registered; resetting surface collision scene", a_id);
                std::swap(*where, scenes.back());
                scenes.pop_back();
            }
            const auto start = std::chrono::high_resolution_clock::now();
            auto process = std::make_shared<Scene>(a_positions, a_scene);
            const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start);
            logger::info("Surface collision initialization: {:.2f}ms", elapsed.count());
            return scenes.emplace_back(a_id, std::move(process)).second;
        } catch (const std::exception& e) {
            logger::error("Failed to register surface collision scene: {}", e.what());
            return nullptr;
        } catch (...) {
            logger::error("Failed to register surface collision scene: unknown error");
            return nullptr;
        }
    }

    void Manager::Unregister(RE::FormID a_id) noexcept
    {
        const auto where = std::ranges::find(scenes, a_id, [](const auto& a_entry) { return a_entry.first; });
        if (where == scenes.end()) {
            logger::error("No object registered using ID {:X}", a_id);
            return;
        }
        scenes.erase(where);
    }

}  // namespace Thread::NiNode::Surface
