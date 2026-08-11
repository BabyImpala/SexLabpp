#pragma once

#include "Thread/Thread.h"

namespace Thread
{
    class Instance;
}

namespace Thread::Interaction
{
    constexpr int32_t kInterTypeCount = 28;
    constexpr int32_t kCTypeCount = 14;

    enum InterType : int32_t
    {
        bKissing      = 0,
        bStimulation  = 1,
        aAnimObjFace  = 2,
        pAnimObjFace  = 3,
        aGrinding     = 4,
        pGrinding     = 5,
        aSuckingToes  = 6,
        pSuckingToes  = 7,
        aFootJob      = 8,
        pFootJob      = 9,
        aHandJob      = 10,
        pHandJob      = 11,
        aBoobJob      = 12,
        pBoobJob      = 13,
        aFacial       = 14,
        pFacial       = 15,
        aLickingShaft = 16,
        pLickingShaft = 17,
        aOral         = 18,
        pOral         = 19,
        aDeepthroat   = 20,
        pDeepthroat   = 21,
        aSkullfuck    = 22,
        pSkullfuck    = 23,
        aVaginal      = 24,
        pVaginal      = 25,
        aAnal         = 26,
        pAnal         = 27,
    };

    // Matches enum in egacyNiPosition.h
    enum class CType : int32_t  
    {
        None         = 0,
        Vaginal      = 1,   // Position is being penetrated by partner
        Anal         = 2,   // Position is being penetrated by partner
        Oral         = 3,   // Position is licking/sucking partner
        Grinding     = 4,   // Position is being grinded against by partner (crotch area)
        Deepthroat   = 5,   // Implies Oral, partner's penis close to/at maximum depth
        Skullfuck    = 6,   // Positions head penetrated in an unexpected way by partner (Usually gore)
        LickingShaft = 7,   // Position licking partners shaft
        FootJob      = 8,   // Position pleasuring partner using at least one foot
        HandJob      = 9,   // Position pleasuring partner using at least one hand
        Kissing      = 10,  // Position kissing partner
        Facial       = 11,  // Positions face in front of partner penis
        AnimObjFace  = 12,  // Position mouth close to partner anim object node
        SuckingToes  = 13,  // Position mouth close to partner toes
    };

    struct InterTypeEntry
    {
        std::string_view name;
        int32_t complement;     // partner will have this interType
        CType ctype;
        bool swapped;           // swapped => actor is idxB, partner is idxA (RevType)
        bool supported;         // mainly by legacy NiNode
    };

    inline constexpr std::array<InterTypeEntry, kInterTypeCount> kInterTypeTable = {{
        { "bKissing",      bKissing,      CType::Kissing,      false,   true  },  //  0
        { "bStimulation",  bStimulation,  CType::None,         false,   false },  //  1
        { "aAnimObjFace",  pAnimObjFace,  CType::AnimObjFace,  true,    true  },  //  2
        { "pAnimObjFace",  aAnimObjFace,  CType::AnimObjFace,  false,   true  },  //  3
        { "aGrinding",     pGrinding,     CType::Grinding,     true,    true  },  //  4
        { "pGrinding",     aGrinding,     CType::Grinding,     false,   true  },  //  5
        { "aSuckingToes",  pSuckingToes,  CType::SuckingToes,  false,   true  },  //  6
        { "pSuckingToes",  aSuckingToes,  CType::SuckingToes,  true,    true  },  //  7
        { "aFootJob",      pFootJob,      CType::FootJob,      false,   true  },  //  8
        { "pFootJob",      aFootJob,      CType::FootJob,      true,    true  },  //  9
        { "aHandJob",      pHandJob,      CType::HandJob,      false,   true  },  // 10
        { "pHandJob",      aHandJob,      CType::HandJob,      true,    true  },  // 11
        { "aBoobJob",      pBoobJob,      CType::None,         false,   false },  // 12
        { "pBoobJob",      aBoobJob,      CType::None,         false,   false },  // 13
        { "aFacial",       pFacial,       CType::Facial,       true,    true  },  // 14
        { "pFacial",       aFacial,       CType::Facial,       false,   true  },  // 15
        { "aLickingShaft", pLickingShaft, CType::LickingShaft, false,   true  },  // 16
        { "pLickingShaft", aLickingShaft, CType::LickingShaft, true,    true  },  // 17
        { "aOral",         pOral,         CType::Oral,         false,   true  },  // 18
        { "pOral",         aOral,         CType::Oral,         true,    true  },  // 19
        { "aDeepthroat",   pDeepthroat,   CType::Deepthroat,   false,   true  },  // 20
        { "pDeepthroat",   aDeepthroat,   CType::Deepthroat,   true,    true  },  // 21
        { "aSkullfuck",    pSkullfuck,    CType::Skullfuck,    true,    true  },  // 22
        { "pSkullfuck",    aSkullfuck,    CType::Skullfuck,    false,   true  },  // 23
        { "aVaginal",      pVaginal,      CType::Vaginal,      true,    true  },  // 24
        { "pVaginal",      aVaginal,      CType::Vaginal,      false,   true  },  // 25
        { "aAnal",         pAnal,         CType::Anal,         true,    true  },  // 26
        { "pAnal",         aAnal,         CType::Anal,         false,   true  },  // 27
    }};

