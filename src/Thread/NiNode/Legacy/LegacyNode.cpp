#include "LegacyNode.h"

#include "Registry/Define/RaceKey.h"
#include "Registry/Define/Transform.h"

namespace Thread::LegacyNiNode::Node
{
    namespace
    {
        constexpr std::size_t SAMPLES_PER_LANDMARK{ 4 };
        constexpr std::size_t MAX_CANDIDATES_PER_LANDMARK{ 32 };
        constexpr std::size_t SHAFT_SECTION_COUNT{ 4 };
        constexpr std::size_t SHAFT_RING_SAMPLES{ 6 };
        constexpr std::size_t SHAFT_TIP_SAMPLES{ 4 };
        constexpr std::size_t MAX_SHAFT_RING_CANDIDATES{ 32 };
        constexpr std::uint8_t SHAFT_EQUIPMENT_STABLE_FRAMES{ 2 };

        struct Candidate
        {
            std::uint16_t vertex;
            float weight;
            RE::NiPoint3 local;
        };

        struct CachedInfluence
        {
            std::uint16_t skinIndex;
            float weight;
        };

        struct CachedSample
        {
            std::uint16_t vertex;
            std::vector<CachedInfluence> influences;
        };

        struct SurfaceTopology
        {
            std::array<std::vector<CachedSample>, 2> landmarks;
            std::uint32_t vertexCount;
            std::uint32_t boneCount;
            std::uint32_t stride;
            std::uint16_t vertexFlags;
            std::array<std::uint16_t, 2> targetBones;
            std::array<std::uint16_t, 2> targetVertexCounts;
            std::uint64_t fingerprint;
        };

        struct ShaftRingTopology
        {
            std::vector<CachedSample> samples;
        };

        struct ShaftTopology
        {
            std::vector<ShaftRingTopology> rings;
            std::vector<CachedSample> tip;
            std::vector<std::uint16_t> chainBones;
            std::vector<std::uint16_t> chainVertexCounts;
            std::uint32_t vertexCount;
            std::uint32_t boneCount;
            std::uint32_t stride;
            std::uint16_t vertexFlags;
            std::uint64_t fingerprint;
        };

        struct ShaftWeights
        {
            std::vector<std::uint16_t> chainBones;
            std::size_t depth;
            float coverage;
        };

        struct ShaftCandidate
        {
            std::uint16_t vertex;
            float chainWeight;
            RE::NiPoint3 local;
            RE::NiPoint3 world;
            std::vector<CachedInfluence> influences;
        };

        struct SurfaceWeights
        {
            std::array<std::uint16_t, 2> bones;
            std::array<std::uint16_t, 2> counts;
            std::array<float, 2> maximum;
        };

        struct SurfaceSelection
        {
            std::string vaginal;
            std::string anal;
        };

        std::unordered_map<std::string, std::vector<SurfaceTopology>> assetTopologyCache;
        std::unordered_map<std::uint64_t, SurfaceTopology> fingerprintTopologyCache;
        std::unordered_map<std::string, SurfaceSelection> surfaceSelectionCache;
        std::unordered_map<std::string, std::vector<ShaftTopology>> shaftAssetTopologyCache;
        std::unordered_map<std::uint64_t, ShaftTopology> shaftFingerprintTopologyCache;
        std::unordered_map<std::string, std::string> shaftSelectionCache;

        void HashValue(std::uint64_t& a_hash, std::uint64_t a_value)
        {
            a_hash ^= a_value + 0x9E3779B97F4A7C15ULL + (a_hash << 6) + (a_hash >> 2);
        }

        // Equipped-part pointer identity is a cheap signal that Skyrim has finished replacing actor geometry.
        std::uint64_t GetBipedSignature(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return 0;
            }
            const auto& biped = a_actor->GetBiped();
            if (!biped) {
                return 0;
            }
            std::uint64_t result = 0xCBF29CE484222325ULL;
            HashValue(result, reinterpret_cast<std::uintptr_t>(biped.get()));
            for (const auto& object : biped->objects) {
                HashValue(result, reinterpret_cast<std::uintptr_t>(object.partClone.get()));
                HashValue(result, reinterpret_cast<std::uintptr_t>(object.part));
                HashValue(result, reinterpret_cast<std::uintptr_t>(object.addon));
            }
            return result;
        }

        // asset paths are case insensitive and may use either slash style
        std::string NormalizeAssetPath(std::string_view a_path)
        {
            std::string result(a_path);
            for (auto& c : result) {
                if (c == '\\') {
                    c = '/';
                } else if (c >= 'A' && c <= 'Z') {
                    c += 'a' - 'A';
                }
            }
            return result;
        }

        std::string MakeAssetTopologyKey(std::string_view a_modelPath, std::string_view a_geometryName, std::string_view a_openingName)
        {
            if (a_modelPath.empty()) {
                return {};
            }
            auto result = NormalizeAssetPath(a_modelPath);
            result.reserve(result.size() + a_geometryName.size() + a_openingName.size() + 2);
            result.push_back('\n');
            result.append(a_geometryName);
            result.push_back('\n');
            result.append(a_openingName);
            return result;
        }

        std::string MakeShaftSelectionKey(std::string_view a_modelPath, std::string_view a_baseName)
        {
            if (a_modelPath.empty()) {
                return {};
            }
            auto result = NormalizeAssetPath(a_modelPath);
            result.reserve(result.size() + a_baseName.size() + 1);
            result.push_back('\n');
            result.append(a_baseName);
            return result;
        }

        std::optional<SurfaceSelection> GetSurfaceSelection(std::string_view a_modelPath)
        {
            const auto key = NormalizeAssetPath(a_modelPath);
            if (const auto cached = surfaceSelectionCache.find(key); cached != surfaceSelectionCache.end()) {
                return cached->second;
            }
            return std::nullopt;
        }

        void CacheSurfaceSelection(std::string_view a_modelPath, std::string_view a_openingName, std::string_view a_geometryName)
        {
            if (a_modelPath.empty()) {
                return;
            }
            auto& selection = surfaceSelectionCache[NormalizeAssetPath(a_modelPath)];
            (a_openingName == "vaginal" ? selection.vaginal : selection.anal) = a_geometryName;
        }

        std::optional<std::string> GetShaftSelection(std::string_view a_modelPath, std::string_view a_baseName)
        {
            const auto key = MakeShaftSelectionKey(a_modelPath, a_baseName);
            if (key.empty()) {
                return std::nullopt;
            }
            if (const auto cached = shaftSelectionCache.find(key); cached != shaftSelectionCache.end()) {
                return cached->second;
            }
            return std::nullopt;
        }

        void CacheShaftSelection(std::string_view a_modelPath, std::string_view a_baseName, std::string_view a_geometryName)
        {
            const auto key = MakeShaftSelectionKey(a_modelPath, a_baseName);
            if (key.empty()) {
                return;
            }
            shaftSelectionCache[key] = a_geometryName;
        }

        bool ShouldUseGeometry(RE::BSGeometry* a_geometry)
        {
            const auto& runtime = a_geometry->GetGeometryRuntimeData();
            if (!runtime.skinInstance || !runtime.shaderProperty) {
                return false;
            }

            using Flag = RE::BSShaderProperty::EShaderPropertyFlag;
            const auto& flags = runtime.shaderProperty->flags;
            return !flags.any(Flag::kHairTint, Flag::kEyeReflect, Flag::kEffectLighting, Flag::kSoftEffect, Flag::kDecal, Flag::kDynamicDecal);
        }

        std::optional<SurfaceWeights> GetSurfaceWeights(RE::BSGeometry* a_geometry, const std::array<RE::NiAVObject*, 2>& a_targets)
        {
            if (!ShouldUseGeometry(a_geometry)) {
                return std::nullopt;
            }

            auto* skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
            auto* skinData = skin ? skin->skinData.get() : nullptr;
            auto* skinPartition = skin ? skin->skinPartition.get() : nullptr;
            if (!skinData || !skinPartition || !skin->bones || skinPartition->numPartitions == 0 || skinPartition->vertexCount == 0) {
                return std::nullopt;
            }

            SurfaceWeights result{};
            const auto boneCount = std::min(skin->numMatrices, skinData->GetBoneCount());
            for (std::size_t landmark = 0; landmark < a_targets.size(); ++landmark) {
                const auto boneIndex = [&]() -> std::optional<std::uint16_t> {
                    for (std::uint32_t i = 0; i < boneCount; ++i) {
                        if (skin->bones[i] == a_targets[landmark]) {
                            return static_cast<std::uint16_t>(i);
                        }
                    }
                    return std::nullopt;
                }();
                if (!boneIndex) {
                    return std::nullopt;
                }

                result.bones[landmark] = *boneIndex;
                const auto* weightedVertices = skinData->GetBoneDataBoneVertData(*boneIndex);
                result.counts[landmark] = skinData->GetBoneDataVerts(*boneIndex);
                if (!weightedVertices) {
                    return std::nullopt;
                }
                for (std::uint16_t i = 0; i < result.counts[landmark]; ++i) {
                    const auto& vertex = weightedVertices[i];
                    if (vertex.vert < skinPartition->vertexCount) {
                        result.maximum[landmark] = std::max(result.maximum[landmark], vertex.weight);
                    }
                }
                if (result.maximum[landmark] <= FLT_EPSILON) {
                    return std::nullopt;
                }
            }
            return result;
        }

