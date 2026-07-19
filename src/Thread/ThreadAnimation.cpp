#include "Thread.h"

#include "Registry/Util/Scale.h"
#include "Thread/Collision/CollisionHandler.h"
#include "Util/Script.h"

#include <bit>

namespace Thread
{
    void Instance::ContinueStartAnimations()
    {
        actorLockRequested = false;
        actorLockAcknowledged = true;
        logger::info("Papyrus actor locks acknowledged for thread {:X}.", linkedQst->GetFormID());
    }

    void Instance::CancelPendingAnimations(RE::TESQuest* a_linkedQst)
    {
        std::unique_lock lock{ _mInstances };
        for (auto&& instance : instances) {
            if (instance->linkedQst != a_linkedQst) {
                continue;
            }
            instance->ReleaseAnimations();
            if (!instance->pendingAnimations.empty()) {
                logger::info("Cancelled {} pending animation synchronization(s).", instance->pendingAnimations.size());
                instance->pendingAnimations.clear();
            }
            return;
        }
    }

    void Instance::UpdateAnimations(float a_delta)
    {
        std::shared_lock lock{ _mInstances };
        for (auto&& instance : instances) {
            instance->UpdatePendingAnimations(a_delta);
        }
    }

    bool Instance::GetActiveClips(RE::Actor* a_actor, std::vector<ActiveClip>& a_clips) const
    {
        RE::BSAnimationGraphManagerPtr graphManager;
        if (!a_actor->GetAnimationGraphManager(graphManager) || !graphManager) {
            return false;
        }

        auto& runtime = graphManager->GetRuntimeData();
        RE::BSSpinLockGuard lock{ runtime.updateLock };
        if (runtime.activeGraph >= graphManager->graphs.size()) {
            return false;
        }

        const auto animationGraph = graphManager->graphs[runtime.activeGraph].get();
        if (!animationGraph || !animationGraph->behaviorGraph || !animationGraph->behaviorGraph->isActive) {
            return false;
        }

        const auto activeNodes = *reinterpret_cast<RE::hkArray<RE::hkbNodeInfo>**>(&animationGraph->behaviorGraph->activeNodes);
        if (!activeNodes) {
            return true;
        }

        const auto addClip = [&](RE::hkbClipGenerator* a_clip) {
            if (!a_clip || std::ranges::contains(a_clips, a_clip, &ActiveClip::generator)) {
                return;
            }
            const auto animationNameAddress = std::bit_cast<std::uintptr_t>(a_clip->animationName) & ~std::uintptr_t{ RE::hkStringPtr::kManaged };
            const auto animationName = reinterpret_cast<const char*>(animationNameAddress);
            a_clips.emplace_back(
                a_clip,
                animationName ? std::string{ animationName } : std::string{},
                a_clip->localTime,
                a_clip->animationControl ? a_clip->animationControl->weight : 1.0f);
        };

        for (const auto& activeNode : *activeNodes) {
            if (!activeNode.nodeClone) {
                continue;
            }
            if (const auto clip = skyrim_cast<RE::hkbClipGenerator*>(activeNode.nodeClone)) {
                addClip(clip);
            } else if (const auto synchronizedClip = skyrim_cast<RE::BSSynchronizedClipGenerator*>(activeNode.nodeClone)) {
                addClip(synchronizedClip->clipGenerator);
            }
        }
        return true;
    }

