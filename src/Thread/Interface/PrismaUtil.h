#pragma once
#include "PrismaUI_API.h"

namespace Thread::PrismaUI
{
    // ── VIEW CREATOR

    inline PRISMA_UI_API::IVPrismaUI2* PrismaAPI{ nullptr };

    template<typename ListenerRegistrar>
    inline bool CreatePrismaView(std::string_view filepath, PrismaView* a_view, ListenerRegistrar&& registerListeners)
    {
        if (!a_view) return false;
        if (!PrismaAPI) {
            PrismaAPI = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
            if (!PrismaAPI) {
                logger::error("CreatePrismaView >> PrismaUI unavailable for '{}'", filepath);
                return false;
            }
        }
        if (!PrismaAPI->IsValid(*a_view)) {
            *a_view = PrismaAPI->CreateView(filepath.data());
            if (!*a_view) {
                logger::error("CreatePrismaView >> CreateView returned null for '{}'", filepath);
                return false;
            }
            PrismaAPI->SetOrder(*a_view, 1000);
            PrismaAPI->Hide(*a_view);
            registerListeners();
            logger::info("CreatePrismaView >> Initialized '{}'", filepath);
        }
        return true;
    };

    // ── OVERLAY SUPPRESSOR

    class OverlaySuppressor : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
      public:
        static inline constexpr std::array SUPPRESSING_MENUS{
            // menus with pause effects on game
            RE::Console::MENU_NAME, RE::JournalMenu::MENU_NAME, RE::MainMenu::MENU_NAME, RE::TweenMenu::MENU_NAME,
            RE::MapMenu::MENU_NAME, RE::LoadingMenu::MENU_NAME, RE::MessageBoxMenu::MENU_NAME,
            RE::SleepWaitMenu::MENU_NAME, RE::RaceSexMenu::MENU_NAME,
            // menus that might get activated by user interactions
            RE::DialogueMenu::MENU_NAME, RE::BarterMenu::MENU_NAME, RE::ContainerMenu::MENU_NAME,
            RE::InventoryMenu::MENU_NAME, RE::MagicMenu::MENU_NAME, RE::FavoritesMenu::MENU_NAME,
            RE::GiftMenu::MENU_NAME, RE::FaderMenu::MENU_NAME,
            // other nicities
            RE::LevelUpMenu::MENU_NAME, RE::StatsMenu::MENU_NAME
        };

        static void Register(PrismaView a_view)
        {
            GetInstance().AddView(a_view);
        }

        static void Unregister(PrismaView a_view)
        {
            GetInstance().RemoveView(a_view);
        }

      private:
        struct Entry
        {
            PrismaView view;
            bool suppressedByMenu{ false };
        };

        std::vector<Entry> m_entries;
        std::mutex m_mutex;

        static OverlaySuppressor& GetInstance()
        {
            static OverlaySuppressor instance;
            return instance;
        }

        OverlaySuppressor()
        {
            RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(this);
        }

        void AddView(PrismaView a_view)
        {
            std::lock_guard lock(m_mutex);
            for (auto& e : m_entries)
                if (e.view == a_view)
                    return;
            m_entries.push_back({ a_view });
        }

        void RemoveView(PrismaView a_view)
        {
            std::lock_guard lock(m_mutex);
            std::erase_if(m_entries, [&](const Entry& e) { return e.view == a_view; });
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (!a_event || !PrismaAPI)
                return RE::BSEventNotifyControl::kContinue;

            const bool isSuppressing = std::ranges::any_of(SUPPRESSING_MENUS,
                [&](const auto& name) { return a_event->menuName == name; });

            if (!isSuppressing)
                return RE::BSEventNotifyControl::kContinue;

            std::lock_guard lock(m_mutex);
            for (auto& e : m_entries) {
                if (!PrismaAPI->IsValid(e.view))
                    continue;
                if (a_event->opening) {
                    if (!PrismaAPI->IsHidden(e.view)) {
                        e.suppressedByMenu = true;
                        PrismaAPI->Hide(e.view);
                    }
                } else {
                    if (e.suppressedByMenu) {
                        e.suppressedByMenu = false;
                        PrismaAPI->Show(e.view);
                    }
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    // ── VIEWS MANAGEMENT

    enum class PrismaOverlayIndex : int32_t
    {
        kPrismaSceneMenu        =  0,
        kAnimSpeedOverlay       =  1,
        kEnjoymentBars          =  2,
        kOffsetAdjustMenu       =  3,
        kSceneSelectorMenu      =  4,
        kThreadConfigMenu       =  5,
        kVisibilityControlMenu  =  6,
    };

    void RegisterPrismaViews();

    bool IsViewValid(PrismaView* a_view);
    void ShowView(PrismaView* a_view);
    void HideView(PrismaView* a_view);
    bool IsViewVisible(PrismaView* a_view);

    void OverlayInit(RE::TESQuest* a_qst, PrismaOverlayIndex index);
    void OverlayDestroy(PrismaOverlayIndex index);

    std::string JsonEscape(const std::string& s);

}  // namespace Thread::PrismaUI