#include "Papyrus/Papyrus.h"
#include "Registry/CumFx.h"
#include "Registry/Library.h"
#include "Registry/Stats.h"
#include "Serialization.h"
#include "SexLabPPlusAPI.h"
#include "SexLabPPlusAPI_Impl.h"
#include "Thread/Collision/CollisionHandler.h"
#include "Thread/Interface/SceneMenu.h"
#include "Thread/Interface/SelectionMenu.h"
#include "Thread/NiNode/NiUpdate.h"
#include "UserData/StripData.h"


// class EventHandler :
// 	public Singleton<EventHandler>,
// 	public RE::BSTEventSink<RE::BSAnimationGraphEvent>
// {
// public:
// 	using EventResult = RE::BSEventNotifyControl;

// 	void Register()
// 	{
// 		RE::PlayerCharacter::GetSingleton()->AddAnimationGraphEventSink(this);
// 	}

// public:
// 	EventResult ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
// 	{
// 		if (!a_event || a_event->holder->IsNot(RE::FormType::ActorCharacter))
// 			return EventResult::kContinue;

// 		auto source = const_cast<RE::Actor*>(a_event->holder->As<RE::Actor>());
// 		if (source->IsWeaponDrawn())
// 			logger::info("Tag = {} | Payload = {}", a_event->tag, a_event->payload);
// 		return EventResult::kContinue;
// 	}
// };

void APIMessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	if (a_msg->type == SLPP::InterfaceExchangeMessage::kExchangeInterface) {
		auto* msg = static_cast<SLPP::InterfaceExchangeMessage*>(a_msg->data);
		if (msg) {
			static SLPP::SexLabPPlusAPI_Impl singleton;
			msg->interfacePtr = &singleton;
			logger::info("[SexLab P+] API interface dispatched to {}", a_msg->sender ? a_msg->sender : "unknown");
		}
	}
}

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kPostLoad:
		{
			Settings::Initialize();
			Registry::CumFx::GetSingleton()->Initialize();
			auto* messaging = SKSE::GetMessagingInterface();
			if (messaging)
				messaging->RegisterListener(SLPP::kPluginName.data(), APIMessageHandler);
			break;
		}
	case SKSE::MessagingInterface::kDataLoaded:
		if (!GameForms::LoadData()) {
			logger::critical("Unable to load esp objects");
			const auto err =
			  "Some game objects could not be loaded. This is usually due to a required game plugin not being loaded in your game."
			  "See the SexLabUtil.log for more information about which form failed to load."
			  "\n\nExit Game now? (Recommended yes)";
			if (REX::W32::MessageBoxA(nullptr, err, "SexLab p+ Load Data", 0x00000004) == 6)
				std::_Exit(EXIT_FAILURE);
			return;
		}
		SKSE::AllocTrampoline(static_cast<size_t>(1) << 5);
		Thread::Collision::CollisionHandler::Install();
		Thread::NiNode::NiUpdate::Install();
		Registry::Library::GetSingleton()->Initialize();
		UserData::StripData::GetSingleton()->Load();
		Settings::InitializeData();
		break;
	case SKSE::MessagingInterface::kSaveGame:
		std::thread([]() {
			Settings::Save();
			Registry::Library::GetSingleton()->Save();
			UserData::StripData::GetSingleton()->Save();
		}).detach();
		break;
	case SKSE::MessagingInterface::kPostLoadGame:
		// EventHandler::GetSingleton()->Register();
		break;
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible");
		return false;
	}

	SKSE::Init(a_skse, true);
#ifndef NDEBUG
	spdlog::set_pattern("%s(%#): [%T] [%^%l%$] %v"s);
#else
	spdlog::set_pattern("[%T] [%^%l%$] %v"s);
#endif

	const auto msging = SKSE::GetMessagingInterface();
	if (!msging->RegisterListener("SKSE", SKSEMessageHandler)) {
		logger::critical("Failed to register Listener");
		return false;
	}

	if (!Papyrus::Register()) {
		logger::critical("Failed to register papyrus functions");
		return false;
	}

	Thread::Interface::SceneMenu::Register();
	Thread::Interface::SelectionMenu::Register();

	const auto serialization = SKSE::GetSerializationInterface();
	serialization->SetUniqueID('slpp');
	serialization->SetSaveCallback(Serialization::Serialize::SaveCallback);
	serialization->SetLoadCallback(Serialization::Serialize::LoadCallback);
	serialization->SetRevertCallback(Serialization::Serialize::RevertCallback);
	serialization->SetFormDeleteCallback(Serialization::Serialize::FormDeleteCallback);

	Registry::Statistics::StatisticsData::GetSingleton()->Register();

	logger::info("Initialization complete");

	return true;
}