    void Instance::TryStartAnimations()
    {
        std::vector<std::vector<ActiveClip>> activeClips(pendingAnimations.size());

        // Wait until every pending actor is loaded and out of ragdoll
        for (size_t i = 0; i < pendingAnimations.size(); i++) {
            auto& pending = pendingAnimations[i];
            if (pending.transitionAcknowledged || pending.retryDelay > 0.0f) {
                continue;
            }
            pending.readinessChecks++;
            const auto actor = pending.actor;
            if (!actor || !actor->Is3DLoaded() || actor->IsInRagdollState()) {
                return;
            }
        }

        // Exit furniture and other active states once before waiting for stability
        if (!actorPreparationApplied) {
            for (auto& pending : pendingAnimations) {
                if (pending.transitionAcknowledged || pending.retryDelay > 0.0f) {
                    continue;
                }
                pending.actor->StopCombat();
                pending.actor->EndDialogue();
                pending.actor->InterruptCast(false);
                pending.actor->StopInteractingQuick(true);
                pending.actor->NotifyAnimationGraph("IdleFurnitureExit");
                pending.actor->NotifyAnimationGraph("AnimObjectUnequip");
                pending.actor->NotifyAnimationGraph("IdleStop");
                pending.actor->NotifyAnimationGraph("IdleForceDefaultState");
                if (const auto position = GetPosition(pending.actor); position && !position->data.IsHuman()) {
                    pending.actor->NotifyAnimationGraph("ReturnDefaultState");
                    pending.actor->NotifyAnimationGraph("ReturnToDefault");
                    pending.actor->NotifyAnimationGraph("FNISDefault");
                    pending.actor->NotifyAnimationGraph("IdleReturnToDefault");
                    pending.actor->NotifyAnimationGraph("ForceFurnExit");
                    pending.actor->NotifyAnimationGraph("Reset");
                }
                if (const auto process = pending.actor->GetActorRuntimeData().currentProcess) {
                    process->ClearMuzzleFlashes();
                }
            }
            actorPreparationApplied = true;
            return;
        }

        // Wait for requested activity exits to reach stable actor states
        for (size_t i = 0; i < pendingAnimations.size(); i++) {
            const auto& pending = pendingAnimations[i];
            if (pending.transitionAcknowledged || pending.retryDelay > 0.0f) {
                continue;
            }
            const auto actorState = pending.actor->AsActorState();
            if (actorState->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal || actorState->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal) {
                return;
            }
        }

        // Snapshot active clips so later updates can verify each transition
        for (size_t i = 0; i < pendingAnimations.size(); i++) {
            const auto& pending = pendingAnimations[i];
            if (pending.transitionAcknowledged || pending.retryDelay > 0.0f) {
                continue;
            }
            if (!GetActiveClips(pending.actor, activeClips[i])) {
                return;
            }
        }

        // Ask Papyrus to apply actor locks, then continue after its acknowledgement
        if (!actorLockAcknowledged) {
            if (!actorLockRequested) {
                const auto scriptObject = Script::GetScriptObject(linkedQst, "sslThreadModel");
                Script::CallbackPtr callbackPtr{};
                actorLockRequested = scriptObject && Script::DispatchMethodCall(scriptObject, "LockActorsForAnimation", callbackPtr);
                if (!actorLockRequested) {
                    logger::error("Failed to request Papyrus actor locks for thread {:X}.", linkedQst->GetFormID());
                }
            }
            return;
        }

        // Apply native locks and placement before dispatching each animation event
        for (size_t i = 0; i < pendingAnimations.size(); i++) {
            auto& pending = pendingAnimations[i];
            if (pending.transitionAcknowledged || pending.retryDelay > 0.0f) {
                continue;
            }
            const bool firstDispatch = pending.dispatchAttempts == 0;
            if (firstDispatch) {
                const auto& positionInfo = activeScene->GetNthPosition(pending.position);
                Collision::CollisionHandler::AddActor(pending.actor->GetFormID());
                Registry::Scale::GetSingleton()->SetScale(pending.actor, positionInfo->data.GetRace(), positionInfo->data.GetScale());
            }
            ReassertPlacement(pending.position, firstDispatch);
            pending.dispatchAttempts++;
            if (!pending.actor->NotifyAnimationGraph(pending.event)) {
                pending.retryDelay = 0.1f;
                continue;
            }
            pending.previousClips = std::move(activeClips[i]);
            pending.transitionAcknowledged = true;
            const auto readinessElapsed = pending.elapsed;
            pending.elapsed = 0.0f;
            logger::info("Actor {:X} accepted animation event {} after {} dispatch attempt(s), {} readiness check(s), and {:.3f}s.", pending.actor->GetFormID(), pending.event, pending.dispatchAttempts, pending.readinessChecks, readinessElapsed);
        }
    }

