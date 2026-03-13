#pragma once

#include "Registry/Define/RaceKey.h"
#include "Registry/Define/Sex.h"
#include "Registry/Validation.h"
#include "SexLabPPlusAPI.h"
#include "Thread/Thread.h"


namespace SLPP
{
	class ISceneInstance_Impl : public ISceneInstance
	{
	  public:
		ISceneInstance_Impl(InstanceID a_id) : id(a_id) {}
		~ISceneInstance_Impl() override = default;

		InstanceID GetInstanceID() override { return id; }

		std::string GetActiveStage() override
		{
			auto* instance = Thread::Instance::GetInstance(id);
			if (instance)
				return instance->GetActiveStage()->id;
			return "";
		}

		std::vector<std::string> GetAllStages() override
		{
			std::vector<std::string> stages;
			auto* instance = Thread::Instance::GetInstance(id);
			auto scene = instance->GetActiveScene();
			if (instance && scene)
				for (const auto& stage : scene->GetAllStages())
					stages.push_back(stage->id);
			return stages;
		}

		std::string GetActiveScene() override
		{
			auto* instance = Thread::Instance::GetInstance(id);
			if (instance)
				return instance->GetActiveScene()->id;
			return "";
		}

		std::vector<std::string> GetAvailableScenes() override
		{
			std::vector<std::string> scenes;
			auto* instance = Thread::Instance::GetInstance(id);
			if (instance)
				for (const auto& scene : instance->GetThreadScenes())
					scenes.push_back(scene->id);
			return scenes;
		}

		void SetActiveStage(std::string stage_id) override
		{
			auto* instance = Thread::Instance::GetInstance(id);
			instance->AdvanceScene(instance->GetActiveScene()->GetStageByID(stage_id));
		}

		void SetActiveScene(std::string scene_id) override
		{
			auto* instance = Thread::Instance::GetInstance(id);
			auto scenes = instance->GetThreadScenes();
			for (const auto& scene : scenes) {
				if (scene->id == scene_id) {
					instance->SetActiveScene(scene);
					return;
				}
			}
		}

		std::vector<std::string> GetStageTags() override
		{
			std::vector<std::string> tags;
			auto* instance = Thread::Instance::GetInstance(id);
			auto stage = instance->GetActiveStage();
			if (instance && stage)
				for (const auto& tag : stage->tags.AsVector())
					tags.push_back(std::string(tag));
			return tags;
		}

		std::vector<std::string> GetSceneTags() override
		{
			std::vector<std::string> tags;
			auto* instance = Thread::Instance::GetInstance(id);
			auto scene = instance->GetActiveScene();
			if (instance && scene)
				for (const auto& tag : scene->tags.AsVector())
					tags.push_back(std::string(tag));
			return tags;
		}

		bool StageHasTag(std::string tag) override
		{
			auto* instance = Thread::Instance::GetInstance(id);
			auto stage = instance->GetActiveStage();
			if (instance && stage)
				return stage->tags.HasTag(RE::BSFixedString(tag.data()));
			return false;
		}

		bool SceneHasTag(std::string tag) override
		{
			auto* instance = Thread::Instance::GetInstance(id);
			auto scene = instance->GetActiveScene();
			if (instance && scene)
				return scene->tags.HasTag(RE::BSFixedString(tag.data()));
			return false;
		}

		void EndScene() override
		{
			Thread::Instance::DestroyInstance(id);
		}

		std::vector<RE::Actor*> GetActors() override
		{
			std::vector<RE::Actor*> actors;
			auto* instance = Thread::Instance::GetInstance(id);
			if (instance)
				for (auto& actor : instance->GetActors())
					actors.push_back(actor);
			return actors;
		}

		SEX GetSex(RE::Actor* actor) override
		{
			SEX sex;
			bool isHuman = true;
			if (Settings::bCreatureGender)
				isHuman = Registry::RaceKey(actor).Is(Registry::RaceKey::Value::Human);
			switch (Registry::GetSex(actor)) {
			case Registry::Sex::Female:
				sex = isHuman ? SEX::Female : SEX::CreatureFemale;
				break;
			case Registry::Sex::Male:
				sex = isHuman ? SEX::Male : SEX::CreatureMale;
				break;
			case Registry::Sex::Futa:
				sex = SEX::Futa;
				break;
			default:
				sex = SEX::None;
			}
			return sex;
		}

