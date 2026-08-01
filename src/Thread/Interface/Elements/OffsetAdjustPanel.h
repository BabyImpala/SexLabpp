#pragma once

#include "Thread/Interface/SceneHUD.h"

namespace Thread::Interface
{
    class OffsetAdjustPanel final
    {
      public:
        void Open(SceneHUD& a_hud);
        void Close();
        void Render(SceneHUD& a_hud);
        void RefreshStageOffsets(SceneHUD& a_hud);

      private:
        struct TargetItem
        {
            RE::Actor* actor{};
            std::uint32_t formId{};
            std::size_t positionIndex{};
            std::string label;
            bool isCenter{ false };
        };

        struct AxisState
        {
            float value{};
            float baseline{};
            float dragStartValue{};
            bool hasBaseline{ false };
        };

        struct AxisDefinition
        {
            const char* label;
            Registry::CoordinateType coordinate;
            float displayRange;
        };

        void OnTargetSelected(SceneHUD& a_hud, std::size_t a_targetIndex);
        void OnSetOffset(SceneHUD& a_hud, Registry::CoordinateType a_axis, std::uint32_t a_targetId, float a_value);
        void OnResetOffsets(SceneHUD& a_hud);
        void OnSetAdjustStageOnly(SceneHUD& a_hud, bool a_state);

        void RefreshTargets(SceneHUD& a_hud);
        void RefreshValues(SceneHUD& a_hud);
        void RenderTargetPicker(SceneHUD& a_hud);
        void RenderAdjustmentPanel(SceneHUD& a_hud);
        bool OffsetTrack(UI::Scale& a_scale, const AxisDefinition& a_axis, AxisState& a_state, bool& a_draggingOut);

        static constexpr float kTranslationDisplayRange = 200.0f;
        static constexpr float kRotationDisplayRange = 180.0f;
        static constexpr std::array kAxisDefinitions{
            AxisDefinition{ "X", Registry::CoordinateType::X, kTranslationDisplayRange },
            AxisDefinition{ "Y", Registry::CoordinateType::Y, kTranslationDisplayRange },
            AxisDefinition{ "Z", Registry::CoordinateType::Z, kTranslationDisplayRange },
            AxisDefinition{ "R", Registry::CoordinateType::R, kRotationDisplayRange },
        };

        std::vector<TargetItem> _targets;
        std::array<AxisState, kAxisDefinitions.size()> _axes{};
        std::optional<std::size_t> _selectedTarget;
        std::optional<std::size_t> _draggingAxis;
        bool _hasFurnitureCenter{ false };
        bool _adjustStageOnly{ false };
    };
}