    bool Instance::HoldAnimation(PendingAnimation& a_pending)
    {
        RE::BSAnimationGraphManagerPtr graphManager;
        if (!a_pending.actor->GetAnimationGraphManager(graphManager) || !graphManager) {
            return false;
        }

        auto& runtime = graphManager->GetRuntimeData();
        RE::BSSpinLockGuard lock{ runtime.updateLock };
        if (runtime.activeGraph >= graphManager->graphs.size()) {
            return false;
        }

        const auto animationGraph = graphManager->graphs[runtime.activeGraph].get();
        if (!animationGraph || !animationGraph->behaviorGraph || !animationGraph->behaviorGraph->activeNodes) {
            return false;
        }

        for (const auto& activeNode : *animationGraph->behaviorGraph->activeNodes) {
            auto clip = skyrim_cast<RE::hkbClipGenerator*>(activeNode.nodeClone);
            if (!clip) {
                if (const auto synchronizedClip = skyrim_cast<RE::BSSynchronizedClipGenerator*>(activeNode.nodeClone)) {
                    clip = synchronizedClip->clipGenerator;
                }
            }
            if (clip != a_pending.observedGenerator) {
                continue;
            }
            clip->playbackSpeed = 0.0f;
            clip->localTime = clip->startTime;
            clip->time = clip->startTime;
            if (clip->animationControl) {
                clip->animationControl->playbackSpeed = 0.0f;
                clip->animationControl->localTime = clip->startTime;
            }
            a_pending.playbackHeld = true;
            return true;
        }
        return false;
    }

    void Instance::ReleaseAnimations()
    {
        std::vector<RE::BSAnimationGraphManagerPtr> graphManagers;
        std::vector<std::unique_ptr<RE::BSSpinLockGuard>> locks;
        graphManagers.reserve(pendingAnimations.size());
        locks.reserve(pendingAnimations.size());

        for (const auto& pending : pendingAnimations) {
            if (!pending.playbackHeld) {
                graphManagers.emplace_back();
                continue;
            }
            RE::BSAnimationGraphManagerPtr graphManager;
            if (!pending.actor->GetAnimationGraphManager(graphManager) || !graphManager) {
                graphManagers.emplace_back();
                continue;
            }
            graphManagers.emplace_back(graphManager);
            locks.emplace_back(std::make_unique<RE::BSSpinLockGuard>(graphManager->GetRuntimeData().updateLock));
        }

        for (size_t i = 0; i < pendingAnimations.size(); i++) {
            const auto& pending = pendingAnimations[i];
            const auto& graphManager = graphManagers[i];
            if (!pending.playbackHeld || !graphManager) {
                continue;
            }
            const auto& runtime = graphManager->GetRuntimeData();
            if (runtime.activeGraph >= graphManager->graphs.size()) {
                continue;
            }
            const auto animationGraph = graphManager->graphs[runtime.activeGraph].get();
            if (!animationGraph || !animationGraph->behaviorGraph || !animationGraph->behaviorGraph->activeNodes) {
                continue;
            }
            for (const auto& activeNode : *animationGraph->behaviorGraph->activeNodes) {
                auto clip = skyrim_cast<RE::hkbClipGenerator*>(activeNode.nodeClone);
                if (!clip) {
                    if (const auto synchronizedClip = skyrim_cast<RE::BSSynchronizedClipGenerator*>(activeNode.nodeClone)) {
                        clip = synchronizedClip->clipGenerator;
                    }
                }
                if (clip != pending.observedGenerator) {
                    continue;
                }
                clip->localTime = clip->startTime;
                clip->time = clip->startTime;
                clip->playbackSpeed = animationPlaybackSpeed;
                if (clip->animationControl) {
                    clip->animationControl->localTime = clip->startTime;
                    clip->animationControl->playbackSpeed = animationPlaybackSpeed;
                }
                break;
            }
        }
    }

