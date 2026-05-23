#pragma once
#include "PrismaUtil.h"

namespace Thread::PrismaUI
{
    class FurnSelectionMenu
    {
      public:
        struct Item
        {
          private:
            std::string name;
            std::string value;

          public:
            Item(const std::string& a_name, const std::string& a_value) :
              name(a_name), value(a_value) {}
            const std::string& GetName() const { return name; }
            const std::string& GetValue() const { return value; }
        };

      public:
        static PrismaView* GetView() { return &fsmView; }

        static bool Register();
        static void Open(RE::TESQuest* a_qst, const std::vector<Item>& a_items);

      private:
        static inline constexpr std::string_view FILEPATH{ "SexLab\\FurnSelectionMenu.html" };
        static inline PrismaView fsmView{ 0 };
        static inline RE::TESQuest* fsm_linkedThread{ nullptr };
        static inline std::vector<Item> fsm_items{};

        static void HandleSelection(const std::string& data);
    };

}  // namespace Thread::PrismaUI
