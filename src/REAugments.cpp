#include "REAugments.h"

#include "BipedHooks.h"

namespace AP::REAug {

    bool ApplyArmorAddon(RE::TESObjectARMO* a_armor, RE::TESRace* a_race,
                         ActorWeightModel* a_model, bool a_isFemale) {
        using func_t = bool (*)(RE::TESObjectARMO*, RE::TESRace*, ActorWeightModel*, bool);
        static REL::Relocation<func_t> func{ REL::RelocationID(17392, 17792) };
        return func(a_armor, a_race, a_model, a_isFemale);
    }

    void SetEquipFlag(RE::AIProcess* a_process) {
        // Flag 1<<0 ("equipment changed") - matches SOS's Flag::kUnk01.
        using func_t = void (*)(RE::AIProcess*, std::uint8_t);
        static REL::Relocation<func_t> func{ REL::RelocationID(38867, 39907) };
        func(a_process, 1 << 0);
    }

    void UpdateEquipment(RE::AIProcess* a_process, RE::Actor* a_actor) {
        using func_t = void (*)(RE::AIProcess*, RE::Actor*);
        static REL::Relocation<func_t> func{ REL::RelocationID(38404, 39395) };
        func(a_process, a_actor);
    }

    void SendInventoryUpdateMessage(RE::TESObjectREFR* a_inventoryRef,
                                    const RE::TESBoundObject* a_updateObj) {
        using func_t = void (*)(RE::TESObjectREFR*, const RE::TESBoundObject*);
        static REL::Relocation<func_t> func{ REL::RelocationID(51911, 52849) };
        func(a_inventoryRef, a_updateObj);
    }

    void RefreshPlayer(bool a_sceneKick) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* proc   = player ? player->GetActorRuntimeData().currentProcess : nullptr;
        if (!proc) {
            spdlog::warn("RefreshPlayer: no AIProcess.");
            return;
        }
        // Proven on 1.5.97 (checkpoint 2): this synchronous rebuild runs even
        // while Container/Barter pause the game. The worn-pass counter guards
        // against silent regressions.
        const auto c0 = BipedHooks::PlayerWornPassCount();
        SetEquipFlag(proc);
        UpdateEquipment(proc, player);
        const auto c1 = BipedHooks::PlayerWornPassCount();
        spdlog::debug("refresh (SetEquipFlag+UpdateEquipment): wornpass ran {}x", c1 - c0);
        if (c1 == c0) {
            spdlog::warn("refresh: skinning pass did NOT run - preview will not update visually.");
        }

        if (a_sceneKick) {
            // Container/Barter pause the game; kick the player's scene graph so
            // the rebuilt biped renders immediately (0x2000 flag from IED).
            if (auto* root = player->Get3D(false)) {
                RE::NiUpdateData ctx;
                ctx.time  = 0.0f;
                ctx.flags = static_cast<RE::NiUpdateData::Flag>(0x2000);
                root->UpdateWorldBound();
                root->Update(ctx);
            }
        }
        spdlog::debug("RefreshPlayer done (kick={}).", a_sceneKick);
    }

}  // namespace AP::REAug
