#include "NiInstance.h"

#include "InteractionType.h"
#include "UserData/Settings.h"

namespace Thread::NiNode
{
	NiInstance::NiInstance(const std::vector<RE::Actor*>& a_positions, const Registry::Scene* a_scene)
	{
		positions.reserve(a_positions.size());
		states.reserve(a_positions.size() * (a_positions.size() - 1));
		for (size_t i = 0; i < a_positions.size(); i++) {
			const auto sex = a_scene->GetNthPosition(i)->data.GetSex().get();
			positions.emplace_back(a_positions[i], sex);
			for (size_t n = 0; n < a_positions.size(); n++) {
				states.emplace_back(std::make_pair(i, n), PairInteractionState{});
			}
		}
	}

	void NiInstance::Update(float a_timeStamp)
	{
		for (auto& niActor : positions) {
			niActor.CaptureSnapshot(a_timeStamp);
		}

		for (auto& [pair, state] : states) {
			auto& posA = positions[pair.first];
			auto& posB = positions[pair.second];

			EvaluateRuleBased(state, posA, posB, a_timeStamp);
			UpdateHysteresis(state, a_timeStamp);
		}
	}

	void NiInstance::EvaluateRuleBased(PairInteractionState& state, const NiActor& a, const NiActor& b, float a_timeStamp) const
	{
		state.interactions[static_cast<size_t>(InteractionType::Type::Kissing)] = EvaluateKissing(a, b);
	}

	void NiInstance::UpdateHysteresis(PairInteractionState& state, float a_timeStamp)
	{
		const float delta = a_timeStamp - state.lastUpdateTime;
		state.lastUpdateTime = a_timeStamp;

		const auto types = magic_enum::enum_values<InteractionType::Type>();
		for (auto&& type : types) {
			auto& activeState = state.interactions[static_cast<size_t>(type)];
			const float confidence = activeState.confidence;
			const auto doActive = !activeState.active && confidence >= Settings::fEnterThreshold;
			const auto doInactive = activeState.active && confidence < Settings::fExitThreshold;
			if (doActive || doInactive) {
				activeState.active = doActive;
				activeState.duration = 0.0f;
			} else {
				activeState.duration += delta;
			}
		}
	}

}  // namespace Thread::NiNode