        std::optional<ShaftWeights> GetShaftWeights(RE::BSGeometry* a_geometry, RE::NiAVObject* a_base)
        {
            if (!ShouldUseGeometry(a_geometry) || !a_base) {
                return std::nullopt;
            }
            auto* skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
            auto* skinData = skin ? skin->skinData.get() : nullptr;
            auto* partition = skin ? skin->skinPartition.get() : nullptr;
            if (!skinData || !partition || !skin->bones || partition->vertexCount == 0) {
                return std::nullopt;
            }

            struct Descendant
            {
                std::uint16_t skinIndex;
                std::size_t depth;
                float distanceSq;
            };
            std::vector<Descendant> descendants;
            const auto boneCount = std::min(skin->numMatrices, skinData->GetBoneCount());
            for (std::uint32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
                auto* bone = skin->bones[boneIndex];
                if (!bone || skinData->GetBoneDataVerts(boneIndex) == 0 || !skinData->GetBoneDataBoneVertData(boneIndex)) {
                    continue;
                }
                std::size_t depth = 0;
                for (auto* current = bone; current; current = current->parent, ++depth) {
                    if (current == a_base) {
                        descendants.push_back({ static_cast<std::uint16_t>(boneIndex), depth, (bone->world.translate - a_base->world.translate).SqrLength() });
                        break;
                    }
                }
            }
            if (descendants.empty()) {
                return std::nullopt;
            }

            // A skin can contain side branches. The deepest, then farthest, descendant identifies the shaft branch.
            const Descendant* distal = std::addressof(descendants.front());
            for (const auto& descendant : descendants) {
                if (descendant.depth > distal->depth || (descendant.depth == distal->depth && descendant.distanceSq > distal->distanceSq)) {
                    distal = std::addressof(descendant);
                }
            }
            std::vector<std::uint16_t> chain;
            for (auto* current = skin->bones[distal->skinIndex]; current; current = current->parent) {
                if (const auto match = std::ranges::find_if(descendants, [&](const Descendant& a_descendant) { return skin->bones[a_descendant.skinIndex] == current; }); match != descendants.end()) {
                    chain.push_back(match->skinIndex);
                }
                if (current == a_base) {
                    break;
                }
            }
            std::ranges::reverse(chain);
            if (chain.empty()) {
                return std::nullopt;
            }

            std::size_t weightedVertices = 0;
            for (const auto boneIndex : chain) {
                weightedVertices += skinData->GetBoneDataVerts(boneIndex);
            }
            return ShaftWeights{ std::move(chain), distal->depth, static_cast<float>(weightedVertices) / static_cast<float>(partition->vertexCount) };
        }

        std::string_view GetModelPath(RE::Actor* a_actor, RE::BSGeometry* a_geometry)
        {
            const auto& biped = a_actor->GetBiped();
            if (!biped) {
                return {};
            }
            for (const auto& object : biped->objects) {
                if (!object.part || !object.partClone) {
                    continue;
                }
                for (auto* current = static_cast<RE::NiAVObject*>(a_geometry); current; current = current->parent) {
                    if (current == object.partClone.get()) {
                        const auto* path = object.part->GetModel();
                        return path ? std::string_view(path) : std::string_view{};
                    }
                }
            }
            return {};
        }

        bool ReadCandidateLocalPositions(RE::BSGeometry* a_geometry, RE::NiSkinPartition* a_partition, std::array<std::vector<Candidate>, 2>& a_candidates)
        {
            if (auto* dynamicShape = netimmerse_cast<RE::BSDynamicTriShape*>(a_geometry)) {
                auto& dynamicData = dynamicShape->GetDynamicTrishapeRuntimeData();
                if (dynamicData.dynamicData) {
                    RE::BSSpinLockGuard lock(dynamicData.lock);
                    const auto* vertices = static_cast<const DirectX::XMVECTOR*>(dynamicData.dynamicData);
                    for (auto& candidates : a_candidates) {
                        for (auto& candidate : candidates) {
                            DirectX::XMFLOAT3 position;
                            DirectX::XMStoreFloat3(&position, vertices[candidate.vertex]);
                            candidate.local = { position.x, position.y, position.z };
                        }
                    }
                    return true;
                }
            }

            auto& partition = a_partition->partitions[0];
            if (!partition.vertexDesc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX) || !partition.buffData || !partition.buffData->rawVertexData) {
                return false;
            }

