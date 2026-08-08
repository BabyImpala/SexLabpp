#include "InterType.h"
#include <unordered_map>
#include <logger.h>

namespace Thread::Interactions
{
    static std::array<std::string_view, SUPPORTED_INTER_COUNT> s_typeNames;
    static std::array<Registry::Tag, SUPPORTED_INTER_COUNT> s_correspondingTags;
    static std::unordered_map<CType, InterType> s_ctypeToInterType;
    static std::unordered_map<int, InterType> s_niTypeToInterType;

    void Initialize()
    {
        // Initialize type names (obfuscated-friendly short names)
        s_typeNames.fill("unknown");
        
        s_typeNames[static_cast<size_t>(InterType::pStimulation)] = "pStim";
        s_typeNames[static_cast<size_t>(InterType::aAnimObjFace)] = "aAOF";
        s_typeNames[static_cast<size_t>(InterType::pAnimObjFace)] = "pAOF";
        s_typeNames[static_cast<size_t>(InterType::pSuckingToes)] = "pST";
        s_typeNames[static_cast<size_t>(InterType::pGrinding)] = "pGrind";
        s_typeNames[static_cast<size_t>(InterType::pSkullfuck)] = "pSF";
        s_typeNames[static_cast<size_t>(InterType::aHandJob)] = "aHJ";
        s_typeNames[static_cast<size_t>(InterType::aFootJob)] = "aFJ";
        s_typeNames[static_cast<size_t>(InterType::aBoobJob)] = "aBJ";
        s_typeNames[static_cast<size_t>(InterType::bKissing)] = "bKiss";
        s_typeNames[static_cast<size_t>(InterType::aSuckingToes)] = "aST";
        s_typeNames[static_cast<size_t>(InterType::pFacial)] = "pFac";
        s_typeNames[static_cast<size_t>(InterType::aOral)] = "aOrl";
        s_typeNames[static_cast<size_t>(InterType::aLickingShaft)] = "aLS";
        s_typeNames[static_cast<size_t>(InterType::aDeepthroat)] = "aDT";
        s_typeNames[static_cast<size_t>(InterType::pVaginal)] = "pVag";
        s_typeNames[static_cast<size_t>(InterType::pAnal)] = "pAnl";
        s_typeNames[static_cast<size_t>(InterType::aFacial)] = "aFac";
        s_typeNames[static_cast<size_t>(InterType::aGrinding)] = "aGrind";
        s_typeNames[static_cast<size_t>(InterType::pHandJob)] = "pHJ";
        s_typeNames[static_cast<size_t>(InterType::pFootJob)] = "pFJ";
        s_typeNames[static_cast<size_t>(InterType::pBoobJob)] = "pBJ";
        s_typeNames[static_cast<size_t>(InterType::pLickingShaft)] = "pLS";
        s_typeNames[static_cast<size_t>(InterType::pOral)] = "pOrl";
        s_typeNames[static_cast<size_t>(InterType::pDeepthroat)] = "pDT";
        s_typeNames[static_cast<size_t>(InterType::aSkullfuck)] = "aSF";
        s_typeNames[static_cast<size_t>(InterType::aVaginal)] = "aVag";
        s_typeNames[static_cast<size_t>(InterType::aAnal)] = "aAnl";

        // Initialize corresponding tags for fallback detection
        s_correspondingTags.fill(Registry::Tag::None);
        
        s_correspondingTags[static_cast<size_t>(InterType::pVaginal)] = Registry::Tag::Vaginal;
        s_correspondingTags[static_cast<size_t>(InterType::pAnal)] = Registry::Tag::Anal;
        s_correspondingTags[static_cast<size_t>(InterType::pOral)] = Registry::Tag::Oral;
        s_correspondingTags[static_cast<size_t>(InterType::aVaginal)] = Registry::Tag::Vaginal;
        s_correspondingTags[static_cast<size_t>(InterType::aAnal)] = Registry::Tag::Anal;
        s_correspondingTags[static_cast<size_t>(InterType::aOral)] = Registry::Tag::Oral;
        s_correspondingTags[static_cast<size_t>(InterType::pGrinding)] = Registry::Tag::Grinding;
        s_correspondingTags[static_cast<size_t>(InterType::aGrinding)] = Registry::Tag::Grinding;
        s_correspondingTags[static_cast<size_t>(InterType::bKissing)] = Registry::Tag::Kissing;
        s_correspondingTags[static_cast<size_t>(InterType::aDeepthroat)] = Registry::Tag::Deepthroat;
        s_correspondingTags[static_cast<size_t>(InterType::pDeepthroat)] = Registry::Tag::Deepthroat;

        // Initialize CType to InterType mapping
        s_ctypeToInterType[CType::CTYPE_Vaginal] = InterType::pVaginal;
        s_ctypeToInterType[CType::CTYPE_Anal] = InterType::pAnal;
        s_ctypeToInterType[CType::CTYPE_Oral] = InterType::pOral;
        s_ctypeToInterType[CType::CTYPE_Grinding] = InterType::pGrinding;
        s_ctypeToInterType[CType::CTYPE_Deepthroat] = InterType::pDeepthroat;
        s_ctypeToInterType[CType::CTYPE_Skullfuck] = InterType::pSkullfuck;
        s_ctypeToInterType[CType::CTYPE_LickingShaft] = InterType::pLickingShaft;
        s_ctypeToInterType[CType::CTYPE_FootJob] = InterType::pFootJob;
        s_ctypeToInterType[CType::CTYPE_HandJob] = InterType::pHandJob;
        s_ctypeToInterType[CType::CTYPE_Kissing] = InterType::bKissing;
        s_ctypeToInterType[CType::CTYPE_Facial] = InterType::pFacial;
        s_ctypeToInterType[CType::CTYPE_AnimObjFace] = InterType::pAnimObjFace;
        s_ctypeToInterType[CType::CTYPE_SuckingToes] = InterType::pSuckingToes;

        // Initialize NiType to InterType mapping
        // NiType values from NiType.def: Vaginal=0, Anal=1, Oral=2, Grinding=3, Deepthroat=4, 
        // Skullfuck=5, LickingShaft=6, Kissing=7
        s_niTypeToInterType[0] = InterType::pVaginal;    // Vaginal
        s_niTypeToInterType[1] = InterType::pAnal;       // Anal
        s_niTypeToInterType[2] = InterType::pOral;       // Oral
        s_niTypeToInterType[3] = InterType::pGrinding;   // Grinding
        s_niTypeToInterType[4] = InterType::pDeepthroat; // Deepthroat
        s_niTypeToInterType[5] = InterType::pSkullfuck;  // Skullfuck
        s_niTypeToInterType[6] = InterType::pLickingShaft; // LickingShaft
        s_niTypeToInterType[7] = InterType::bKissing;    // Kissing

        logger::info("InterType lookup tables initialized");
    }