		bool GetAutoPlay() override
		{
			auto* instance = Thread::Instance::GetInstance(id);
			if (instance)
				return instance->GetAutoplayEnabled();
			return false;
		}

		void SetAutoPlay(bool value) override
		{
			auto* instance = Thread::Instance::GetInstance(id);
			if (instance)
				instance->SetAutoplayEnabled(value);
		}

		void SetAnimationSpeed(float speed) override
		{
			auto* instance = Thread::Instance::GetInstance(id);
			if (instance)
				instance->SetAnimationPlaybackSpeed(speed);
		}

		bool IsGhostMode(RE::Actor* actor) override
		{
			auto* instance = Thread::Instance::GetInstance(id);
			if (instance)
				return instance->IsGhostMode(actor);
			return false;
		}

		void SetGhostMode(RE::Actor* actor, bool value) override
		{
			auto* instance = Thread::Instance::GetInstance(id);
			if (instance)
				instance->SetGhostMode(actor, value);
		}

	  private:
		InstanceID id;
	};

	inline std::unordered_map<std::string, ISexLabPPlusAPI::SceneEventCallback> sceneEventListeners;
	inline std::unordered_map<std::string, ISexLabPPlusAPI::InteractioneEventCallback> interactionEventListeners;
	inline std::unordered_map<InstanceID, ISceneInstance_Impl*> instanceMap;

	class SexLabPPlusAPI_Impl : public ISexLabPPlusAPI
	{
	  public:
		SexLabPPlusAPI_Impl() = default;
		~SexLabPPlusAPI_Impl() override = default;

		std::uint32_t GetVersion() const override { return kAPIVersion; }

		void RegisterSceneEventListener(SceneEventCallback a_callback, std::string a_source) override
		{
			sceneEventListeners.emplace(std::move(a_source), std::move(a_callback));
		}

		void UnregisterSceneEventListener(std::string a_source) override
		{
			sceneEventListeners.erase(a_source);
		}

		void RegisterInteractionEventListener(InteractioneEventCallback a_callback, std::string a_source) override
		{
			interactionEventListeners.emplace(std::move(a_source), std::move(a_callback));
		}

		void UnregisterInteractionEventListener(std::string a_source) override
		{
			interactionEventListeners.erase(a_source);
		}

		std::int32_t ValidateActor(RE::Actor* actor) override
		{
			if (!actor)
				return -1;
			return Registry::IsValidActorImpl(actor);
		}

		ISceneInstance* StartScene(const std::vector<RE::Actor*> a_actors, FurniturePreference a_furniturePreference) override
		{
			if (a_actors.empty())
				return nullptr;

			Thread::Instance::SceneMapping scenes{};
			Thread::Instance::FurniturePreference furniturePref = Thread::Instance::FurniturePreference::Disallow;
			switch (a_furniturePreference) {
			case FurniturePreference::Disallow:
				furniturePref = Thread::Instance::FurniturePreference::Disallow;
				break;
			case FurniturePreference::Allow:
				furniturePref = Thread::Instance::FurniturePreference::Allow;
				break;
			case FurniturePreference::Prefer:
				furniturePref = Thread::Instance::FurniturePreference::Prefer;
				break;
			}

			InstanceID id = reinterpret_cast<InstanceID>(a_actors[0]);
			if (Thread::Instance::CreateInstance(id, a_actors, scenes, furniturePref)) {
				return instanceMap[id];
			}
			return nullptr;
		}
	};

	inline void DispatchSceneEvent(SceneEvent a_event, InstanceID id, RE::Actor* a_actor = nullptr)
	{
		ISceneInstance* instance = instanceMap[id];
		for (const auto& [_, callback] : sceneEventListeners) {
			callback(a_event, instance, a_actor);
		}
	}

	inline void DispatchInteractionEvent(InteractioneEvent a_event, RE::Actor* a_actor, RE::Actor* a_partner)
	{
		if (a_event == InteractioneEvent::None || !a_actor || !a_partner) {
			return;
		}
		for (const auto& [_, callback] : interactionEventListeners) {
			callback(a_event, a_actor, a_partner);
		}
	}
}