#pragma once
#include "Thread/Interface/PrismaSceneMenu.h"

namespace Thread::PrismaUI
{
    class SceneSelectorMenu
    {
      public:
        static void Init();
        static void Destroy();

        static void PopulateScenes();

        static void OnSceneSelected(const std::string& sceneId);
        static void OnSceneResetBySearch(const std::string& query);
        static void OnAnnotationEdited(const std::string& data);

      private:
        struct SceneEntry
        {
            std::string id;
            std::string name;
            std::string packageName;
            std::string author;
            std::string tags;
            std::string annotations;
            bool isActive{ false };
        };

        inline static bool isOverlayVisible{ false };

        static std::string BuildScenesJson();
    };

}  // namespace Thread::PrismaUI
