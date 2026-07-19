#pragma once

namespace Papyrus::SexLabUtil
{
    bool HasKeywordSub(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::TESForm* a_form, std::string_view a_substring);
    RE::BSFixedString RemoveSubString(RE::StaticFunctionTag*, std::string a_str, std::string a_substring);
    void PrintConsole(RE::StaticFunctionTag*, std::string a_str);

    int IntMinMaxIndex(RE::StaticFunctionTag*, std::vector<int> arr, bool findHighestValue);
    int IntMinMaxValue(RE::StaticFunctionTag*, std::vector<int> arr, bool findHighestValue);
    int FloatMinMaxIndex(RE::StaticFunctionTag*, std::vector<float> arr, bool findHighestValue);
    float FloatMinMaxValue(RE::StaticFunctionTag*, std::vector<float> arr, bool findHighestValue);
    
    std::vector<RE::Actor*> MakeActorArray(RE::StaticFunctionTag*, RE::Actor* a1, RE::Actor* a2, RE::Actor* a3, RE::Actor* a4, RE::Actor* a5);
    float GetCurrentGameRealTime(RE::StaticFunctionTag*);
    std::string GetTranslation(RE::StaticFunctionTag*, std::string a_str);
    bool IsGodModeEnabled(RE::StaticFunctionTag*);
    
    std::vector<RE::BSFixedString> ShuffleStringArray(RE::StaticFunctionTag*, std::vector<RE::BSFixedString> arr, RE::BSFixedString asFirst, int aiMaxLen);
    void HideElementsGameHUD(RE::StaticFunctionTag*, bool a_hide);

    static constexpr std::array<std::string_view, 21> HUD_ELEMENTS = {
        "HUD Menu", "LootMenu", "TrueHUD", "BTPS Menu", "BTPS Ovelay Menu", "oxygenMeter2", "CastingBar", "MiniMapMenu",
        "Floating Damage V2", "Durability Menu", "KNNWidgetMeter", "KNNWidgetMeterOp", "lvlWidget", "goldWidget",
        "gametimeWidget", "shoutWidget", "resistWidget", "playtimeWidget", "weightWidget", "equipWidget_STB", "STBActiveEffects"
    };

    inline bool Register(VM* a_vm)
    {
        REGISTERFUNC(HasKeywordSub, "SexLabUtil", true);
        REGISTERFUNC(RemoveSubString, "SexLabUtil", true);
        REGISTERFUNC(PrintConsole, "SexLabUtil", true);
        REGISTERFUNC(IntMinMaxIndex, "SexLabUtil", true);
        REGISTERFUNC(IntMinMaxValue, "SexLabUtil", true);
        REGISTERFUNC(FloatMinMaxIndex, "SexLabUtil", true);
        REGISTERFUNC(FloatMinMaxValue, "SexLabUtil", true);
        REGISTERFUNC(MakeActorArray, "SexLabUtil", true);
        REGISTERFUNC(GetCurrentGameRealTime, "SexLabUtil", true);
        REGISTERFUNC(GetTranslation, "SexLabUtil", true);
        REGISTERFUNC(IsGodModeEnabled, "SexLabUtil", true);
        REGISTERFUNC(ShuffleStringArray, "SexLabUtil", true);
        REGISTERFUNC(HideElementsGameHUD, "SexLabUtil", true);

        return true;
    };

} // namespace Papyrus::SexLabUtil