    //  Built once from kInterTypeTable at static init
    using TempInterMap = std::array<std::vector<int32_t>, kCTypeCount>;

    inline std::pair<TempInterMap, TempInterMap> BuildTempInterMap()
    {
        TempInterMap a_actorAsIdxA{}, a_partnerAsIdxA{};
        for (int32_t it = 0; it < kInterTypeCount; ++it) {
            const auto& e = kInterTypeTable[it];
            if (!e.supported)
                continue;
            const auto ct = static_cast<int32_t>(e.ctype);
            (e.swapped ? a_partnerAsIdxA : a_actorAsIdxA)[ct].push_back(it);
        }
        return { a_actorAsIdxA, a_partnerAsIdxA };
    }

    inline const std::pair<TempInterMap, TempInterMap>& GetTempInterMaps()
    {
        static const auto maps = BuildTempInterMap();
        return maps;
    }

    inline const TempInterMap& InterTypesWithActorAsIdxA()
    {
        return GetTempInterMaps().first;
    }

    inline const TempInterMap& InterTypesWithPartnerAsIdxA()
    {
        return GetTempInterMaps().second;
    }

    inline const std::unordered_map<std::string_view, int32_t>& InterTypeByName()
    {
        static const auto map = [] {
            std::unordered_map<std::string_view, int32_t> m;
            m.reserve(kInterTypeCount);
            for (int32_t i = 0; i < kInterTypeCount; ++i)
                m.emplace(kInterTypeTable[i].name, i);
            return m;
        }();
        return map;
    }

    //  C++ API (nullptr = any partner)
    std::vector<bool> GetInteractionFlagsImpl(Thread::Instance* instance, RE::Actor* a_actor, RE::Actor* a_partner);
    std::vector<int32_t> GetActiveInterTypesImpl(Thread::Instance* instance, RE::Actor* a_actor, RE::Actor* a_partner);

    bool HasActiveInteractionImpl(Thread::Instance* instance, RE::Actor* a_actor, RE::Actor* a_partner, int32_t interType);
    bool HasActiveInteractionAllImpl(Thread::Instance* instance, RE::Actor* a_actor, const std::vector<int32_t>& interTypes);
    bool HasActiveInteractionAnyImpl(Thread::Instance* instance, RE::Actor* a_actor, const std::vector<int32_t>& interTypes);

    std::vector<RE::Actor*> GetPartnersByInteractionTypeImpl(Thread::Instance* instance, RE::Actor* a_actor, int32_t interType);
    RE::Actor* GetPartnerByInteractionTypeImpl(Thread::Instance* instance, RE::Actor* a_actor, int32_t interType);

    float GetInteractionVelocityImpl(Thread::Instance* instance, RE::Actor* a_actor, RE::Actor* a_partner, int32_t interType);
    
    std::vector<RE::BSFixedString> GetInteractionStringArrayImpl(Thread::Instance* instance, RE::Actor* a_actor);
    std::string GetInteractionStringImpl(Thread::Instance* instance, RE::Actor* a_actor);

}  // namespace Thread::Interaction
