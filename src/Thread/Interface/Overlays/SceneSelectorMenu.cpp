#include "SceneSelectorMenu.h"
#include "Registry/Library.h"

namespace Thread::PrismaUI
{
    void SceneSelectorMenu::Init()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        const std::string json = BuildScenesJson();
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "ssm_initOverlay", json.c_str());
        isOverlayVisible = true;
    };

    void SceneSelectorMenu::Destroy()
    {
        if (!IsViewValid(&PrismaSceneMenu::psmView)) return;
        PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "ssm_destroyOverlay", "");
        isOverlayVisible = false;
    };

    // ── C++ to JS
    
    void SceneSelectorMenu::PopulateScenes()
    {
        if (!isOverlayVisible) return;
        const std::string json = BuildScenesJson();
        if (IsViewValid(&PrismaSceneMenu::psmView)) {
            PrismaAPI->InteropCall(PrismaSceneMenu::psmView, "ssm_populateScenes", json.c_str());
        }
    };

    // ── JS TO C++

    void SceneSelectorMenu::OnSceneSelected(const std::string& sceneId)
    {
        if (!PrismaSceneMenu::psm_linkedThread) return;
        Script::DispatchMethodCall(PrismaSceneMenu::psm_threadScript, "ResetScene", PrismaSceneMenu::psm_callbackPtr, RE::BSFixedString{ sceneId.c_str() });
        PopulateScenes();
    };

    void SceneSelectorMenu::OnSceneResetBySearch(const std::string& query)
    {
        if (!PrismaSceneMenu::psm_linkedThread) return;
        Script::DispatchMethodCall(PrismaSceneMenu::psm_threadScript, "OnSceneResetBySearch", PrismaSceneMenu::psm_callbackPtr, RE::BSFixedString{ query.c_str() });
        // should re-init the PrismaSceneMenu fully since the current thread instance is destroyed by this
    };

    void SceneSelectorMenu::OnAnnotationEdited(const std::string& data)
    {
        const auto sep = data.find('|');
        if (sep == std::string::npos)
            return;

        const RE::BSFixedString sceneId{ data.substr(0, sep).c_str() };
        const std::string annotText = data.substr(sep + 1);

        auto* lib = Registry::Library::GetSingleton();
        const auto* scene = lib->GetSceneById(sceneId);
        if (!scene) return;
        auto existing = scene->tags.GetAnnotations();
        for (const auto& a : existing) {
            lib->EditScene(sceneId, [&](Registry::Scene* s) {
                s->tags.RemoveAnnotation(a);
            });
        }
        std::istringstream ss(annotText);
        std::string token;
        while (std::getline(ss, token, ',')) {
            const auto start = token.find_first_not_of(' ');
            const auto end = token.find_last_not_of(' ');
            if (start != std::string::npos) {
                lib->EditScene(sceneId, [&](Registry::Scene* s) {
                    s->tags.AddAnnotation(RE::BSFixedString{ token.substr(start, end - start + 1).c_str() });
                });
            }
        }
        logger::info("SceneSelectorMenu >> annotations updated for scene '{}'", sceneId.c_str());
    };

    // ── HELPERS

    std::string SceneSelectorMenu::BuildScenesJson()
    {
        auto* instance = Instance::GetInstance(PrismaSceneMenu::psm_linkedThread);
        if (!instance) return "{}";

        const auto* activeScene = instance->GetActiveScene();
        const auto playingScenes = instance->GetThreadScenes();
        auto* lib = Registry::Library::GetSingleton();

        std::vector<SceneEntry> entries;
        entries.reserve(playingScenes.size());
        for (const auto* scene : playingScenes) {
            SceneEntry e;

            e.id = scene->id;
            e.name = scene->name;
            e.isActive = (scene == activeScene);
            const auto* pkg = lib->GetPackageFromScene(scene);
            if (pkg) {
                e.packageName = pkg->GetName().c_str();
                e.author      = pkg->GetAuthor().c_str();
            }
            const auto tagVec = scene->tags.AsVector();
            bool first = true;
            for (const auto& t : tagVec) {
                if (!first) e.tags += ", ";
                e.tags += t.c_str();
                first = false;
            }
            const auto annots = scene->tags.GetAnnotations();
            first = true;
            for (const auto& a : annots) {
                if (!first) e.annotations += ", ";
                e.annotations += a.c_str();
                first = false;
            }
            entries.push_back(std::move(e));
        }

        std::sort(entries.begin(), entries.end(), [](const SceneEntry& a, const SceneEntry& b) {
            if (a.isActive != b.isActive) return a.isActive;
            return a.name < b.name;
        });

        std::string json = "[";
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            if (i > 0) json += ',';
            json += '{';
            json += "\"id\":\""          + JsonEscape(e.id)          + "\",";
            json += "\"name\":\""        + JsonEscape(e.name)        + "\",";
            json += "\"package\":\""     + JsonEscape(e.packageName) + "\",";
            json += "\"author\":\""      + JsonEscape(e.author)      + "\",";
            json += "\"tags\":\""        + JsonEscape(e.tags)        + "\",";
            json += "\"annotations\":\"" + JsonEscape(e.annotations) + "\",";
            json += "\"isActive\":"      + std::string(e.isActive ? "true" : "false");
            json += '}';
        }
        json += ']';
        return json;
    };

}  // namespace Thread::PrismaUI