    std::string_view GetInterTypeName(InterType type)
    {
        const auto idx = static_cast<size_t>(type);
        if (idx >= SUPPORTED_INTER_COUNT) {
            return "invalid";
        }
        return s_typeNames[idx];
    }

    Registry::Tag GetCorrespondingTag(InterType type)
    {
        const auto idx = static_cast<size_t>(type);
        if (idx >= SUPPORTED_INTER_COUNT) {
            return Registry::Tag::None;
        }
        return s_correspondingTags[idx];
    }

    std::optional<InterType> CTypeToInterType(CType ctype)
    {
        auto it = s_ctypeToInterType.find(ctype);
        if (it != s_ctypeToInterType.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::optional<InterType> NiTypeToInterType(int niTypeValue)
    {
        auto it = s_niTypeToInterType.find(niTypeValue);
        if (it != s_niTypeToInterType.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool IsPenetrationReceiving(InterType type)
    {
        return type == InterType::pVaginal || type == InterType::pAnal || 
               type == InterType::pOral || type == InterType::pDeepthroat ||
               type == InterType::pSkullfuck || type == InterType::pLickingShaft;
    }

    bool IsPenetrationGiving(InterType type)
    {
        return type == InterType::aVaginal || type == InterType::aAnal ||
               type == InterType::aOral || type == InterType::aDeepthroat ||
               type == InterType::aSkullfuck || type == InterType::aLickingShaft;
    }

    bool IsOralActivity(InterType type)
    {
        return type == InterType::aOral || type == InterType::pOral ||
               type == InterType::aDeepthroat || type == InterType::pDeepthroat ||
               type == InterType::aLickingShaft || type == InterType::pLickingShaft ||
               type == InterType::aSuckingToes || type == InterType::pSuckingToes;
    }

}  // namespace Thread::Interactions
