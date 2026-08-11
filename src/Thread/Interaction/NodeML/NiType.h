#pragma once

namespace Thread::NiNode::NiType
{
    enum class Cluster
    {
        None = 0,
        ClusterCrotch,
        ClusterHead,
        ClusterKissing,
    };
    constexpr static inline size_t NUM_CLUSTERS = magic_enum::enum_count<Cluster>();

    enum class Type
    {
        None = 0,
#define NI_TYPE(name, cluster) name,

#include "NiType.def"

#undef NI_TYPE
    };
    constexpr static inline size_t NUM_TYPES = magic_enum::enum_count<Type>();

    inline Cluster GetClusterForType(Type type)
    {
#define NI_TYPE(name, cluster)   \
    if (type == Type::name) {    \
        return Cluster::cluster; \
    }
#include "NiType.def"
#undef NI_TYPE
        return Cluster::None;
    }

    inline std::vector<Type> GetTypesForCluster(Cluster a_cluster)
    {
        std::vector<Type> types;
#define NI_TYPE(name, cluster)           \
    if (a_cluster == Cluster::cluster) { \
        types.push_back(Type::name);     \
    }
#include "NiType.def"
#undef NI_TYPE
        return types;
    }
}  // namespace NiType