#pragma once
#include "PCH.h"

namespace AP::REAug {

    // Opaque engine type: the biped-model holder ApplyArmorAddon populates
    // (SKSE lineage calls it ActorWeightModel). We only pass pointers through.
    struct ActorWeightModel;

    // TESObjectARMO::ApplyArmorAddon - attaches the armor's ArmorAddon 3D for
    // race/sex/weight into the given weight model. SE REL::ID 17392.
    bool ApplyArmorAddon(RE::TESObjectARMO* a_armor, RE::TESRace* a_race,
                         ActorWeightModel* a_model, bool a_isFemale);

    // AIProcess "equipment changed" flag + rebuild. SE REL::ID 38867 / 38404.
    // Together they re-run the (hooked) skinning pass without any equip.
    void SetEquipFlag(RE::AIProcess* a_process);
    void UpdateEquipment(RE::AIProcess* a_process, RE::Actor* a_actor);

    // Full player visual refresh (flag + update + optional scene kick so the
    // rebuild renders inside paused Container/Barter menus - IED's recipe).
    void RefreshPlayer(bool a_sceneKick);

    // Engine broadcast: "this ref's inventory changed" - open menus rebuild
    // their lists with an engine-allocated payload. SE REL::ID 51911 (newer
    // CommonLibSSE-NG exposes this as RE::SendUIMessage; ours predates it).
    void SendInventoryUpdateMessage(RE::TESObjectREFR* a_inventoryRef,
                                    const RE::TESBoundObject* a_updateObj);

}  // namespace AP::REAug
