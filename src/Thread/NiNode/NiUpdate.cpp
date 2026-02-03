#include "NiUpdate.h"

namespace Thread::NiNode
{
	void NiUpdate::Install()
	{
		REL::Relocation<std::uintptr_t> plu{ RE::PlayerCharacter::VTABLE[0] };
		_UpdatePlayer = plu.write_vfunc(0xAD, UpdatePlayer);
		logger::info("Registered Functions");
	}

	void NiUpdate::UpdatePlayer(RE::PlayerCharacter* player, float delta)
	{
		_UpdatePlayer(player, delta);
		std::scoped_lock lk{ _m };
		time += delta;
		for (auto&& [_, process] : instances) {
			process->Update(time);
		}
	}

	std::shared_ptr<NiInstance> NiUpdate::Register(RE::FormID a_id, std::vector<RE::Actor*> a_positions, const Registry::Scene* a_scene) noexcept
	{
		try {
			std::scoped_lock lk{ _m };
			const auto where = std::ranges::find(instances, a_id, [](auto& it) { return it.first; });
			if (where != instances.end()) {
				logger::info("Object with ID {:X} already registered. Resetting NiInstance.", a_id);
				std::swap(*where, instances.back());
				instances.pop_back();
			}
			auto process = std::make_shared<NiInstance>(a_positions, a_scene);
			return instances.emplace_back(a_id, process).second;
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
		std::scoped_lock lk{ _m };
		const auto where = std::ranges::find(instances, a_id, [](auto& it) { return it.first; });
		if (where == instances.end()) {
			logger::error("No object registered using ID {:X}", a_id);
			return;
		}
		instances.erase(where);
	}

}  // namespace Thread::NiNode

