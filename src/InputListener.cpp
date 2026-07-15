#include "InputListener.h"

#include "HoverTracker.h"
#include "MenuWatcher.h"
#include "PreviewSession.h"
#include "Settings.h"

#include <bit>
#include <chrono>

namespace {
    std::uint64_t NowMs() {
        using namespace std::chrono;
        return static_cast<std::uint64_t>(
            duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }
}

namespace AP {

    InputListener& InputListener::GetSingleton() {
        static InputListener instance;
        return instance;
    }

    void InputListener::Register() {
        if (auto* idm = RE::BSInputDeviceManager::GetSingleton()) {
            idm->AddEventSink(static_cast<RE::BSTEventSink<RE::InputEvent*>*>(&GetSingleton()));
        }
        if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
            holder->AddEventSink(
                static_cast<RE::BSTEventSink<RE::TESContainerChangedEvent>*>(&GetSingleton()));
            holder->AddEventSink(
                static_cast<RE::BSTEventSink<RE::TESEquipEvent>*>(&GetSingleton()));
        }
        spdlog::info("InputListener registered.");
    }

    bool InputListener::PadPressInFlight() {
        const auto last = GetSingleton().lastPadEdgeMs_.load();
        return last != 0 && NowMs() - last <= 80;  // same-frame + jitter headroom
    }

    std::uint32_t InputListener::ResolvedPadScaleformKey() {
        const auto pad = GetSingleton().padKey_;
        // XInput button masks are single bits (DPAD_UP 0x0001 ... Y 0x8000);
        // Scaleform key codes count up from 266 in the same bit order -
        // verified against SkyUI config comments (BACK 0x20 -> 271, L3 0x40 ->
        // 272, LB 0x100 -> 274, RB 0x200 -> 275). Trigger codes (0x09/0x0A)
        // are not masks and get no translation.
        if (pad == 0 || pad == 0xFF || !std::has_single_bit(pad) || pad > 0x8000) {
            return 0;
        }
        return 266 + static_cast<std::uint32_t>(std::countr_zero(pad));
    }