    void Instance::UpdatePendingAnimations(float a_delta)
    {
        constexpr float retryTimeout = 15.0f;
        constexpr float playbackTimeout = 2.0f;
        constexpr float minimumClipWeight = 0.01f;
        constexpr float minimumTimeChange = 0.0001f;

        bool retryStart = false;
        for (auto& pending : pendingAnimations) {
            pending.elapsed += a_delta;
            if (!pending.transitionAcknowledged) {
                pending.retryDelay -= a_delta;
                retryStart |= pending.elapsed < retryTimeout && pending.retryDelay <= 0.0f;
            }
        }
        if (retryStart) {
            TryStartAnimations();
        }

        bool synchronizationFailed = false;
        for (auto& pending : pendingAnimations) {
            if (!pending.transitionAcknowledged) {
                if (pending.elapsed >= retryTimeout) {
                    logger::error("Actor {:X} did not accept animation event {} after {} dispatch attempt(s), {} readiness check(s), and {:.3f}s.", pending.actor->GetFormID(), pending.event, pending.dispatchAttempts, pending.readinessChecks, pending.elapsed);
                    synchronizationFailed = true;
                }
                continue;
            }
            if (pending.playbackHeld) {
                continue;
            }

            std::vector<ActiveClip> activeClips;
            if (GetActiveClips(pending.actor, activeClips)) {
                for (const auto& clip : activeClips) {
                    if (clip.animationName.empty() || clip.weight <= minimumClipWeight) {
                        continue;
                    }
                    const auto previous = std::ranges::find(pending.previousClips, clip.generator, &ActiveClip::generator);
                    const bool changed = previous == pending.previousClips.end() || previous->animationName != clip.animationName || clip.localTime + minimumTimeChange < previous->localTime;
                    if (!changed) {
                        continue;
                    }
                    if (pending.observedGenerator == clip.generator && pending.observedAnimation == clip.animationName && std::abs(clip.localTime - pending.observedLocalTime) > minimumTimeChange) {
                        if (HoldAnimation(pending)) {
                            logger::info("Actor {:X} held animation clip {} for synchronization.", pending.actor->GetFormID(), clip.animationName);
                        }
                        break;
                    }
                    pending.observedGenerator = clip.generator;
                    pending.observedAnimation = clip.animationName;
                    pending.observedLocalTime = clip.localTime;
                }
            }

            if (!pending.playbackHeld && pending.elapsed >= playbackTimeout) {
                logger::warn("Actor {:X} acknowledged animation event {}, but clip playback could not be verified.", pending.actor->GetFormID(), pending.event);
                synchronizationFailed = true;
            }
        }

        if (synchronizationFailed) {
            ReleaseAnimations();
            pendingAnimations.clear();
            const auto scriptObject = Script::GetScriptObject(linkedQst, "sslThreadModel");
            Script::CallbackPtr callbackPtr{};
            if (!scriptObject || !Script::DispatchMethodCall(scriptObject, "OnAnimationSyncFailed", callbackPtr)) {
                logger::error("Failed to notify Papyrus of animation synchronization failure for thread {:X}.", linkedQst->GetFormID());
            }
            return;
        }
        if (!pendingAnimations.empty() && std::ranges::all_of(pendingAnimations, &PendingAnimation::playbackHeld)) {
            for (const auto& pending : pendingAnimations) {
                ReassertPlacement(pending.position, false);
            }
            ReleaseAnimations();
            logger::info("Released {} synchronized animation clips.", pendingAnimations.size());
            pendingAnimations.clear();
        }
    }

    void Instance::SetAnimationPlaybackSpeed(float playbackSpeed)
    {
        animationPlaybackSpeed = playbackSpeed;
        std::vector<std::pair<RE::BSAnimationGraphManagerPtr, std::unique_ptr<RE::BSSpinLockGuard>>> lockedGraphs;

        for (auto& position : positions) {
            const auto* actor = position.data.GetActor();
            if (!actor) {
                continue;
            }

            RE::BSAnimationGraphManagerPtr graphMgr;
            if (!actor->GetAnimationGraphManager(graphMgr) || !graphMgr) {
                continue;
            }

            auto& runtime = graphMgr->GetRuntimeData();
            lockedGraphs.emplace_back(
                graphMgr,
                std::make_unique<RE::BSSpinLockGuard>(runtime.updateLock));
        }

        for (auto& [graphMgr, lock] : lockedGraphs) {
            auto& runtime = graphMgr->GetRuntimeData();
            auto activeGraph = runtime.activeGraph;

            RE::BShkbAnimationGraph* animationGraph = graphMgr->graphs[activeGraph].get();
            if (!animationGraph || !animationGraph->behaviorGraph) {
                continue;
            }

            auto* activeNodes = *reinterpret_cast<RE::hkArray<RE::hkbNodeInfo>**>(
                &animationGraph->behaviorGraph->activeNodes);
            if (!activeNodes) {
                continue;
            }

            for (const RE::hkbNodeInfo& activeNode : *activeNodes) {
                if (!activeNode.nodeClone) {
                    continue;
                }
                if (auto* clip = skyrim_cast<RE::hkbClipGenerator*>(activeNode.nodeClone)) {
                    const bool held = std::ranges::any_of(pendingAnimations, [&](const auto& pending) {
                        return pending.playbackHeld && pending.observedGenerator == clip;
                    });
                    clip->playbackSpeed = held ? 0.0f : playbackSpeed;
                    if (clip->animationControl) {
                        clip->animationControl->playbackSpeed = held ? 0.0f : playbackSpeed;
                    }
                }
            }
        }
    }
}  // namespace Thread
