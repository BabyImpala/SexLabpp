#include "NiUpdate.h"

#include <SimpleIni.h>

#include "NiDescriptor.h"

namespace Thread::Interaction::NiML
{
    void NiUpdate::Install()
    {
        if (!InitializeDescriptors()) {
            logger::critical("Failed to initialize descriptors. NiNode interactions will not work.");
            assert(false && "Failed to initialize descriptors");
            return;
        }

        auto& trampoline = SKSE::GetTrampoline();
        REL::Relocation<std::uintptr_t> update{ REL::RelocationID(35565, 36564), REL::VariantOffset(0x53, 0x6E, 0x68) };
        _OnFrameUpdate = trampoline.write_call<5>(update.address(), OnFrameUpdate);
    }

    float NiUpdate::GetDeltaTime()
    {
        static REL::Relocation<float*> deltaTime{ REL::VariantID(523660, 410199, 0x30C3A08) };
        return *deltaTime.get();
    }

    bool NiUpdate::InitializeDescriptors()
    {
        if (!fs::exists(MODELPATH)) {
            logger::error("Descriptors: Settings file not found at {}", MODELPATH);
            return false;
        }

        CSimpleIniA inifile{};
        inifile.SetUnicode();
        const auto ec = inifile.LoadFile(MODELPATH);
        if (ec < 0) {
            logger::error("Descriptors: Failed to read .ini file, Error: {}", ec);
            return false;
        }

        try {
#define NI_TYPE(name, cluster) \
    NiDescriptor<NiType::Type::name>::Initialize(inifile);

#include "NiType.def"

#undef NI_TYPE

            logger::info("Descriptors: Model initialization complete");
            return true;
        } catch (const std::exception& e) {
            logger::error("Descriptors: Initialization failed - {}", e.what());
        }
        return false;
    }

    void NiUpdate::OnFrameUpdate(RE::PlayerCharacter* a_this)
    {
        _OnFrameUpdate(a_this);

        static auto calendar = RE::Calendar::GetSingleton();
        static auto lastGameHour = 0.0f;
        const auto currentGameHour = calendar->GetHour();
        if (currentGameHour == lastGameHour) {
            return;
        }
        lastGameHour = currentGameHour;

        std::scoped_lock mlLk{ _mlMutex };
        const bool isMLTraining = mlTrainingState.type != NiType::Type::None;

        std::scoped_lock lk{ _m };
        time += GetDeltaTime();
        for (auto&& [_, process] : _instances) {
            process->Update(time);
            if (!isMLTraining || !process->HasActor(a_this->GetFormID()))
                continue;
            if (++mlTrainingState.frameCount < mlTrainingState.frameInterval) {
                continue;
            }
            mlTrainingState.frameCount = 0;
            process->ForEachCluster([&](RE::ActorPtr a, RE::ActorPtr b, const NiInteractionCluster& cluster) {
                if (!a->IsPlayerRef() && !b->IsPlayerRef()) {
                    return;  // only log interactions involving the player & interaction has likelihood
                } else if (!cluster.IsValid()) {
                    return;  // skip logging if no interactions detected in cluster
                } else if (mlTrainingState.recordedData.empty()) {
                    const auto msg = std::format("ML Training: Starting new recording session for interaction type {}", magic_enum::enum_name(mlTrainingState.type));
                    logger::info("{}", msg);
                    Util::PrintConsole(msg);
                    const auto headerStr = std::format("ActorA,ActorB,{},Label", cluster.GetCsvFeatureHeader());
                    logger::info("ML Training: CSV Header - {}", headerStr);
                    mlTrainingState.recordedData.push_back(headerStr);
                }
                const auto csvRow = cluster.GetCsvFeatureRow();
                if (csvRow.empty()) {
                    logger::error("ML Training: Empty CSV row for interaction type {}", magic_enum::enum_name(mlTrainingState.type));
                    return;  // skip logging if no features detected in interaction
                }
                const auto actorAId = a->GetFormID();
                const auto actorBId = b->GetFormID();
                const auto labelStr = mlTrainingState.enabled ? magic_enum::enum_name(mlTrainingState.type) : "0";
                const auto row = std::format("{:X},{:X},{},{}", actorAId, actorBId, csvRow, labelStr);
                mlTrainingState.recordedData.push_back(row);
            },
                0, 0, NiType::GetClusterForType(mlTrainingState.type));
        }
    }

