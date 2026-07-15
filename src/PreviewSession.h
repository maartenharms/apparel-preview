#pragma once
#include "PCH.h"

#include "PreviewSet.h"
#include "REAugments.h"

namespace AP {

    // All preview state + the refresh that makes it visible. Thread rules:
    // Toggle/Clear marshal to the game thread via the SKSE task interface;
    // the Should*/Visit* readers are called from inside the engine's skinning
    // pass (game thread) and take the same lock.
    class PreviewSession {
    public:
        static PreviewSession& GetSingleton();

        void Toggle(RE::TESObjectARMO* a_armor, std::uint32_t a_variantKey);  // add/remove/swap + refresh
        void Clear();                                // revert to real worn look + refresh
        void OnPlayerAcquired(RE::FormID a_formID);  // container-changed path
        void OnPlayerEquipped(RE::FormID a_formID);  // equip-event path (inventory menu)

        // Row discriminator for an inventory entry. Several list rows can
        // share one base ARMO - they differ only in extra data (enchantment,
        // temper, ownership, custom name). Folds those into one key; 0 means
        // a plain row. Rows that are genuinely identical share a key, which
        // is correct: they are the same item.
        [[nodiscard]] static std::uint32_t VariantKey(RE::InventoryEntryData* a_entry);

        // ---- read side, used by BipedHooks (game thread, during rebuild) ----
        [[nodiscard]] bool IsActive() const;
        // Row-exact (form + variant) - list markers, the DIII condition and
        // the hover-model clear use this so only the toggled row counts.
        [[nodiscard]] bool IsPreviewedEntry(RE::FormID a_formID, std::uint32_t a_variantKey) const;
        [[nodiscard]] std::uint32_t PreviewMask() const;
        void VisitPreviewArmors(const std::function<void(RE::TESObjectARMO*)>& a_fn) const;

    private:
        void ToggleImpl(RE::FormID a_formID, std::uint32_t a_variantKey, std::uint32_t a_mask,
                        const std::string& a_name);
        void RemoveFormAndRefresh(RE::FormID a_formID, const char* a_reason);

        mutable std::mutex lock_;
        PreviewSet         set_;
    };

}  // namespace AP
