#include "NiUpdate.h"

#include "NiMath.h"
#include "SexLabPPlusAPI_Impl.h"

namespace
{
	std::optional<SLPP::InteractioneEvent> ToInteractionEvent(Thread::NiNode::Interaction::Action a_action)
	{
		using Action = Thread::NiNode::Interaction::Action;
		using Event = SLPP::InteractioneEvent;

		switch (a_action) {
		case Action::Vaginal:
			return Event::Vaginal;
		case Action::Anal:
			return Event::Anal;
		case Action::Oral:
			return Event::Oral;
		case Action::Grinding:
			return Event::Grinding;
		case Action::Deepthroat:
			return Event::Deepthroat;
		case Action::Skullfuck:
			return Event::Skullfuck;
		case Action::LickingShaft:
			return Event::LickingShaft;
		case Action::FootJob:
			return Event::FootJob;
		case Action::HandJob:
			return Event::HandJob;
		case Action::Kissing:
			return Event::Kissing;
		case Action::Facial:
			return Event::Facial;
		case Action::AnimObjFace:
			return Event::AnimObjFace;
		case Action::ToeSucking:
			return Event::SuckingToes;
		case Action::None:
		case Action::Total:
			return std::nullopt;
		}

		return std::nullopt;
	}
}

namespace Thread::NiNode
{
	NiInstance::NiInstance(const std::vector<RE::Actor*>& a_positions, const Registry::Scene* a_scene) :
	  positions([&]() {
		  std::vector<NiNode::NiPosition> v{};
		  v.reserve(a_positions.size());
		  for (size_t i = 0; i < a_positions.size(); i++) {
			  auto& it = a_positions[i];
			  auto sex = a_scene->GetNthPosition(i)->data.GetSex().get();
			  v.emplace_back(it, sex);
		  }
		  return v;
	  }()) {}

	bool NiInstance::VisitPositions(std::function<bool(const NiPosition&)> a_visitor) const
	{
		std::scoped_lock lk{ _m };
		for (auto&& pos : positions) {
			if (a_visitor(pos))
				return true;
		}
		return false;
	}

	void NiInstance::UpdateInteractions(float a_delta)
	{
		std::vector<std::tuple<SLPP::InteractioneEvent, RE::Actor*, RE::Actor*>> pendingEvents{};
		std::unique_lock lk{ _m, std::defer_lock };
		if (!lk.try_lock()) {
			return;
		}
		std::vector<NiPosition::Snapshot> snapshots{};
		snapshots.reserve(positions.size());
		pendingEvents.reserve(positions.size());
		for (auto&& it : positions) {
			snapshots.emplace_back(it);
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
			auto nextInteractions = std::set<Interaction>{ snapshots[i].interactions.begin(), snapshots[i].interactions.end() };
			for (const auto& act : nextInteractions) {
				if (pos.interactions.find(act) != pos.interactions.end()) {
					continue;
				}
				auto event = ToInteractionEvent(act.action);
				if (!event) {
					continue;
				}
				auto* actor = pos.actor.get();
				auto* partner = act.partner.get();
				if (!actor || !partner) {
					continue;
				}
				pendingEvents.emplace_back(*event, actor, partner);
			}
			positions[i].interactions = std::move(nextInteractions);
		}
		lk.unlock();
		for (const auto& [event, actor, partner] : pendingEvents) {
			SLPP::DispatchInteractionEvent(event, actor, partner);
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
		const auto ms_passed = gameDaysPassed->value * 24 * 60'000;
		static float ms_passed_last = ms_passed;
		const auto delta = ms_passed - ms_passed_last;
		ms_passed_last = ms_passed;
		for (auto&& [_, process] : processes) {
			process->UpdateInteractions(delta);
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
			auto process = std::make_shared<NiInstance>(a_positions, a_scene);
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

}  // namespace Thread::NiNode
