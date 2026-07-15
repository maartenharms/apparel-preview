#include "PreviewSession.h"

#include "HoverTracker.h"
#include "ListMarker.h"
#include "Settings.h"

namespace AP {

    PreviewSession& PreviewSession::GetSingleton() {
        static PreviewSession instance;
        return instance;
    }

    std::uint32_t PreviewSession::VariantKey(RE::InventoryEntryData* a_entry) {
        // The menu builds one InventoryEntryData per displayed row (that is
        // why rows of one base form show different names), so the row's own
        // extra data lives in the FIRST list here. The base form's own
        // enchantment is deliberately NOT folded in: every row of that base
        // shares it, so it cannot distinguish rows.
        if (!a_entry || !a_entry->extraLists) {
            return 0;
        }
        const RE::ExtraDataList* xList = nullptr;
        for (const auto* candidate : *a_entry->extraLists) {
            if (candidate) {
                xList = candidate;
                break;
            }
        }
        if (!xList) {
            return 0;
        }

        std::uint32_t enchID   = 0;
        std::uint32_t healthBits = 0;
        std::uint32_t ownerID  = 0;
        std::uint32_t nameHash = 0;
        if (const auto* x = xList->GetByType<RE::ExtraEnchantment>(); x && x->enchantment) {
            enchID = x->enchantment->GetFormID();
        }
        if (const auto* x = xList->GetByType<RE::ExtraHealth>(); x) {
            healthBits = std::bit_cast<std::uint32_t>(x->health);  // temper level
        }
        if (const auto* x = xList->GetByType<RE::ExtraOwnership>(); x && x->owner) {
            ownerID = x->owner->GetFormID();
        }
        if (const auto* x = xList->GetByType<RE::ExtraTextDisplayData>(); x) {
            if (const char* custom = x->displayName.c_str(); custom && *custom) {
                nameHash = static_cast<std::uint32_t>(
                    std::hash<std::string_view>{}(std::string_view{ custom }));
            }
        }
        if ((enchID | healthBits | ownerID | nameHash) == 0) {
            return 0;  // plain row
        }

        // FNV-1a fold. A collision would merely mark two rows - cosmetic.
        std::uint32_t key = 2166136261u;
        for (const std::uint32_t part : { enchID, healthBits, ownerID, nameHash }) {
            key = (key ^ part) * 16777619u;
        }
        return key ? key : 1u;  // never alias the "plain row" sentinel
    }

