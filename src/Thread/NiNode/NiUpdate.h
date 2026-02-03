#pragma once

#include "NiInstance.h"

namespace Thread::NiNode
{
	class NiUpdate
	{
	  public:
		static void Install();

		static std::shared_ptr<NiInstance> Register(RE::FormID a_id, std::vector<RE::Actor*> a_positions, const Registry::Scene* a_scene) noexcept;
		static void Unregister(RE::FormID a_id) noexcept;

	  private:
		static void UpdatePlayer(RE::PlayerCharacter* player, float delta);
		static inline REL::Relocation<decltype(UpdatePlayer)> _UpdatePlayer;

		static inline float time = 0.0f;
		static inline std::mutex _m{};
		static inline std::vector<std::pair<RE::FormID, std::shared_ptr<NiInstance>>> instances{};
	};

}  // namespace Thread::NiNode
