#pragma once

#include "InteractionType.h"
#include "NiActor.h"
#include "Registry/Define/Animation.h"

namespace Thread::NiNode
{
	class NiInstance
	{
		struct PairInteractionState
		{
			std::array<InteractionType, InteractionType::NUM_TYPES> interactions{};

			// Time of last update
			float lastUpdateTime = 0.0f;
		};

	  public:
		NiInstance(const std::vector<RE::Actor*>& a_positions, const Registry::Scene* a_scene);
		~NiInstance() = default;

		void Update(float a_timeStamp);

	  private:
		void EvaluateRuleBased(PairInteractionState& state, const NiActor& a, const NiActor& b, float a_timeStamp) const;
		void UpdateHysteresis(PairInteractionState& state, float currentTime);

	  private:
		std::vector<NiActor> positions;
		std::vector<std::pair<std::pair<uint8_t, uint8_t>, PairInteractionState>> states;

		mutable std::mutex _m;
	};

}  // namespace Thread::NiNode
