#pragma once

namespace SLPP
{
	inline constexpr std::string_view kPluginName = "SexLabUtil";
	inline constexpr std::uint32_t kAPIVersion = 1;

	using InstanceID = RE::TESQuest*;

	enum class SceneEvent : std::uint8_t
	{
		SceneStart = 0,
		StageAdvanced,
		SceneEnd,
		ActorOrgasm,
		LeadInStart,
		LeadInEnd,
		StageStart,
		StageEnd,
		LastedStageStart,
		LastedStageEnd,
		AnimationChange,
		PositionChange,
		ActorsRelocated,
		ActorChangeStart,
		ActorChangeEnd,
		AnimationStart,
		AnimationEnd,

		Total
	};

	enum class SceneType : std::uint8_t
	{
		Primary = 0,
		LeadIn,
		Custom,
		Total
	};

	enum class SEX : std::uint8_t
	{
		None = 0,
		Male,
		Female,
		Futa,
		CreatureMale,
		CreatureFemale
	};

	enum class FurniturePreference : std::uint8_t
	{
		Disallow = 0,
		Allow = 1,
		Prefer = 2,
	};

	enum class PathingFlag : std::int8_t
	{
		Disable = -1,
		Default = 0,
		Force = 1,
	};

	enum class InteractioneEvent : std::int32_t
	{
		None = 0,
		Vaginal = 1,
		Anal = 2,
		Oral = 3,
		Grinding = 4,
		Deepthroat = 5,
		Skullfuck = 6,
		LickingShaft = 7,
		FootJob = 8,
		HandJob = 9,
		Kissing = 10,
		Facial = 11,
		AnimObjFace = 12,
		SuckingToes = 13,
	};

	enum class CumEffectType : std::int32_t
	{
		All = -1,
		Vaginal = 0,
		Anal = 1,
		Oral = 2,
	};

	struct SceneData
	{
		RE::Actor* actor{ nullptr };
		SEX sex{ SEX::Male };
		bool submissive{ false };
	};

	class ISceneInstance
	{
	  public:
		virtual ~ISceneInstance() = default;

		virtual InstanceID GetInstanceID() = 0;

		// get stage of the scene current playing
		virtual std::string GetActiveStage() = 0;
		// get all stages of the scene current playing
		virtual std::vector<std::string> GetAllStages() = 0;
		// get current scene playing
		virtual std::string GetActiveScene() = 0;
		// get all scenes available to the instance
		virtual std::vector<std::string> GetAvailableScenes() = 0;

		// set stage of the scene current playing
		virtual void SetActiveStage(std::string stage_id) = 0;
		// set scene want to play
		virtual void SetActiveScene(std::string scene_id) = 0;

		virtual std::vector<std::string> GetStageTags() = 0;
		virtual std::vector<std::string> GetSceneTags() = 0;

		virtual bool StageHasTag(std::string tag) = 0;
		virtual bool SceneHasTag(std::string tag) = 0;

		// warning: this will end the scene and free the instance
		// and this class will be invalid after this call
		virtual void EndScene() = 0;

		virtual std::vector<RE::Actor*> GetActors() = 0;
		virtual SEX GetSex(RE::Actor*) = 0;

		// whether the stage advanced automaticaly
		virtual bool GetAutoPlay() = 0;
		virtual void SetAutoPlay(bool value) = 0;

		// set animation speed multiplier
		virtual void SetAnimationSpeed(float speed) = 0;

		virtual bool IsGhostMode(RE::Actor* actor) = 0;
		virtual void SetGhostMode(RE::Actor* actor, bool value) = 0;
	};

	class ISexLabPPlusAPI
	{
	  public:
		using SceneEventCallback = std::function<void(SceneEvent, ISceneInstance*, RE::Actor* actor)>;
		using InteractioneEventCallback = std::function<void(InteractioneEvent, RE::Actor* actor, RE::Actor* partner)>;

		virtual ~ISexLabPPlusAPI() = default;

		virtual std::uint32_t GetVersion() const = 0;

		virtual std::int32_t ActiveAnimations() const { return 0; }

		virtual void RegisterSceneEventListener(SceneEventCallback a_callback, std::string a_source) = 0;
		virtual void UnregisterSceneEventListener(std::string a_source) = 0;

		virtual void RegisterInteractionEventListener(InteractioneEventCallback a_callback, std::string a_source) = 0;
		virtual void UnregisterInteractionEventListener(std::string a_source) = 0;

		virtual std::int32_t ValidateActor(RE::Actor*) = 0;
		virtual ISceneInstance* StartScene(const std::vector<RE::Actor*> a_actors, FurniturePreference a_furniturePreference) = 0;
	};

	struct InterfaceExchangeMessage
	{
		enum : std::uint32_t
		{
			kExchangeInterface = 'SLPP'
		};

		ISexLabPPlusAPI* interfacePtr = nullptr;
	};

	inline ISexLabPPlusAPI* GetAPI()
	{
		InterfaceExchangeMessage msg;
		const auto* messaging = SKSE::GetMessagingInterface();
		if (!messaging)
			return nullptr;

		messaging->Dispatch(InterfaceExchangeMessage::kExchangeInterface, &msg, sizeof(InterfaceExchangeMessage), kPluginName.data());

		return msg.interfacePtr;
	}
}