    void InputListener::InstallPOVGate() {
        REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_TogglePOVHandler[0] };
        origPOVCanProcess_ = vtbl.write_vfunc(0x1, POVGate);
        spdlog::info("InputListener: POV gate installed (TogglePOVHandler::CanProcess).");
    }

    bool InputListener::POVGate(RE::TogglePOVHandler* a_this, RE::InputEvent* a_event) {
        // With unpaused-menu mods (Skyrim Souls RE), the gameplay input
        // context stays live while Container/Barter is open, so the vanilla
        // POV toggle (right-stick click on gamepad - the same physical button
        // as Item Zoom) fires alongside our preview toggle. Vanilla paused
        // menus never allow a POV switch, so declining here while armed just
        // restores vanilla behavior. Inert when menus pause (never called).
        //
        // Decline only the togglePOV event: vanilla's CanProcess answers true
        // for exactly that event and false for everything else, so anything
        // broader could only suppress a third-party chain for no gain.
        // IsArmedAndMenuOpen (not IsArmed) - a stranded armed_ must never cost
        // the player their POV key in gameplay.
        const auto& cfg = Settings::GetSingleton();
        if (a_event && cfg.enabled && cfg.blockPOVWhileArmed &&
            MenuWatcher::IsArmedAndMenuOpen()) {
            const auto* ue = RE::UserEvents::GetSingleton();
            if (ue && a_event->QUserEvent() == ue->togglePOV) {
                spdlog::debug("TogglePOV gate: declined");
                return false;
            }
        }
        return origPOVCanProcess_(a_this, a_event);
    }

    void InputListener::ResolveBindings() {
        auto&       self = GetSingleton();
        const auto& cfg  = Settings::GetSingleton();
        const auto* cm   = RE::ControlMap::GetSingleton();
        const auto* ue   = RE::UserEvents::GetSingleton();

        self.kbKey_  = cfg.previewKeyDIK;
        self.padKey_ = cfg.previewGamepadButton;
        if (cm && ue) {
            using Ctx = RE::UserEvents::INPUT_CONTEXT_ID;
            if (self.kbKey_ == 0) {
                self.kbKey_ = cm->GetMappedKey(ue->itemZoom, RE::INPUT_DEVICE::kKeyboard, Ctx::kItemMenu);
            }
            if (self.padKey_ == 0) {
                self.padKey_ = cm->GetMappedKey(ue->itemZoom, RE::INPUT_DEVICE::kGamepad, Ctx::kItemMenu);
            }
        }
        if (self.kbKey_ == 0 || self.kbKey_ == 0xFF) {
            self.kbKey_ = 0x2E;  // DIK C - vanilla Item Zoom
        }
        spdlog::info("preview bindings: keyboard=0x{:X} gamepad=0x{:X} (0xFF = none).",
                     self.kbKey_, self.padKey_);
    }

    RE::BSEventNotifyControl InputListener::ProcessEvent(RE::InputEvent* const* a_events,
                                                         RE::BSTEventSource<RE::InputEvent*>*) {
        // Track the active device unconditionally (before the armed gate) so the
        // nav-panel button glyph can follow keyboard vs. controller.
        if (a_events) {
            for (auto* e = *a_events; e; e = e->next) {
                lastInputDevice_.store(e->GetDevice());
            }
        }
        const auto& cfg = Settings::GetSingleton();
        // Menu-open verified: never dereference a stale hovered entry from a
        // menu session that ended without a close event.
        if (!a_events || !cfg.enabled || !MenuWatcher::IsArmedAndMenuOpen()) {
            return RE::BSEventNotifyControl::kContinue;
        }
        for (auto* e = *a_events; e; e = e->next) {
            const auto* btn = e->AsButtonEvent();
            if (!btn) {
                continue;
            }
            // Every EDGE of the physical preview pad button stamps the
            // correlation window - the menus' Scaleform gate eats whatever
            // key code the engine translates this press into (field: R3
            // reached Flash as 112/F1 and drove SkyUI's sort + filter).
            if (btn->GetDevice() == RE::INPUT_DEVICE::kGamepad &&
                btn->GetIDCode() == padKey_ && (btn->IsDown() || btn->IsUp())) {
                lastPadEdgeMs_ = NowMs();
            }
            if (!btn->IsDown()) {
                continue;
            }
            const auto device = btn->GetDevice();
            const auto code   = btn->GetIDCode();
            const bool match  = (device == RE::INPUT_DEVICE::kKeyboard && code == kbKey_) ||
                                (device == RE::INPUT_DEVICE::kGamepad && code == padKey_);
            if (!match) {
                continue;
            }
            if (auto* entry = HoverTracker::GetHovered()) {
                auto* obj  = entry->GetObject();
                auto* armo = obj ? obj->As<RE::TESObjectARMO>() : nullptr;
                if (armo) {
                    PreviewSession::GetSingleton().Toggle(
                        armo, PreviewSession::VariantKey(entry));
                } else {
                    spdlog::debug("preview key: hovered item is not apparel");
                }
            } else {
                spdlog::debug("preview key: no hovered entry");
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl InputListener::ProcessEvent(
        const RE::TESContainerChangedEvent* a_event,
        RE::BSTEventSource<RE::TESContainerChangedEvent>*) {
        if (a_event && PreviewSession::GetSingleton().IsActive()) {
            const auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && a_event->newContainer == player->GetFormID()) {
                PreviewSession::GetSingleton().OnPlayerAcquired(a_event->baseObj);
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl InputListener::ProcessEvent(
        const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) {
        // Equipping a previewed form (reachable from the inventory menu) ends
        // its preview - the real equip owns the slot from here on.
        if (a_event && a_event->equipped && a_event->actor &&
            a_event->actor->IsPlayerRef() && PreviewSession::GetSingleton().IsActive()) {
            PreviewSession::GetSingleton().OnPlayerEquipped(a_event->baseObject);
        }
        return RE::BSEventNotifyControl::kContinue;
    }

}  // namespace AP