    void PreviewSession::Toggle(RE::TESObjectARMO* a_armor, std::uint32_t a_variantKey) {
        if (!a_armor) {
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* race   = player ? player->GetRace() : nullptr;
        if (!race || !a_armor->GetArmorAddon(race)) {
            // Body-mod case (UBE etc. are custom races): a piece with no
            // armature for the player's race attaches nothing - the engine
            // would render a broken or invisible preview. Decline LOUDLY:
            // a silent no-op reads as "the mod is broken".
            spdlog::info("preview declined: '{}' ({:08X}) has no armor addon for race "
                         "'{}' - does not fit this body.",
                         a_armor->GetName(), a_armor->GetFormID(),
                         race ? race->GetName() : "?");
            if (Settings::GetSingleton().notifyUnfitPreview) {
                RE::DebugNotification("Cannot preview: this piece does not fit your body.",
                                      "UIMenuCancel");
            }
            return;
        }
        using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
        const auto slotMask = static_cast<std::uint32_t>(a_armor->GetSlotMask());
        if (!Settings::GetSingleton().allowShields &&
            (slotMask & static_cast<std::uint32_t>(Slot::kShield)) != 0) {
            spdlog::debug("Toggle: '{}' is a shield and shield preview is disabled.", a_armor->GetName());
            return;
        }
        const auto        formID  = a_armor->GetFormID();
        const auto        variant = a_variantKey;
        const auto        mask    = slotMask;
        const std::string name    = a_armor->GetName();
        SKSE::GetTaskInterface()->AddTask([this, formID, variant, mask, name] {
            ToggleImpl(formID, variant, mask, name);
        });
    }

    namespace {
        // Form-level, matching the injection (which is form-level too): any
        // worn entry of this base form makes a preview of it meaningless and
        // would double-visit the armor in ExecuteVisitorOnWorn. Runs on the
        // game thread (SKSE task) with no session lock held.
        bool IsFormWornByPlayer(RE::FormID a_formID) {
            auto* player  = RE::PlayerCharacter::GetSingleton();
            auto* changes = player ? player->GetInventoryChanges() : nullptr;
            if (!changes || !changes->entryList) {
                return false;
            }
            for (const auto* entry : *changes->entryList) {
                if (entry && entry->object && entry->object->GetFormID() == a_formID &&
                    entry->IsWorn()) {
                    return true;
                }
            }
            return false;
        }

        // ---- Helmet Toggle 2 interop -----------------------------------
        // HT2 hides worn headgear through a Dynamic Armor Variants slot
        // swap (replaceBySlot 30/31/42/44 -> invisible ARMA) that also
        // swallows any geometry WE attach in those slots - a head-slot
        // preview renders as a bald character (field-proven). DAV has no
        // query-active-variant API and Papyrus does not run in paused
        // menus, so suspend-and-restore is impossible; the honest move is
        // to decline the preview and say why. HT2's own state global
        // documents itself in its scripts: 0 = headgear shown, 1 = hidden.
        constexpr auto kHT2Plugin = "Helmet Toggle 2.esp";
        constexpr RE::FormID kHelmetStateID = 0x804;  // GLOB 'HT_HelmetState'

        // Slots HT2's hidden variants replace. Bit N = biped slot 30+N.
        constexpr std::uint32_t kHT2HeadSlots =
            (1u << 0) | (1u << 1) | (1u << 12) | (1u << 14);  // 30/31/42/44

        RE::TESGlobal* HelmetStateGlobal() {
            // Resolved once on first use; ToggleImpl only ever runs on the
            // game thread (SKSE task), so the local static is uncontended.
            static RE::TESGlobal* global = []() -> RE::TESGlobal* {
                auto* dh = RE::TESDataHandler::GetSingleton();
                auto* g  = dh ? dh->LookupForm<RE::TESGlobal>(kHelmetStateID, kHT2Plugin)
                              : nullptr;
                if (g) {
                    spdlog::info("Helmet Toggle 2 detected: previews of hidden "
                                 "head slots will be declined.");
                }
                return g;
            }();
            return global;
        }

        // True when HT2 currently hides worn headgear AND the candidate
        // piece overlaps the hidden slots. The slot intersection keeps
        // unrelated headwear previewable (hidden HOOD covers 30/31; a
        // CIRCLET preview in 42 still renders).
        bool HiddenHeadgearConflict(std::uint32_t a_candidateMask) {
            auto* global = HelmetStateGlobal();
            if (!global || global->value <= 0.0f) {
                return false;
            }
            auto* player  = RE::PlayerCharacter::GetSingleton();
            auto* changes = player ? player->GetInventoryChanges() : nullptr;
            if (!changes || !changes->entryList) {
                return false;
            }
            std::uint32_t wornHead = 0;
            for (const auto* entry : *changes->entryList) {
                if (!entry || !entry->object || !entry->IsWorn()) {
                    continue;
                }
                if (const auto* armo = entry->object->As<RE::TESObjectARMO>()) {
                    wornHead |= static_cast<std::uint32_t>(armo->GetSlotMask()) &
                                kHT2HeadSlots;
                }
            }
            return (a_candidateMask & wornHead) != 0;
        }
    }

    void PreviewSession::ToggleImpl(RE::FormID a_formID, std::uint32_t a_variantKey,
                                    std::uint32_t a_mask, const std::string& a_name) {
        // Engine queries BEFORE the lock; lock regions stay leaf-pure over the set.
        const bool wornByPlayer = IsFormWornByPlayer(a_formID);
        const bool headHidden   = HiddenHeadgearConflict(a_mask);

        PreviewSet::Change change;
        {
            std::scoped_lock l(lock_);
            // Rejections only block ADDING; toggling an existing preview
            // off must always work.
            if ((wornByPlayer || headHidden) && !set_.ContainsExact(a_formID, a_variantKey)) {
                change = PreviewSet::Change::kRejected;
            } else {
                change = set_.Toggle(a_formID, a_variantKey, a_mask);
            }
        }
        if (change == PreviewSet::Change::kRejected) {
            if (wornByPlayer) {
                // Reachable from the inventory menu and from barter's sell
                // tab - both list the player's own (possibly worn) gear.
                spdlog::info("preview rejected: '{}' ({:08X}) is currently worn.", a_name,
                             a_formID);
            } else {
                spdlog::info("preview rejected: '{}' ({:08X}, mask={:08X}) targets a head "
                             "slot hidden by Helmet Toggle (HT_HelmetState > 0).",
                             a_name, a_formID, a_mask);
                RE::DebugNotification("Cannot preview: headwear is hidden by Helmet Toggle.",
                                      "UIMenuCancel");
            }
            return;
        }
        spdlog::info("preview {} '{}' ({:08X} variant={:08X}, mask={:08X})",
                     change == PreviewSet::Change::kAdded ? "ON" : "OFF", a_name,
                     a_formID, a_variantKey, a_mask);
        const auto& cfg = Settings::GetSingleton();
        REAug::RefreshPlayer(cfg.sceneKick);
        ListMarker::Apply();
        // Preview ON: clear the floating item-card model so it doesn't
        // block the view of the character (the hover thunk keeps it clear
        // while the row stays previewed). Preview OFF: reload the hovered
        // row's model, unless the user opted out of hover models entirely.
        if (cfg.clearModelOnPreview) {
            if (auto* i3d = RE::Inventory3DManager::GetSingleton()) {
                if (change == PreviewSet::Change::kAdded) {
                    i3d->Clear3D();
                } else if (auto* entry = HoverTracker::GetHovered();
                           entry && !cfg.suppressHoverModel) {
                    auto* obj = entry->GetObject();
                    if (obj && obj->GetFormID() == a_formID &&
                        VariantKey(entry) == a_variantKey) {
                        i3d->UpdateItem3D(entry);
                    }
                }
            }
        }
    }

    void PreviewSession::Clear() {
        bool wasActive;
        {
            std::scoped_lock l(lock_);
            wasActive = !set_.Empty();
            set_.Clear();
        }
        if (wasActive) {
            SKSE::GetTaskInterface()->AddTask([] {
                REAug::RefreshPlayer(Settings::GetSingleton().sceneKick);
                ListMarker::Apply();  // no-op if the menu already closed (disarmed)
            });
            spdlog::info("preview CLEARED.");
        }
    }

    void PreviewSession::OnPlayerAcquired(RE::FormID a_formID) {
        RemoveFormAndRefresh(a_formID, "acquired by player");
    }

    void PreviewSession::OnPlayerEquipped(RE::FormID a_formID) {
        // Equipping the previewed piece is the preview succeeding: the real
        // equip takes over the slot, so the preview ends instead of
        // double-visiting the same form in the rebuild.
        RemoveFormAndRefresh(a_formID, "equipped by player");
    }

    void PreviewSession::RemoveFormAndRefresh(RE::FormID a_formID, const char* a_reason) {
        bool removed;
        {
            std::scoped_lock l(lock_);
            removed = set_.Remove(a_formID);
        }
        if (removed) {
            spdlog::info("previewed item {:08X} {} - dropped from preview.", a_formID, a_reason);
            SKSE::GetTaskInterface()->AddTask([] {
                REAug::RefreshPlayer(Settings::GetSingleton().sceneKick);
                ListMarker::Apply();
            });
        }
    }

    bool PreviewSession::IsActive() const {
        std::scoped_lock l(lock_);
        return !set_.Empty();
    }

    bool PreviewSession::IsPreviewedEntry(RE::FormID a_formID, std::uint32_t a_variantKey) const {
        std::scoped_lock l(lock_);
        return set_.ContainsExact(a_formID, a_variantKey);
    }

    std::uint32_t PreviewSession::PreviewMask() const {
        std::scoped_lock l(lock_);
        return set_.UnionMask();
    }

    void PreviewSession::VisitPreviewArmors(const std::function<void(RE::TESObjectARMO*)>& a_fn) const {
        std::vector<PreviewSet::Entry> snapshot;
        {
            std::scoped_lock l(lock_);
            snapshot = set_.Entries();
        }
        for (const auto& e : snapshot) {
            if (auto* armo = RE::TESForm::LookupByID<RE::TESObjectARMO>(e.formID)) {
                a_fn(armo);
            }
        }
    }

}  // namespace AP