    std::shared_ptr<NiInstance> NiUpdate::Register(RE::FormID a_id, std::vector<RE::Actor*> a_positions, const Registry::Scene* a_scene) noexcept
    {
        try {
            std::scoped_lock lk{ _m };
            const auto where = std::ranges::find(_instances, a_id, [](auto& it) { return it.first; });
            if (where != _instances.end()) {
                logger::info("Object with ID {:X} already registered. Resetting NiInstance.", a_id);
                std::swap(*where, _instances.back());
                _instances.pop_back();
            }
            auto process = std::make_shared<NiInstance>(a_positions, a_scene);
            return _instances.emplace_back(a_id, process).second;
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
        const auto where = std::ranges::find(_instances, a_id, [](auto& it) { return it.first; });
        if (where == _instances.end()) {
            logger::error("No object registered using ID {:X}", a_id);
            return;
        }
        _instances.erase(where);
    }

    void NiUpdate::UpdateMLTrainingState(NiType::Type a_type, bool enabled)
    {
        std::scoped_lock lk{ _mlMutex };
        const auto switchType = mlTrainingState.type != a_type;
        if (!mlTrainingState.recordedData.empty() && switchType) {
            const auto activeTypeStr = magic_enum::enum_name(mlTrainingState.type);
            const auto numRows = mlTrainingState.recordedData.size();
            if (a_type == NiType::Type::None) {
                const auto msg = std::format("ML Training State: Stopping recording for interaction type {}, saving {} rows of data", activeTypeStr, numRows);
                logger::info("{}", msg);
                Util::PrintConsole(msg);
            } else {
                const auto msg = std::format("ML Training State: Switching from {} to {}, saving {} rows of data", activeTypeStr, magic_enum::enum_name(a_type), numRows);
                logger::info("{}", msg);
                Util::PrintConsole(msg);
            }
            const auto activeClusterStr = magic_enum::enum_name(NiType::GetClusterForType(mlTrainingState.type));
            const auto folderPath = std::format("{}\\{}", MODELDATAPATH, activeClusterStr);
            if (!fs::exists(folderPath)) {
                fs::create_directories(folderPath);
            }
            const auto filePath = std::format("{}\\ML_TrainingData_{}.csv", folderPath, std::chrono::system_clock::now().time_since_epoch().count());

            std::ofstream outFile(filePath);
            if (outFile.is_open()) {
                const auto csvFile = std::ranges::fold_left(mlTrainingState.recordedData, "", [](std::string&& acc, const std::string& row) {
                    return acc.empty() ? row : std::move(acc) + "\n" + row;
                });
                outFile << csvFile;
                outFile.close();
                const auto msg = std::format("ML Training State: Saved training data to {}", filePath);
                logger::info("{}", msg);
                Util::PrintConsole(msg);
            } else {
                const auto err = std::format("ML Training State: Failed to save training data to {}", filePath);
                RE::DebugMessageBox(err.c_str());
                logger::error("{}", err);
                Util::PrintConsole(err);
            }
            mlTrainingState.recordedData.clear();
        }
        mlTrainingState.type = a_type;
        mlTrainingState.enabled = enabled;
        mlTrainingState.frameCount = 0;  // reset frame count when changing state
        const auto msg = std::format("ML Training State: Updated training to type: {}. Enabled? {}", magic_enum::enum_name(a_type), enabled);
        logger::info("{}", msg);
        Util::PrintConsole(msg);
    }

    void NiUpdate::SetMLTrainingFrameInterval(size_t interval)
    {
        std::scoped_lock lk{ _mlMutex };
        mlTrainingState.frameInterval = interval;
        const auto msg = std::format("ML Training State: Frame interval set to {} frames", interval);
        logger::info("{}", msg);
        Util::PrintConsole(msg);
    }

    void NiUpdate::ClearMLTrainingData()
    {
        std::scoped_lock lk{ _mlMutex };
        const auto dataSize = mlTrainingState.recordedData.size();
        mlTrainingState.recordedData.clear();
        const auto msg = std::format("ML Training State: Cleared training data, removed {} rows", dataSize);
        logger::info("{}", msg);
        Util::PrintConsole(msg);
    }

    bool NiUpdate::IsMLTrainingEnabled()
    {
        std::scoped_lock lk{ _mlMutex };
        return mlTrainingState.type != NiType::Type::None;
    }

    NiUpdate::MLTrainingState NiUpdate::GetMLTrainingState()
    {
        std::scoped_lock lk{ _mlMutex };
        return mlTrainingState;
    }

}  // namespace Thread::Interaction::NiML