            const auto stride = partition.vertexDesc.GetSize();
            const auto* vertexBuffer = reinterpret_cast<const std::uint8_t*>(partition.buffData->rawVertexData);
            for (auto& candidates : a_candidates) {
                for (auto& candidate : candidates) {
                    const auto* vertex = vertexBuffer + candidate.vertex * stride;
                    candidate.local = {
                        *reinterpret_cast<const float*>(vertex),
                        *reinterpret_cast<const float*>(vertex + 4),
                        *reinterpret_cast<const float*>(vertex + 8)
                    };
                }
            }
            return true;
        }

        bool ReadShaftCandidateLocalPositions(RE::BSGeometry* a_geometry, RE::NiSkinPartition* a_partition, std::vector<ShaftCandidate>& a_candidates)
        {
            if (auto* dynamicShape = netimmerse_cast<RE::BSDynamicTriShape*>(a_geometry)) {
                auto& dynamicData = dynamicShape->GetDynamicTrishapeRuntimeData();
                if (dynamicData.dynamicData) {
                    RE::BSSpinLockGuard lock(dynamicData.lock);
                    const auto* vertices = static_cast<const DirectX::XMVECTOR*>(dynamicData.dynamicData);
                    for (auto& candidate : a_candidates) {
                        DirectX::XMFLOAT3 position;
                        DirectX::XMStoreFloat3(&position, vertices[candidate.vertex]);
                        candidate.local = { position.x, position.y, position.z };
                    }
                    return true;
                }
            }

            auto& partition = a_partition->partitions[0];
            if (!partition.vertexDesc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX) || !partition.buffData || !partition.buffData->rawVertexData) {
                return false;
            }
            const auto stride = partition.vertexDesc.GetSize();
            const auto* vertexBuffer = reinterpret_cast<const std::uint8_t*>(partition.buffData->rawVertexData);
            for (auto& candidate : a_candidates) {
                const auto* vertex = vertexBuffer + candidate.vertex * stride;
                candidate.local = {
                    *reinterpret_cast<const float*>(vertex),
                    *reinterpret_cast<const float*>(vertex + 4),
                    *reinterpret_cast<const float*>(vertex + 8)
                };
            }
            return true;
        }

        std::vector<CachedSample> SelectShaftRingSamples(const std::vector<ShaftCandidate>& a_candidates, const RE::NiPoint3& a_center, RE::NiPoint3 a_tangent)
        {
            if (a_candidates.empty() || a_tangent.SqrLength() <= FLT_EPSILON) {
                return {};
            }
            a_tangent.Unitize();
            std::vector<const ShaftCandidate*> pool;
            pool.reserve(a_candidates.size());
            for (const auto& candidate : a_candidates) {
                pool.push_back(std::addressof(candidate));
            }
            std::ranges::sort(pool, [&](const ShaftCandidate* a_lhs, const ShaftCandidate* a_rhs) {
                const auto score = [&](const ShaftCandidate* a_candidate) {
                    return std::abs((a_candidate->world - a_center).Dot(a_tangent)) / std::max(a_candidate->chainWeight, 0.05f);
                };
                return score(a_lhs) < score(a_rhs);
            });
            if (pool.size() > MAX_SHAFT_RING_CANDIDATES) {
                pool.resize(MAX_SHAFT_RING_CANDIDATES);
            }

            // Start near the bone plane, then spread samples around the circumference.
            std::vector<const ShaftCandidate*> selected{ pool.front() };
            while (selected.size() < std::min(SHAFT_RING_SAMPLES, pool.size())) {
                const ShaftCandidate* best = nullptr;
                float bestScore = -1.0f;
                for (const auto* candidate : pool) {
                    if (std::ranges::contains(selected, candidate)) {
                        continue;
                    }
                    const auto candidateOffset = candidate->world - a_center;
                    const auto candidateRadial = candidateOffset - a_tangent * candidateOffset.Dot(a_tangent);
                    float distanceSq = std::numeric_limits<float>::max();
                    for (const auto* sample : selected) {
                        const auto sampleOffset = sample->world - a_center;
                        const auto sampleRadial = sampleOffset - a_tangent * sampleOffset.Dot(a_tangent);
                        distanceSq = std::min(distanceSq, (candidateRadial - sampleRadial).SqrLength());
                    }
                    const auto score = distanceSq * candidate->chainWeight;
                    if (score > bestScore) {
                        best = candidate;
                        bestScore = score;
                    }
                }
                if (!best) {
                    break;
                }
                selected.push_back(best);
            }

            std::vector<CachedSample> result;
            result.reserve(selected.size());
            for (const auto* sample : selected) {
                result.push_back({ sample->vertex, sample->influences });
            }
            return result;
        }

        std::vector<CachedSample> SelectShaftTipSamples(const std::vector<ShaftCandidate>& a_candidates, const RE::NiPoint3& a_center, RE::NiPoint3 a_tangent)
        {
            if (a_candidates.empty() || a_tangent.SqrLength() <= FLT_EPSILON) {
                return {};
            }
            a_tangent.Unitize();
            std::vector<const ShaftCandidate*> sorted;
            sorted.reserve(a_candidates.size());
            for (const auto& candidate : a_candidates) {
                sorted.push_back(std::addressof(candidate));
            }
            std::ranges::sort(sorted, [&](const ShaftCandidate* a_lhs, const ShaftCandidate* a_rhs) {
                return (a_lhs->world - a_center).Dot(a_tangent) > (a_rhs->world - a_center).Dot(a_tangent);
            });
            std::vector<CachedSample> result;
            result.reserve(std::min(SHAFT_TIP_SAMPLES, sorted.size()));
            for (std::size_t i = 0; i < std::min(SHAFT_TIP_SAMPLES, sorted.size()); ++i) {
                result.push_back({ sorted[i]->vertex, sorted[i]->influences });
            }
            return result;
        }

        std::vector<std::uint16_t> SelectSamples(const std::vector<Candidate>& a_candidates)
        {
            std::vector<const Candidate*> selected;
            selected.reserve(std::min(SAMPLES_PER_LANDMARK, a_candidates.size()));
            selected.push_back(std::addressof(a_candidates.front()));

            while (selected.size() < std::min(SAMPLES_PER_LANDMARK, a_candidates.size())) {
                const Candidate* best = nullptr;
                float bestScore = -1.0f;
                for (const auto& candidate : a_candidates) {
                    if (std::ranges::any_of(selected, [&](const Candidate* a_selected) { return a_selected->vertex == candidate.vertex; })) {
                        continue;
                    }

                    float distanceSq = std::numeric_limits<float>::max();
                    for (const auto* sample : selected) {
                        distanceSq = std::min(distanceSq, (candidate.local - sample->local).SqrLength());
                    }
                    const auto score = distanceSq * candidate.weight;
                    if (score > bestScore) {
                        best = std::addressof(candidate);
                        bestScore = score;
                    }
                }
                if (!best) {
                    break;
                }
                selected.push_back(best);
            }
            return selected | std::views::transform([](const Candidate* a_candidate) { return a_candidate->vertex; }) | std::ranges::to<std::vector>();
        }

        bool MatchesTopology(const SurfaceTopology& a_topology, RE::NiSkinInstance* a_skin, RE::NiSkinData* a_skinData, RE::NiSkinPartition* a_partition, const std::array<RE::NiAVObject*, 2>& a_targets)
        {
            const auto boneCount = std::min(a_skin->numMatrices, a_skinData->GetBoneCount());
            auto& partition = a_partition->partitions[0];
            if (a_topology.vertexCount != a_partition->vertexCount || a_topology.boneCount != boneCount || a_topology.stride != partition.vertexDesc.GetSize() ||
                a_topology.vertexFlags != static_cast<std::uint16_t>(partition.vertexDesc.GetFlags())) {
                return false;
            }
            for (std::size_t landmark = 0; landmark < a_targets.size(); ++landmark) {
                const auto boneIndex = a_topology.targetBones[landmark];
                if (a_topology.landmarks[landmark].empty() || boneIndex >= boneCount || a_skin->bones[boneIndex] != a_targets[landmark] ||
                    a_topology.targetVertexCounts[landmark] != a_skinData->GetBoneDataVerts(boneIndex)) {
                    return false;
                }
                for (const auto& sample : a_topology.landmarks[landmark]) {
                    if (sample.vertex >= a_partition->vertexCount || sample.influences.empty() ||
                        std::ranges::any_of(sample.influences, [&](const CachedInfluence& a_influence) { return a_influence.skinIndex >= boneCount; })) {
                        return false;
                    }
                }
            }
            return true;
        }

        bool MatchesShaftTopology(const ShaftTopology& a_topology, RE::NiSkinInstance* a_skin, RE::NiSkinData* a_skinData, RE::NiSkinPartition* a_partition, RE::NiAVObject* a_base)
        {
            const auto boneCount = std::min(a_skin->numMatrices, a_skinData->GetBoneCount());
            auto& partition = a_partition->partitions[0];
            if (!a_base || a_topology.rings.size() < 2 || a_topology.tip.empty() || a_topology.chainBones.size() != a_topology.chainVertexCounts.size() ||
                a_topology.vertexCount != a_partition->vertexCount || a_topology.boneCount != boneCount || a_topology.stride != partition.vertexDesc.GetSize() ||
                a_topology.vertexFlags != static_cast<std::uint16_t>(partition.vertexDesc.GetFlags())) {
                return false;
            }
            for (std::size_t i = 0; i < a_topology.chainBones.size(); ++i) {
                const auto boneIndex = a_topology.chainBones[i];
                if (boneIndex >= boneCount || !a_skin->bones[boneIndex] || a_topology.chainVertexCounts[i] != a_skinData->GetBoneDataVerts(boneIndex)) {
                    return false;
                }
                auto* current = a_skin->bones[boneIndex];
                while (current && current != a_base) {
                    current = current->parent;
                }
                if (current != a_base) {
                    return false;
                }
            }
            const auto validSamples = [&](const auto& a_samples) {
                return !a_samples.empty() && std::ranges::all_of(a_samples, [&](const CachedSample& a_sample) {
                    return a_sample.vertex < a_partition->vertexCount && !a_sample.influences.empty() &&
                           std::ranges::all_of(a_sample.influences, [&](const CachedInfluence& a_influence) { return a_influence.skinIndex < boneCount; });
                });
            };
            return std::ranges::all_of(a_topology.rings, [&](const ShaftRingTopology& a_ring) { return validSamples(a_ring.samples); }) && validSamples(a_topology.tip);
        }

        std::optional<Opening> MakeNodeOpening(const NiMath::Segment& a_segment, const RE::NiPoint3& a_left, const RE::NiPoint3& a_right)
        {
            auto axis = a_segment.Vector();
            if (axis.SqrLength() <= FLT_EPSILON) {
                return std::nullopt;
            }
            axis.Unitize();

            auto right = a_right - a_left;
            right -= axis * right.Dot(axis);
            const auto diameter = right.Length();
            if (diameter <= FLT_EPSILON) {
                return std::nullopt;
            }
            right /= diameter;

            auto up = right.Cross(axis);
            if (up.SqrLength() <= FLT_EPSILON) {
                return std::nullopt;
            }
            up.Unitize();
            return Opening{ a_segment.first, a_segment.second, axis, right, up, diameter * 0.5f, false };
        }
    }

    bool SurfaceOpening::Bind(RE::BSGeometry* a_geometry, std::string_view a_modelPath, std::string_view a_name, const std::array<RE::NiAVObject*, 2>& a_targets, RE::NiAVObject* a_deep)
    {
        if (!a_deep || !ShouldUseGeometry(a_geometry)) {
            return false;
        }

        auto* skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
        auto* skinData = skin ? skin->skinData.get() : nullptr;
        auto* skinPartition = skin ? skin->skinPartition.get() : nullptr;
        if (!skinData || !skinPartition || !skin->bones || skinPartition->numPartitions == 0 || skinPartition->vertexCount == 0) {
            return false;
        }

        const auto boneCount = std::min(skin->numMatrices, skinData->GetBoneCount());
        const auto geometryName = std::string_view(a_geometry->name.c_str());
        const auto assetKey = MakeAssetTopologyKey(a_modelPath, geometryName, a_name);
        SurfaceTopology topology{};
        bool cacheHit = false;
        if (!assetKey.empty()) {
            if (const auto cached = assetTopologyCache.find(assetKey); cached != assetTopologyCache.end()) {
                for (const auto& variant : cached->second) {
                    if (MatchesTopology(variant, skin, skinData, skinPartition, a_targets)) {
                        topology = variant;
                        cacheHit = true;
                        break;
                    }
                }
            }
        }

        if (!cacheHit) {
            const auto surfaceWeights = GetSurfaceWeights(a_geometry, a_targets);
            if (!surfaceWeights) {
                return false;
            }

            std::array<std::vector<Candidate>, 2> candidates;
            std::uint64_t fingerprint = 0xCBF29CE484222325ULL;
            HashValue(fingerprint, skinPartition->vertexCount);
            HashValue(fingerprint, skinPartition->numPartitions);
            HashValue(fingerprint, skinPartition->partitions[0].vertexDesc.GetSize());
            HashValue(fingerprint, static_cast<std::uint16_t>(skinPartition->partitions[0].vertexDesc.GetFlags()));
            for (const auto value : { geometryName, a_modelPath, a_name }) {
                HashValue(fingerprint, value.size());
                for (const auto c : value) {
                    HashValue(fingerprint, static_cast<std::uint8_t>(c));
                }
            }

            for (std::size_t landmark = 0; landmark < a_targets.size(); ++landmark) {
                const auto boneIndex = surfaceWeights->bones[landmark];
                const auto* weightedVertices = skinData->GetBoneDataBoneVertData(boneIndex);
                const auto weightedVertexCount = skinData->GetBoneDataVerts(boneIndex);
                if (!weightedVertices || weightedVertexCount == 0) {
                    return false;
                }

                for (std::uint16_t i = 0; i < weightedVertexCount; ++i) {
                    const auto& vertex = weightedVertices[i];
                    if (vertex.vert < skinPartition->vertexCount && vertex.weight > FLT_EPSILON) {
                        candidates[landmark].push_back({ vertex.vert, vertex.weight, {} });
                    }
                }
                if (candidates[landmark].empty()) {
                    return false;
                }
                const auto keep = std::min(candidates[landmark].size(), MAX_CANDIDATES_PER_LANDMARK);
                std::partial_sort(candidates[landmark].begin(), candidates[landmark].begin() + keep, candidates[landmark].end(),
                    [](const Candidate& a_lhs, const Candidate& a_rhs) { return a_lhs.weight > a_rhs.weight; });
                candidates[landmark].resize(keep);

                HashValue(fingerprint, boneIndex);
                for (const auto& vertex : candidates[landmark]) {
                    HashValue(fingerprint, vertex.vertex);
                    HashValue(fingerprint, std::bit_cast<std::uint32_t>(vertex.weight));
                }
            }

            std::optional<SurfaceTopology> cachedTopology;
            {
                if (const auto cached = fingerprintTopologyCache.find(fingerprint); cached != fingerprintTopologyCache.end()) {
                    cachedTopology = cached->second;
                }
            }
            if (cachedTopology && MatchesTopology(*cachedTopology, skin, skinData, skinPartition, a_targets)) {
                topology = std::move(*cachedTopology);
                cacheHit = true;
            }

            if (!cacheHit) {
                if (!ReadCandidateLocalPositions(a_geometry, skinPartition, candidates)) {
                    return false;
                }
                topology.vertexCount = skinPartition->vertexCount;
                topology.boneCount = boneCount;
                topology.stride = skinPartition->partitions[0].vertexDesc.GetSize();
                topology.vertexFlags = static_cast<std::uint16_t>(skinPartition->partitions[0].vertexDesc.GetFlags());
                topology.targetBones = surfaceWeights->bones;
                topology.targetVertexCounts = surfaceWeights->counts;
                topology.fingerprint = fingerprint;
                for (std::size_t landmark = 0; landmark < candidates.size(); ++landmark) {
                    for (const auto vertex : SelectSamples(candidates[landmark])) {
                        topology.landmarks[landmark].push_back({ vertex, {} });
                    }
                }

                for (std::uint32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
                    const auto* weightedVertices = skinData->GetBoneDataBoneVertData(boneIndex);
                    if (!weightedVertices) {
                        continue;
                    }
                    for (std::uint16_t i = 0; i < skinData->GetBoneDataVerts(boneIndex); ++i) {
                        for (auto& landmark : topology.landmarks) {
                            for (auto& sample : landmark) {
                                if (sample.vertex == weightedVertices[i].vert) {
                                    sample.influences.push_back({ static_cast<std::uint16_t>(boneIndex), weightedVertices[i].weight });
                                }
                            }
                        }
                    }
                }

                if (!MatchesTopology(topology, skin, skinData, skinPartition, a_targets)) {
                    return false;
                }
                fingerprintTopologyCache.try_emplace(fingerprint, topology);
            }

            if (!assetKey.empty()) {
                auto& variants = assetTopologyCache[assetKey];
                if (std::ranges::none_of(variants, [&](const SurfaceTopology& a_variant) { return a_variant.fingerprint == topology.fingerprint; })) {
                    variants.push_back(topology);
                }
            }
        }

        landmarks = {};
        bones.clear();
        for (std::size_t landmark = 0; landmark < landmarks.size(); ++landmark) {
            landmarks[landmark].samples.reserve(topology.landmarks[landmark].size());
            for (const auto& cachedSample : topology.landmarks[landmark]) {
                Sample sample{ cachedSample.vertex, {}, {} };
                sample.influences.reserve(cachedSample.influences.size());
                for (const auto& influence : cachedSample.influences) {
                    auto bone = std::ranges::find(bones, influence.skinIndex, &Bone::skinIndex);
                    if (bone == bones.end()) {
                        bones.push_back({ influence.skinIndex, {} });
                        bone = std::prev(bones.end());
                    }
                    sample.influences.push_back({ static_cast<std::uint16_t>(std::distance(bones.begin(), bone)), influence.weight });
                }
                landmarks[landmark].samples.push_back(std::move(sample));
            }
        }

        const auto setLocalPositions = [&](const auto& a_getPosition) {
            for (auto& landmark : landmarks) {
                for (auto& sample : landmark.samples) {
                    sample.local = a_getPosition(sample.vertex);
                }
            }
        };
        if (auto* dynamicShape = netimmerse_cast<RE::BSDynamicTriShape*>(a_geometry); dynamicShape && dynamicShape->GetDynamicTrishapeRuntimeData().dynamicData) {
            auto& dynamicData = dynamicShape->GetDynamicTrishapeRuntimeData();
            RE::BSSpinLockGuard lock(dynamicData.lock);
            const auto* vertices = static_cast<const DirectX::XMVECTOR*>(dynamicData.dynamicData);
            setLocalPositions([&](std::uint16_t a_vertex) {
                DirectX::XMFLOAT3 position;
                DirectX::XMStoreFloat3(&position, vertices[a_vertex]);
                return RE::NiPoint3{ position.x, position.y, position.z };
            });
        } else {
            auto& partition = skinPartition->partitions[0];
            if (!partition.vertexDesc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX) || !partition.buffData || !partition.buffData->rawVertexData) {
                return false;
            }
            const auto stride = partition.vertexDesc.GetSize();
            const auto* vertexBuffer = reinterpret_cast<const std::uint8_t*>(partition.buffData->rawVertexData);
            setLocalPositions([&](std::uint16_t a_vertex) {
                const auto* vertex = vertexBuffer + a_vertex * stride;
                return RE::NiPoint3{
                    *reinterpret_cast<const float*>(vertex),
                    *reinterpret_cast<const float*>(vertex + 4),
                    *reinterpret_cast<const float*>(vertex + 8)
                };
            });
        }

        if (landmarks[0].samples.empty() || landmarks[1].samples.empty()) {
            return false;
        }

        geometry.reset(a_geometry);
        skinInstance = skin;
        deep.reset(a_deep);
        logger::info("Legacy {} surface topology {} for '{}' in '{}' ({:016X})", a_name, cacheHit ? "cache hit" : "cached", a_geometry->name, a_modelPath, topology.fingerprint);
        return true;
    }

    std::optional<Opening> SurfaceOpening::Update()
    {
        if (!geometry || !geometry->parent || !deep || geometry->GetGeometryRuntimeData().skinInstance.get() != skinInstance) {
            return std::nullopt;
        }

        auto* skinData = skinInstance->skinData.get();
        auto* skinPartition = skinInstance->skinPartition.get();
        if (!skinData || !skinPartition || !skinInstance->bones) {
            return std::nullopt;
        }

        if (auto* dynamicShape = netimmerse_cast<RE::BSDynamicTriShape*>(geometry.get())) {
            auto& dynamicData = dynamicShape->GetDynamicTrishapeRuntimeData();
            if (dynamicData.dynamicData) {
                RE::BSSpinLockGuard lock(dynamicData.lock);
                const auto* vertices = static_cast<const DirectX::XMVECTOR*>(dynamicData.dynamicData);
                for (auto& landmark : landmarks) {
                    for (auto& sample : landmark.samples) {
                        DirectX::XMFLOAT3 position;
                        DirectX::XMStoreFloat3(&position, vertices[sample.vertex]);
                        sample.local = { position.x, position.y, position.z };
                    }
                }
            }
        }

        for (auto& bone : bones) {
            if (bone.skinIndex >= skinInstance->numMatrices || !skinInstance->bones[bone.skinIndex]) {
                return std::nullopt;
            }
            bone.transform = skinInstance->bones[bone.skinIndex]->world * skinData->GetBoneDataSkinToBone(bone.skinIndex);
        }

        std::array<RE::NiPoint3, 2> points;
        for (std::size_t landmarkIndex = 0; landmarkIndex < landmarks.size(); ++landmarkIndex) {
            for (const auto& sample : landmarks[landmarkIndex].samples) {
                RE::NiPoint3 position{};
                float totalWeight = 0.0f;
                for (const auto& influence : sample.influences) {
                    position += (bones[influence.bone].transform * sample.local) * influence.weight;
                    totalWeight += influence.weight;
                }
                if (totalWeight > FLT_EPSILON) {
                    points[landmarkIndex] += position / totalWeight;
                }
            }
            points[landmarkIndex] /= static_cast<float>(landmarks[landmarkIndex].samples.size());
        }

        const auto center = (points[0] + points[1]) * 0.5f;
        auto axis = deep->world.translate - center;
        if (axis.SqrLength() <= FLT_EPSILON) {
            return std::nullopt;
        }
        axis.Unitize();

        auto right = points[1] - points[0];
        right -= axis * right.Dot(axis);
        const auto diameter = right.Length();
        if (diameter <= FLT_EPSILON) {
            return std::nullopt;
        }
        right /= diameter;

        auto up = right.Cross(axis);
        if (up.SqrLength() <= FLT_EPSILON) {
            return std::nullopt;
        }
        up.Unitize();
        return Opening{ center, deep->world.translate, axis, right, up, diameter * 0.5f, true };
    }

    NodeData::NodeData(RE::Actor* a_actor, bool a_forceSchlong)
    {
        const auto obj = a_actor->Get3D();
        if (!obj) {
            const auto msg = std::format("Unable to retrieve 3D of actor {:X}", a_actor->GetFormID());
            throw std::exception(msg.c_str());
        }
        const auto racekey = Registry::RaceKey(a_actor);
        const auto racestr = racekey.IsValid() ? racekey.AsString() : "?";
        const auto get = [&](auto str, auto& target, bool log) {
            auto node = obj->GetObjectByName(str);
            auto ninode = node ? node->AsNode() : nullptr;
            if (!ninode) {
                if (log)
                    logger::info("Actor {:X} (Race: {}) is missing Node {} (This may be expected)", a_actor->GetFormID(), racestr, str);
                return false;
            }
            target = RE::NiPointer{ ninode };
            return true;
        };
		// This is likely a mistake? It will always miss non-human actors
        if (!get(PELVIS, pelvis, true) || !get(SPINELOWER, spine_lower, true)) {
            throw std::exception("Missing mandatory 3d object (body)");
        }
        get(HEAD, head, true);
        get(HANDLEFTREF, hand_left, true);
        get(HANDRIGHTREF, hand_right, false);
        get(THUMBLEFT, thumb_left, false);
        get(THUMBRIGHT, thumb_right, false);
        get(FOOTLEFT, foot_left, true);
        get(FOOTRIGHT, foot_right, false);
        get(TOELEFT, toe_left, true);
        get(TOERIGHT, toe_right, true);
        get(CLITORIS, clitoris, true);
        get(VAGINADEEP, vaginadeep, true);
        get(VAGINAB, vaginab, false);
        get(VAGINALLEFT, vaginaleft, false);
        get(VAGINALRIGHT, vaginaright, false);
        get(ANALDEEP, analdeep, true);
        get(ANALLEFT, analleft, false);
        get(ANALRIGHT, analright, false);
        get(ANIMOBJECTA, animobj_a, false);
        get(ANIMOBJECTB, animobj_b, false);
        get(ANIMOBJECTL, animobj_l, false);
        get(ANIMOBJECTR, animobj_r, false);
        for (auto&& it : SCHLONG_NODES) {
            auto niavbase = obj->GetObjectByName(it.base);
            auto niobj = niavbase ? niavbase->AsNode() : nullptr;
            if (!niobj) {
                continue;
            }
            auto ptr = std::make_shared<SchlongData>(a_actor, RE::NiPointer{ niobj }, it.rot);
            schlongs.push_back(ptr);
        }
        logger::info("Legacy shaft discovery actor {:X}: configured bases found={}", a_actor->GetFormID(), schlongs.size());
        const std::array<RE::NiAVObject*, 2> vaginalTargets{ vaginaleft.get(), vaginaright.get() };
        const std::array<RE::NiAVObject*, 2> analTargets{ analleft.get(), analright.get() };
        const bool wantsVaginal = vaginadeep && std::ranges::all_of(vaginalTargets, [](auto* a_target) { return a_target != nullptr; });
        const bool wantsAnal = analdeep && std::ranges::all_of(analTargets, [](auto* a_target) { return a_target != nullptr; });

        const auto& biped = a_actor->GetBiped();
        if (biped && (wantsVaginal || wantsAnal)) {
            for (const auto& object : biped->objects) {
                if (!object.part || !object.partClone) {
                    continue;
                }
                const auto* path = object.part->GetModel();
                const auto modelPath = path ? std::string_view(path) : std::string_view{};
                const auto selection = GetSurfaceSelection(modelPath);
                if (!selection || ((!wantsVaginal || vaginalSurface || selection->vaginal.empty()) && (!wantsAnal || analSurface || selection->anal.empty()))) {
                    continue;
                }

                RE::BSVisit::TraverseScenegraphGeometries(object.partClone.get(), [&](RE::BSGeometry* a_geometry) {
                    const auto geometryName = std::string_view(a_geometry->name.c_str());
                    if (wantsVaginal && !vaginalSurface && geometryName == selection->vaginal) {
                        SurfaceOpening candidate;
                        if (candidate.Bind(a_geometry, modelPath, "vaginal", vaginalTargets, vaginadeep.get())) {
                            vaginalSurface = std::move(candidate);
                        }
                    }
                    if (wantsAnal && !analSurface && geometryName == selection->anal) {
                        SurfaceOpening candidate;
                        if (candidate.Bind(a_geometry, modelPath, "anal", analTargets, analdeep.get())) {
                            analSurface = std::move(candidate);
                        }
                    }
                    return (!wantsVaginal || vaginalSurface) && (!wantsAnal || analSurface) ? RE::BSVisit::BSVisitControl::kStop : RE::BSVisit::BSVisitControl::kContinue;
                });
                if ((!wantsVaginal || vaginalSurface) && (!wantsAnal || analSurface)) {
                    break;
                }
            }
        }

        RE::NiPointer<RE::BSGeometry> vaginalGeometry;
        RE::NiPointer<RE::BSGeometry> analGeometry;
        SurfaceWeights vaginalWeights{};
        SurfaceWeights analWeights{};
        if ((wantsVaginal && !vaginalSurface) || (wantsAnal && !analSurface)) {
            RE::BSVisit::TraverseScenegraphGeometries(obj, [&](RE::BSGeometry* a_geometry) {
                const auto consider = [&](const auto& a_targets, RE::NiPointer<RE::BSGeometry>& a_bestGeometry, SurfaceWeights& a_bestWeights) {
                    const auto weights = GetSurfaceWeights(a_geometry, a_targets);
                    if (!weights) {
                        return;
                    }
                    const auto score = std::min(weights->maximum[0], weights->maximum[1]);
                    const auto bestScore = std::min(a_bestWeights.maximum[0], a_bestWeights.maximum[1]);
                    const auto sum = weights->maximum[0] + weights->maximum[1];
                    const auto bestSum = a_bestWeights.maximum[0] + a_bestWeights.maximum[1];
                    bool preferModelPath = false;
                    if (a_bestGeometry && score == bestScore && sum == bestSum) {
                        preferModelPath = !GetModelPath(a_actor, a_geometry).empty() && GetModelPath(a_actor, a_bestGeometry.get()).empty();
                    }
                    if (!a_bestGeometry || score > bestScore || (score == bestScore && sum > bestSum) || preferModelPath) {
                        a_bestGeometry.reset(a_geometry);
                        a_bestWeights = *weights;
                    }
                };
                if (wantsVaginal && !vaginalSurface) {
                    consider(vaginalTargets, vaginalGeometry, vaginalWeights);
                }
                if (wantsAnal && !analSurface) {
                    consider(analTargets, analGeometry, analWeights);
                }
                return RE::BSVisit::BSVisitControl::kContinue;
            });
        }
        if (vaginalGeometry) {
            const auto modelPath = GetModelPath(a_actor, vaginalGeometry.get());
            logger::info("Legacy vaginal surface selected '{}' in '{}': max weights left={:.4f} ({} vertices), right={:.4f} ({} vertices)", vaginalGeometry->name,
                modelPath, vaginalWeights.maximum[0], vaginalWeights.counts[0], vaginalWeights.maximum[1], vaginalWeights.counts[1]);
            SurfaceOpening candidate;
            if (candidate.Bind(vaginalGeometry.get(), modelPath, "vaginal", vaginalTargets, vaginadeep.get())) {
                vaginalSurface = std::move(candidate);
                CacheSurfaceSelection(modelPath, "vaginal", std::string_view(vaginalGeometry->name.c_str()));
            }
        }
        if (analGeometry) {
            const auto modelPath = GetModelPath(a_actor, analGeometry.get());
            logger::info("Legacy anal surface selected '{}' in '{}': max weights left={:.4f} ({} vertices), right={:.4f} ({} vertices)", analGeometry->name,
                modelPath, analWeights.maximum[0], analWeights.counts[0], analWeights.maximum[1], analWeights.counts[1]);
            SurfaceOpening candidate;
            if (candidate.Bind(analGeometry.get(), modelPath, "anal", analTargets, analdeep.get())) {
                analSurface = std::move(candidate);
                CacheSurfaceSelection(modelPath, "anal", std::string_view(analGeometry->name.c_str()));
            }
        }
        logger::info("Actor {:X} skinned openings: vaginal={}, anal={}", a_actor->GetFormID(), vaginalSurface.has_value(), analSurface.has_value());
        if (a_forceSchlong && schlongs.empty()) {
        }
    }

    std::optional<NiMath::Segment> NodeData::GetVaginalSegment() const
    {
        if (!vaginadeep || !vaginaleft || !vaginaright)
            return std::nullopt;

        const auto start = (vaginaleft->world.translate + vaginaright->world.translate) / 2;
        const auto end = vaginadeep->world.translate;
        return NiMath::Segment{ start, end };
    }

    std::optional<NiMath::Segment> NodeData::GetAnalSegment() const
    {
        if (!analdeep || !analleft || !analright)
            return std::nullopt;

        const auto start = (analleft->world.translate + analright->world.translate) / 2;
        const auto end = analdeep->world.translate;
        return NiMath::Segment{ start, end };
    }

    std::optional<Opening> NodeData::GetVaginalOpening()
    {
        if (vaginalSurface) {
            if (auto opening = vaginalSurface->Update()) {
                return opening;
            }
        }
        const auto segment = GetVaginalSegment();
        return segment && vaginaleft && vaginaright ? MakeNodeOpening(*segment, vaginaleft->world.translate, vaginaright->world.translate) : std::nullopt;
    }

    std::optional<Opening> NodeData::GetAnalOpening()
    {
        if (analSurface) {
            if (auto opening = analSurface->Update()) {
                return opening;
            }
        }
        const auto segment = GetAnalSegment();
        return segment && analleft && analright ? MakeNodeOpening(*segment, analleft->world.translate, analright->world.translate) : std::nullopt;
    }

    void NodeData::UpdateSchlongs()
    {
        for (const auto& schlong : schlongs) {
            schlong->UpdateCollisionShape();
        }
    }

    NiMath::Segment NodeData::GetCrotchSegment() const
    {
        assert(pelvis && spine_lower);
        return { spine_lower->world.translate, pelvis->world.translate };
    }

    std::optional<RE::NiPoint3> NodeData::GetToeVectorLeft() const
    {
        if (!foot_left || !toe_left)
            return std::nullopt;
        return toe_left->world.translate - foot_left->world.translate;
    }

    std::optional<RE::NiPoint3> NodeData::GetToeVectorRight() const
    {
        if (!foot_right || !toe_right)
            return std::nullopt;
        return toe_right->world.translate - foot_right->world.translate;
    }

    NiMath::Segment NodeData::FakeSchlong::GetReferenceSegment() const
    {
        return { ApproximateBase(), ApproximateTip() };
    }

    RE::NiPointer<RE::NiNode> NodeData::FakeSchlong::GetBaseReferenceNode() const
    {
        return { nullptr };
    }

    RE::NiPoint3 NodeData::FakeSchlong::ApproximateNode(float a_forward, float a_upward) const
    {
        assert(ownerNodes.pelvis);
        const auto pelvisWorld = ownerNodes.pelvis->world;
        Registry::Coordinate approx(std::vector{ a_forward, 0.0f, a_upward, 0.0f });
        RE::NiPoint3 angle;
        pelvisWorld.rotate.ToEulerAnglesXYZ(angle);
        Registry::Coordinate ret{ pelvisWorld.translate, angle.z };
        approx.Apply(ret);
        return ret.AsNiPoint();
    }

    RE::NiPoint3 NodeData::FakeSchlong::ApproximateTip() const
    {
        constexpr float forward = 20.0f;
        constexpr float upward = -4.0f;
        return ApproximateNode(forward, upward);
    }

    RE::NiPoint3 NodeData::FakeSchlong::ApproximateMid() const
    {
        constexpr float forward = 15.0f;
        constexpr float upward = -6.2f;
        return ApproximateNode(forward, upward);
    }

    RE::NiPoint3 NodeData::FakeSchlong::ApproximateBase() const
    {
        constexpr float forward = 10.0f;
        constexpr float upward = -5.0f;
        return ApproximateNode(forward, upward);
    }

    NodeData::SchlongData::SchlongData(RE::Actor* a_actor, RE::NiPointer<RE::NiNode> a_basenode, const glm::mat3& a_rot) :
      actor(a_actor),
      nodes({ a_basenode }),
      rot({ a_rot[0].x, a_rot[0].y, a_rot[0].z }, { a_rot[1].x, a_rot[1].y, a_rot[1].z }, { a_rot[2].x, a_rot[2].y, a_rot[2].z }),
      equipmentSignature(GetBipedSignature(a_actor))
    {
        assert(a_basenode);
        FindSurface(a_actor);
        if (surface) {
            return;
        }

        // Mesh discovery failed, so preserve the previous skeleton-only chain as the compatibility fallback.
        do {
            auto parent = nodes.back();
            auto& childs = parent->GetChildren();
            switch (childs.size()) {
            case 0:
                break;
            case 1:
                {
                    auto& child = childs.front();
                    auto niobj = child ? child->AsNode() : nullptr;
                    if (niobj) {
                        nodes.emplace_back(niobj);
                    }
                }
                break;
            default:
                {
                    auto v1 = nodes.size() < 2 ? parent->world.rotate.GetVectorY() : parent->world.translate - a_basenode->world.translate;
                    if (v1.SqrLength() > FLT_EPSILON) {
                        for (auto&& child : childs) {
                            if (!child)
                                continue;
                            auto nichild = child->AsNode();
                            if (!nichild)
                                continue;
                            auto v2 = nichild->world.translate - a_basenode->world.translate;
                            auto angle = NiMath::GetAngleDegree(v1, v2);
                            if (angle <= 90.0f) {
                                nodes.emplace_back(nichild);
                                break;
                            }
                        }
                    } else {
                        auto user = a_basenode->GetUserData();
                        auto id = user ? user->GetFormID() : 0;
                        logger::error("Ambiguous Skeleton Structure for user {:X} at node depth {}", id, nodes.size());
                    }
                }
                break;
            }
            if (nodes.back() == parent)
                break;
        } while (true);
    }

    void NodeData::SchlongData::FindSurface(RE::Actor* a_actor)
    {
        if (!a_actor || nodes.empty()) {
            return;
        }
        auto* base = nodes.front().get();
        const auto baseName = std::string_view(base->name.c_str());
        const auto& biped = a_actor->GetBiped();
        if (biped) {
            for (const auto& object : biped->objects) {
                if (!object.part || !object.partClone) {
                    continue;
                }
                const auto* path = object.part->GetModel();
                const auto modelPath = path ? std::string_view(path) : std::string_view{};
                const auto selectedGeometry = GetShaftSelection(modelPath, baseName);
                if (!selectedGeometry) {
                    continue;
                }
                RE::BSVisit::TraverseScenegraphGeometries(object.partClone.get(), [&](RE::BSGeometry* a_geometry) {
                    if (std::string_view(a_geometry->name.c_str()) == std::string_view(selectedGeometry->data(), selectedGeometry->size()) && BindSurface(a_geometry, modelPath)) {
                        return RE::BSVisit::BSVisitControl::kStop;
                    }
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
                if (surface) {
                    return;
                }
            }
        }

        RE::NiPointer<RE::BSGeometry> bestGeometry;
        ShaftWeights bestWeights{};
        std::size_t geometryCount = 0;
        std::size_t skinnedGeometryCount = 0;
        std::size_t longestWeightedChain = 0;

        // Temporary validation for duplicate genital skeleton chains; remove after shaft discovery is verified.
        std::vector<RE::NiAVObject*> validationDescendants;
        RE::BSVisit::TraverseScenegraphObjects(base, [&](RE::NiAVObject* a_object) {
            if (a_object && !std::ranges::contains(validationDescendants, a_object)) {
                validationDescendants.push_back(a_object);
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        RE::BSVisit::TraverseScenegraphGeometries(a_actor->Get3D(), [&](RE::BSGeometry* a_geometry) {
            ++geometryCount;
            const auto& runtime = a_geometry->GetGeometryRuntimeData();
            if (runtime.skinInstance && runtime.skinInstance->skinData && runtime.skinInstance->skinPartition && runtime.skinInstance->bones) {
                ++skinnedGeometryCount;

                std::size_t pointerMatches = 0;
                std::size_t weightedPointerMatches = 0;
                std::size_t genitalNameMatches = 0;
                std::size_t weightedGenitalNameMatches = 0;
                bool selectedBaseReferenced = false;
                const auto boneCount = std::min(runtime.skinInstance->numMatrices, runtime.skinInstance->skinData->GetBoneCount());
                for (std::uint32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
                    auto* bone = runtime.skinInstance->bones[boneIndex];
                    if (!bone) {
                        continue;
                    }
                    const bool weighted = runtime.skinInstance->skinData->GetBoneDataVerts(boneIndex) > 0 &&
                                          runtime.skinInstance->skinData->GetBoneDataBoneVertData(boneIndex);
                    if (std::ranges::contains(validationDescendants, bone)) {
                        ++pointerMatches;
                        weightedPointerMatches += weighted;
                    }
                    const auto boneName = std::string_view(bone->name.c_str());
					// Todo: Remove debug jank after testing more!
					// This is just debug jank, along with a lot of the other code around here.
                    if (boneName.starts_with("NPC Genitals0") && boneName.contains("[Gen0")) {
                        ++genitalNameMatches;
                        weightedGenitalNameMatches += weighted;
                    }
                    selectedBaseReferenced |= bone == base;
                }
                if (pointerMatches > 0 || genitalNameMatches > 0) {
                    logger::info("Legacy shaft validation geometry '{}' in '{}': pointer descendants={} (weighted={}), Gen0X bones={} (weighted={}), selected base={}, eligible={}",
                        a_geometry->name, GetModelPath(a_actor, a_geometry), pointerMatches, weightedPointerMatches, genitalNameMatches, weightedGenitalNameMatches,
                        selectedBaseReferenced, ShouldUseGeometry(a_geometry));
                }
            }
            const auto weights = GetShaftWeights(a_geometry, base);
            if (!weights) {
                return RE::BSVisit::BSVisitControl::kContinue;
            }
            longestWeightedChain = std::max(longestWeightedChain, weights->chainBones.size());
            if (weights->chainBones.size() < 2) {
                return RE::BSVisit::BSVisitControl::kContinue;
            }
            bool preferModelPath = false;
            if (bestGeometry && weights->depth == bestWeights.depth && weights->chainBones.size() == bestWeights.chainBones.size() && weights->coverage == bestWeights.coverage) {
                preferModelPath = !GetModelPath(a_actor, a_geometry).empty() && GetModelPath(a_actor, bestGeometry.get()).empty();
            }
            if (!bestGeometry || weights->depth > bestWeights.depth ||
                (weights->depth == bestWeights.depth && weights->chainBones.size() > bestWeights.chainBones.size()) ||
                (weights->depth == bestWeights.depth && weights->chainBones.size() == bestWeights.chainBones.size() && weights->coverage > bestWeights.coverage) || preferModelPath) {
                bestGeometry.reset(a_geometry);
                bestWeights = *weights;
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        if (!bestGeometry) {
            logger::info("Legacy shaft surface not found for actor {:X}: base='{}', geometries={}, skinned={}, longest weighted descendant chain={}", a_actor->GetFormID(),
                baseName, geometryCount, skinnedGeometryCount, longestWeightedChain);
            return;
        }

        const auto modelPath = GetModelPath(a_actor, bestGeometry.get());
        logger::info("Legacy shaft surface selected '{}' in '{}': base='{}', skinned chain bones={}", bestGeometry->name, modelPath, baseName, bestWeights.chainBones.size());
        if (BindSurface(bestGeometry.get(), modelPath, std::addressof(bestWeights.chainBones))) {
            CacheShaftSelection(modelPath, baseName, std::string_view(bestGeometry->name.c_str()));
        } else {
            logger::info("Legacy shaft surface bind failed for '{}' in '{}': base='{}'", bestGeometry->name, modelPath, baseName);
        }
    }

    bool NodeData::SchlongData::BindSurface(RE::BSGeometry* a_geometry, std::string_view a_modelPath, const std::vector<std::uint16_t>* a_knownChain)
    {
        if (nodes.empty() || !ShouldUseGeometry(a_geometry)) {
            return false;
        }
        auto* skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
        auto* skinData = skin ? skin->skinData.get() : nullptr;
        auto* skinPartition = skin ? skin->skinPartition.get() : nullptr;
        if (!skinData || !skinPartition || !skin->bones || skinPartition->numPartitions == 0 || skinPartition->vertexCount == 0) {
            return false;
        }

        auto* base = nodes.front().get();
        const auto baseName = std::string_view(base->name.c_str());
        const auto geometryName = std::string_view(a_geometry->name.c_str());
        auto cacheName = std::string("shaft:");
        cacheName.append(baseName);
        const auto assetKey = MakeAssetTopologyKey(a_modelPath, geometryName, cacheName);
        ShaftTopology topology{};
        bool cacheHit = false;
        if (!assetKey.empty()) {
            // The asset recipe skips geometry scoring and boneweight scans for later actors using the same NIF
            if (const auto cached = shaftAssetTopologyCache.find(assetKey); cached != shaftAssetTopologyCache.end()) {
                for (const auto& variant : cached->second) {
                    if (MatchesShaftTopology(variant, skin, skinData, skinPartition, base)) {
                        topology = variant;
                        cacheHit = true;
                        break;
                    }
                }
            }
        }

        if (!cacheHit) {
            std::vector<std::uint16_t> chainBones;
            if (a_knownChain) {
                chainBones = *a_knownChain;
            } else if (const auto weights = GetShaftWeights(a_geometry, base)) {
                chainBones = weights->chainBones;
            }
            if (chainBones.size() < 2) {
                return false;
            }

            const auto boneCount = std::min(skin->numMatrices, skinData->GetBoneCount());
            std::uint64_t fingerprint = 0xCBF29CE484222325ULL;
            HashValue(fingerprint, skinPartition->vertexCount);
            HashValue(fingerprint, skinPartition->numPartitions);
            HashValue(fingerprint, skinPartition->partitions[0].vertexDesc.GetSize());
            HashValue(fingerprint, static_cast<std::uint16_t>(skinPartition->partitions[0].vertexDesc.GetFlags()));
            for (const auto value : { geometryName, a_modelPath, baseName }) {
                HashValue(fingerprint, value.size());
                for (const auto c : value) {
                    HashValue(fingerprint, static_cast<std::uint8_t>(c));
                }
            }

            std::vector<ShaftCandidate> candidates;
            std::vector<std::int32_t> candidateByVertex(skinPartition->vertexCount, -1);
            for (const auto boneIndex : chainBones) {
                if (boneIndex >= boneCount) {
                    return false;
                }
                const auto* weightedVertices = skinData->GetBoneDataBoneVertData(boneIndex);
                const auto weightedVertexCount = skinData->GetBoneDataVerts(boneIndex);
                if (!weightedVertices || weightedVertexCount == 0) {
                    return false;
                }
                HashValue(fingerprint, boneIndex);
                HashValue(fingerprint, weightedVertexCount);
                for (std::uint16_t i = 0; i < weightedVertexCount; ++i) {
                    const auto& vertex = weightedVertices[i];
                    if (vertex.vert >= skinPartition->vertexCount || vertex.weight <= FLT_EPSILON) {
                        continue;
                    }
                    HashValue(fingerprint, vertex.vert);
                    HashValue(fingerprint, std::bit_cast<std::uint32_t>(vertex.weight));
                    auto& slot = candidateByVertex[vertex.vert];
                    if (slot < 0) {
                        slot = static_cast<std::int32_t>(candidates.size());
                        candidates.push_back({ vertex.vert, vertex.weight, {}, {}, {} });
                    } else {
                        candidates[slot].chainWeight += vertex.weight;
                    }
                }
            }
            if (candidates.empty()) {
                return false;
            }

            std::optional<ShaftTopology> cachedTopology;
            {
                // Pathless meshes can still reuse an identical structure discovered earlier in this process
                if (const auto cached = shaftFingerprintTopologyCache.find(fingerprint); cached != shaftFingerprintTopologyCache.end()) {
                    cachedTopology = cached->second;
                }
            }
            if (cachedTopology && MatchesShaftTopology(*cachedTopology, skin, skinData, skinPartition, base)) {
                topology = std::move(*cachedTopology);
                cacheHit = true;
            }

            if (!cacheHit) {
                if (!ReadShaftCandidateLocalPositions(a_geometry, skinPartition, candidates)) {
                    return false;
                }
                for (std::uint32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
                    const auto* weightedVertices = skinData->GetBoneDataBoneVertData(boneIndex);
                    if (!weightedVertices) {
                        continue;
                    }
                    for (std::uint16_t i = 0; i < skinData->GetBoneDataVerts(boneIndex); ++i) {
                        const auto vertex = weightedVertices[i].vert;
                        if (vertex < candidateByVertex.size() && candidateByVertex[vertex] >= 0) {
                            candidates[candidateByVertex[vertex]].influences.push_back({ static_cast<std::uint16_t>(boneIndex), weightedVertices[i].weight });
                        }
                    }
                }

                std::vector<RE::NiTransform> transforms(boneCount);
                std::vector<bool> validTransforms(boneCount, false);
                for (std::uint32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
                    if (skin->bones[boneIndex]) {
                        transforms[boneIndex] = skin->bones[boneIndex]->world * skinData->GetBoneDataSkinToBone(boneIndex);
                        validTransforms[boneIndex] = true;
                    }
                }
                for (auto& candidate : candidates) {
                    float totalWeight = 0.0f;
                    for (const auto& influence : candidate.influences) {
                        if (validTransforms[influence.skinIndex]) {
                            candidate.world += (transforms[influence.skinIndex] * candidate.local) * influence.weight;
                            totalWeight += influence.weight;
                        }
                    }
                    if (totalWeight <= FLT_EPSILON) {
                        return false;
                    }
                    candidate.world /= totalWeight;
                }

                topology.vertexCount = skinPartition->vertexCount;
                topology.boneCount = boneCount;
                topology.stride = skinPartition->partitions[0].vertexDesc.GetSize();
                topology.vertexFlags = static_cast<std::uint16_t>(skinPartition->partitions[0].vertexDesc.GetFlags());
                topology.chainBones = chainBones;
                topology.fingerprint = fingerprint;
                topology.chainVertexCounts.reserve(chainBones.size());
                for (const auto boneIndex : chainBones) {
                    topology.chainVertexCounts.push_back(skinData->GetBoneDataVerts(boneIndex));
                }

                // Skip a multi-bone chain's root because it commonly sits inside the pelvis or also weights the scrotum
                const std::size_t firstChainIndex = chainBones.size() > 2 ? 1 : 0;
                const auto sectionCount = std::min(SHAFT_SECTION_COUNT, chainBones.size() - firstChainIndex);
                topology.rings.reserve(sectionCount);
                for (std::size_t section = 0; section < sectionCount; ++section) {
                    const auto chainIndex = firstChainIndex + section * (chainBones.size() - firstChainIndex - 1) / (sectionCount - 1);
                    const auto previous = chainIndex == 0 ? chainIndex : chainIndex - 1;
                    const auto next = chainIndex + 1 < chainBones.size() ? chainIndex + 1 : chainIndex;
                    const auto center = skin->bones[chainBones[chainIndex]]->world.translate;
                    const auto tangent = skin->bones[chainBones[next]]->world.translate - skin->bones[chainBones[previous]]->world.translate;
                    topology.rings.push_back({ SelectShaftRingSamples(candidates, center, tangent) });
                }
                const auto last = chainBones.size() - 1;
                const auto tipCenter = skin->bones[chainBones[last]]->world.translate;
                const auto tipTangent = tipCenter - skin->bones[chainBones[last - 1]]->world.translate;
                topology.tip = SelectShaftTipSamples(candidates, tipCenter, tipTangent);

                if (!MatchesShaftTopology(topology, skin, skinData, skinPartition, base)) {
                    return false;
                }
                shaftFingerprintTopologyCache.try_emplace(fingerprint, topology);
            }

            if (!assetKey.empty()) {
                auto& variants = shaftAssetTopologyCache[assetKey];
                if (std::ranges::none_of(variants, [&](const ShaftTopology& a_variant) { return a_variant.fingerprint == topology.fingerprint; })) {
                    variants.push_back(topology);
                }
            }
        }

        Surface result;
        result.geometry.reset(a_geometry);
        result.skinInstance = skin;
        const auto makeSample = [&](const CachedSample& a_cachedSample) {
            Sample sample{ a_cachedSample.vertex, {}, {} };
            sample.influences.reserve(a_cachedSample.influences.size());
            for (const auto& influence : a_cachedSample.influences) {
                auto bone = std::ranges::find(result.bones, influence.skinIndex, &Bone::skinIndex);
                std::uint16_t slot;
                if (bone == result.bones.end()) {
                    slot = static_cast<std::uint16_t>(result.bones.size());
                    result.bones.push_back({ influence.skinIndex, {} });
                } else {
                    slot = static_cast<std::uint16_t>(std::distance(result.bones.begin(), bone));
                }
                sample.influences.push_back({ slot, influence.weight });
            }
            return sample;
        };
        result.rings.reserve(topology.rings.size());
        for (const auto& cachedRing : topology.rings) {
            Ring ring;
            ring.samples.reserve(cachedRing.samples.size());
            for (const auto& cachedSample : cachedRing.samples) {
                ring.samples.push_back(makeSample(cachedSample));
            }
            result.rings.push_back(std::move(ring));
        }
        result.tip.samples.reserve(topology.tip.size());
        for (const auto& cachedSample : topology.tip) {
            result.tip.samples.push_back(makeSample(cachedSample));
        }

        const auto setLocalPositions = [&](const auto& a_getPosition) {
            for (auto& ring : result.rings) {
                for (auto& sample : ring.samples) {
                    sample.local = a_getPosition(sample.vertex);
                }
            }
            for (auto& sample : result.tip.samples) {
                sample.local = a_getPosition(sample.vertex);
            }
        };
        // Dynamic shapes are usually stuff like the head, etc. Stuff that has deltas applied dynamically (Epxressions for example).
        if (auto* dynamicShape = netimmerse_cast<RE::BSDynamicTriShape*>(a_geometry); dynamicShape && dynamicShape->GetDynamicTrishapeRuntimeData().dynamicData) {
            auto& dynamicData = dynamicShape->GetDynamicTrishapeRuntimeData();
            RE::BSSpinLockGuard lock(dynamicData.lock);
            const auto* vertices = static_cast<const DirectX::XMVECTOR*>(dynamicData.dynamicData);
            setLocalPositions([&](std::uint16_t a_vertex) {
                DirectX::XMFLOAT3 position;
                DirectX::XMStoreFloat3(&position, vertices[a_vertex]);
                return RE::NiPoint3{ position.x, position.y, position.z };
            });
        } else {
            auto& partition = skinPartition->partitions[0];
            if (!partition.vertexDesc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX) || !partition.buffData || !partition.buffData->rawVertexData) {
                return false;
            }
            const auto stride = partition.vertexDesc.GetSize();
            const auto* vertexBuffer = reinterpret_cast<const std::uint8_t*>(partition.buffData->rawVertexData);
            setLocalPositions([&](std::uint16_t a_vertex) {
                const auto* vertex = vertexBuffer + a_vertex * stride;
                return RE::NiPoint3{
                    *reinterpret_cast<const float*>(vertex),
                    *reinterpret_cast<const float*>(vertex + 4),
                    *reinterpret_cast<const float*>(vertex + 8)
                };
            });
        }

        std::vector<RE::NiPointer<RE::NiNode>> skinnedNodes{ nodes.front() };
        for (const auto boneIndex : topology.chainBones) {
            auto* node = skin->bones[boneIndex] ? skin->bones[boneIndex]->AsNode() : nullptr;
            if (node && node != skinnedNodes.back().get()) {
                skinnedNodes.emplace_back(node);
            }
        }
        if (skinnedNodes.size() > 1) {
            nodes = std::move(skinnedNodes);
        }
        surface = std::move(result);
        logger::info("Legacy shaft surface topology {} for '{}' in '{}' ({:016X}, {} rings)", cacheHit ? "cache hit" : "cached", a_geometry->name, a_modelPath,
            topology.fingerprint, topology.rings.size());
        return true;
    }

    void NodeData::SchlongData::UpdateCollisionShape()
    {
        if (surface && (!surface->geometry || !surface->geometry->parent || surface->geometry->GetGeometryRuntimeData().skinInstance.get() != surface->skinInstance)) {
            // Re-arm discovery when an equipped schlong is replaced or removed during a scene.
            surface.reset();
            collisionShape.reset();
            equipmentSignature = GetBipedSignature(actor);
            stableEquipmentFrames = 0;
            surfaceSearchPending = true;
        }

        if (!surface) {
            const auto currentSignature = GetBipedSignature(actor);
            if (currentSignature != equipmentSignature) {
                equipmentSignature = currentSignature;
                stableEquipmentFrames = 0;
                surfaceSearchPending = true;
            }

            // SOS can equip its mesh after scene setup. Wait for the biped state to settle, then scan exactly once.
            if (surfaceSearchPending && ++stableEquipmentFrames >= SHAFT_EQUIPMENT_STABLE_FRAMES) {
                surfaceSearchPending = false;
                stableEquipmentFrames = 0;
                const auto start = std::chrono::high_resolution_clock::now();
                FindSurface(actor);
                const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start);
                logger::info("Legacy shaft delayed surface initialization: {:.2f}ms ({})", elapsed.count(), surface ? "bound" : "not found");
            }
            if (!surface) {
                collisionShape.reset();
                return;
            }
        }
        auto* skinData = surface->skinInstance->skinData.get();
        if (!skinData || !surface->skinInstance->bones) {
            collisionShape.reset();
            return;
        }

        if (auto* dynamicShape = netimmerse_cast<RE::BSDynamicTriShape*>(surface->geometry.get())) {
            auto& dynamicData = dynamicShape->GetDynamicTrishapeRuntimeData();
            if (dynamicData.dynamicData) {
                RE::BSSpinLockGuard lock(dynamicData.lock);
                const auto* vertices = static_cast<const DirectX::XMVECTOR*>(dynamicData.dynamicData);
                const auto updateSamples = [&](Ring& a_ring) {
                    for (auto& sample : a_ring.samples) {
                        DirectX::XMFLOAT3 position;
                        DirectX::XMStoreFloat3(&position, vertices[sample.vertex]);
                        sample.local = { position.x, position.y, position.z };
                    }
                };
                for (auto& ring : surface->rings) {
                    updateSamples(ring);
                }
                updateSamples(surface->tip);
            }
        }

        for (auto& bone : surface->bones) {
            if (bone.skinIndex >= surface->skinInstance->numMatrices || !surface->skinInstance->bones[bone.skinIndex]) {
                collisionShape.reset();
                return;
            }
            bone.transform = surface->skinInstance->bones[bone.skinIndex]->world * skinData->GetBoneDataSkinToBone(bone.skinIndex);
        }
        const auto skinSamples = [&](Ring& a_ring) {
            a_ring.worldPositions.resize(a_ring.samples.size());
            for (std::size_t i = 0; i < a_ring.samples.size(); ++i) {
                const auto& sample = a_ring.samples[i];
                RE::NiPoint3 position{};
                float totalWeight = 0.0f;
                for (const auto& influence : sample.influences) {
                    position += (surface->bones[influence.bone].transform * sample.local) * influence.weight;
                    totalWeight += influence.weight;
                }
                if (totalWeight <= FLT_EPSILON) {
                    return false;
                }
                a_ring.worldPositions[i] = position / totalWeight;
            }
            return true;
        };

        if (!collisionShape) {
            collisionShape.emplace();
        }
        auto& shape = *collisionShape;
        shape.sections.clear();
        shape.tip = {};
        shape.skinned = true;
        shape.sections.reserve(surface->rings.size());
        for (auto& ring : surface->rings) {
            if (!skinSamples(ring) || ring.worldPositions.empty()) {
                collisionShape.reset();
                return;
            }
            RE::NiPoint3 center{};
            for (const auto& point : ring.worldPositions) {
                center += point;
            }
            center /= static_cast<float>(ring.worldPositions.size());
            shape.sections.push_back({ center, 0.0f });
        }
        if (shape.sections.size() < 2) {
            collisionShape.reset();
            return;
        }

        // Radius is measured perpendicular to the local centerline, so bent poses do not inflate it.
        for (std::size_t i = 0; i < shape.sections.size(); ++i) {
            const auto previous = i == 0 ? i : i - 1;
            const auto next = i + 1 < shape.sections.size() ? i + 1 : i;
            auto tangent = shape.sections[next].center - shape.sections[previous].center;
            if (tangent.SqrLength() <= FLT_EPSILON) {
                collisionShape.reset();
                return;
            }
            tangent.Unitize();
            for (const auto& point : surface->rings[i].worldPositions) {
                const auto offset = point - shape.sections[i].center;
                shape.sections[i].radius += (offset - tangent * offset.Dot(tangent)).Length();
            }
            shape.sections[i].radius /= static_cast<float>(surface->rings[i].worldPositions.size());
        }

        if (!skinSamples(surface->tip) || surface->tip.worldPositions.empty()) {
            collisionShape.reset();
            return;
        }
        for (const auto& point : surface->tip.worldPositions) {
            shape.tip += point;
        }
        shape.tip /= static_cast<float>(surface->tip.worldPositions.size());
    }

    NiMath::Segment NodeData::SchlongData::GetReferenceSegment() const
    {
        if (collisionShape && collisionShape->sections.size() >= 2) {
            return { collisionShape->sections.front().center, collisionShape->tip };
        }
        switch (nodes.size()) {
        case 0:
            assert(false);
            throw std::invalid_argument("Schlong Data without any Nodes?");
        case 1:
            {
                auto translate = rot * nodes.front()->world.rotate;
                auto vforward = translate.GetVectorY() * MIN_SCHLONG_LEN;
                vforward.Unitize();
                auto s1 = nodes.front()->world.translate;
                auto s2 = (vforward * MIN_SCHLONG_LEN) + s1;
                return NiMath::Segment(s1, s2);
            }
        default:
            {
                std::vector<Eigen::Vector3f> argV{};
                argV.reserve(nodes.size());
                for (auto&& node : nodes) {
                    if (!node)
                        continue;
                    auto argT = NiMath::ToEigen(node->world.translate);
                    argV.push_back(argT);
                }
                return NiMath::LeastSquares(argV, MIN_SCHLONG_LEN);
            }
        }
    }

    RE::NiPointer<RE::NiNode> NodeData::SchlongData::GetBaseReferenceNode() const
    {
        switch (nodes.size()) {
        case 0:
            assert(false);
            throw std::invalid_argument("Schlong Data without any Nodes?");
        default:
            return nodes.front();
        }
    }
}
