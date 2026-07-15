#include "HoverTracker.h"

#include "InputListener.h"
#include "MenuWatcher.h"
#include "NavButton.h"
#include "PreviewSession.h"
#include "Settings.h"

#include <thread>

namespace AP {

    namespace {
        // GFx 3.x GFxKeyEvent layout (CommonLibSSE-NG does not declare it):
        // the key code directly follows GFxEvent::type - the same packing as
        // GFxMouseEvent, whose first member sits at 0x04.
        struct GFxKeyEventView : RE::GFxEvent {
            std::uint32_t keyCode;  // 04
        };
    }

    void HoverTracker::Install() {
        // ItemCard hover hook. On AE 1.6.1170 this call moved to +0x22C (the AE
        // function 51897 has extra early-body code); the hooked instruction
        // stream is byte-identical to SE 51019 (SE +0x114 / AE +0x22C) (verified vs the AE binary).
        const REL::Relocation<std::uintptr_t> site{ REL::RelocationID(51019, 51897),
                                                    REL::VariantOffset(0x114, 0x22C, 0x114) };
        if (*reinterpret_cast<std::uint8_t*>(site.address()) != 0xE8) {
            spdlog::error("HoverTracker: expected E8 at 51019 (SE +0x114 / AE +0x22C), found {:02X} - hook NOT installed.",
                          *reinterpret_cast<std::uint8_t*>(site.address()));
            return;
        }
        auto& trampoline = SKSE::GetTrampoline();
        func_ = trampoline.write_call<5>(site.address(), Thunk);
        spdlog::info("HoverTracker: hover hook installed at 51019 (SE +0x114 / AE +0x22C).");

        REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_Inventory3DManager[0] };
        origCanProcess_ = vtbl.write_vfunc(0x1, CanProcessGate);

        REL::Relocation<std::uintptr_t> contVtbl{ RE::VTABLE_ContainerMenu[0] };
        origContainerPM_ = contVtbl.write_vfunc(0x4, ContainerProcessMessage);
        REL::Relocation<std::uintptr_t> bartVtbl{ RE::VTABLE_BarterMenu[0] };
        origBarterPM_ = bartVtbl.write_vfunc(0x4, BarterProcessMessage);
        REL::Relocation<std::uintptr_t> invVtbl{ RE::VTABLE_InventoryMenu[0] };
        origInventoryPM_ = invVtbl.write_vfunc(0x4, InventoryProcessMessage);
        spdlog::info("HoverTracker: inspect gates installed (I3DM CanProcess + menu ProcessMessage).");
    }

    bool HoverTracker::CanProcessGate(RE::Inventory3DManager* a_this, RE::InputEvent* a_event) {
        if (a_event && MenuWatcher::IsArmed() && Settings::GetSingleton().blockApparelInspect) {
            const auto* ue = RE::UserEvents::GetSingleton();
            if (ue && a_event->QUserEvent() == ue->itemZoom) {
                auto* entry = hovered_.load();
                auto* obj   = entry ? entry->GetObject() : nullptr;
                const bool eat = obj && obj->As<RE::TESObjectARMO>() != nullptr;
                spdlog::debug("I3DM CanProcess(itemZoom): apparel={} -> {}", eat,
                              eat ? "declined" : "passed");
                if (eat) {
                    return false;  // no zoom/inspect for apparel - key toggles the preview
                }
            }
        }
        return origCanProcess_(a_this, a_event);
    }

    bool HoverTracker::ShouldEatZoomMessage(RE::UIMessage& a_message) {
        if (!MenuWatcher::IsArmed() || !Settings::GetSingleton().blockApparelInspect) {
            return false;
        }
        if (a_message.type != RE::UI_MESSAGE_TYPE::kUserEvent || !a_message.data) {
            return false;
        }
        const auto* ue   = RE::UserEvents::GetSingleton();
        auto*       data = static_cast<RE::BSUIMessageData*>(a_message.data);
        if (!ue || data->fixedStr != ue->itemZoom) {
            return false;
        }
        auto* entry = hovered_.load();
        auto* obj   = entry ? entry->GetObject() : nullptr;
        const bool eat = obj && obj->As<RE::TESObjectARMO>() != nullptr;
        spdlog::debug("menu ProcessMessage(itemZoom userEvent): apparel={} -> {}", eat,
                      eat ? "eaten" : "passed");
        return eat;
    }

    bool HoverTracker::ShouldEatScaleformKey(RE::UIMessage& a_message) {
        if (!MenuWatcher::IsArmed() || !Settings::GetSingleton().blockApparelInspect) {
            return false;
        }
        if (a_message.type != RE::UI_MESSAGE_TYPE::kScaleformEvent || !a_message.data) {
            return false;
        }
        const auto* data = static_cast<RE::BSUIScaleformData*>(a_message.data);
        const auto* ev   = data->scaleformEvent;
        using ET = RE::GFxEvent::EventType;
        if (!ev || (ev->type != ET::kKeyDown && ev->type != ET::kKeyUp)) {
            return false;
        }
        const auto code   = static_cast<const GFxKeyEventView*>(ev)->keyCode;
        const auto wanted = InputListener::ResolvedPadScaleformKey();
        // Two ways this key event belongs to the preview button:
        //  (a) it IS the 266+bit code we computed from the binding, or
        //  (b) it arrived in the correlation window of a physical pad-button
        //      edge - the engine delivers gamepad buttons to Flash as
        //      TRANSLATED KEYBOARD codes (field log 2026-07-12: R3 came in
        //      as 112/F1 and drove SkyUI's column sort AND its filter box;
        //      273 never arrived at all).
        const bool isPreviewCode = wanted != 0 && code == wanted;
        const bool correlated    = InputListener::PadPressInFlight();
        if (!isPreviewCode && !correlated) {
            if (ev->type == ET::kKeyDown) {
                spdlog::debug("scaleform keyDown {} passed (preview key {}, no pad edge)",
                              code, wanted);
            }
            return false;
        }
        // Eat for apparel AND for empty hovers. The empty-hover press has no
        // preview target and no zoom target - the only consumer left is the
        // UI's own binding on the same physical button (SkyUI/Vel'dun filter
        // box), which is exactly the mode flip the field reported: scrolling
        // on controller clears the hover for a beat, and a press in that
        // window fell through to Flash. Non-apparel hovers keep vanilla
        // item zoom.
        auto* entry = hovered_.load();
        auto* obj   = entry ? entry->GetObject() : nullptr;
        const bool eat = !entry || (obj && obj->As<RE::TESObjectARMO>() != nullptr);
        if (eat && ev->type == ET::kKeyDown) {
            spdlog::debug("scaleform key {} eaten ({}; {} - preview button)", code,
                          isPreviewCode ? "bound code" : "pad-edge correlated",
                          entry ? "apparel hovered" : "no hover");
        }
        return eat;  // both down and up, so Flash never sees half a press
    }

    RE::UI_MESSAGE_RESULTS HoverTracker::ContainerProcessMessage(RE::ContainerMenu* a_this,
                                                                 RE::UIMessage& a_message) {
        if (ShouldEatZoomMessage(a_message) || ShouldEatScaleformKey(a_message)) {
            return RE::UI_MESSAGE_RESULTS::kHandled;
        }
        return origContainerPM_(a_this, a_message);
    }

    RE::UI_MESSAGE_RESULTS HoverTracker::BarterProcessMessage(RE::BarterMenu* a_this,
                                                              RE::UIMessage& a_message) {
        if (ShouldEatZoomMessage(a_message) || ShouldEatScaleformKey(a_message)) {
            return RE::UI_MESSAGE_RESULTS::kHandled;
        }
        return origBarterPM_(a_this, a_message);
    }

    RE::UI_MESSAGE_RESULTS HoverTracker::InventoryProcessMessage(RE::InventoryMenu* a_this,
                                                                 RE::UIMessage& a_message) {
        if (ShouldEatZoomMessage(a_message) || ShouldEatScaleformKey(a_message)) {
            return RE::UI_MESSAGE_RESULTS::kHandled;
        }
        return origInventoryPM_(a_this, a_message);
    }

    RE::InventoryEntryData* HoverTracker::GetHovered() { return hovered_.load(); }
    void                    HoverTracker::ClearHovered() { hovered_ = nullptr; }

    std::int64_t HoverTracker::Thunk(RE::InventoryEntryData* a_entry) {
        if (a_entry && MenuWatcher::IsArmed()) {
            hovered_ = a_entry;
            // Hover-model suppression: the Flash side calls UpdateItem3D (loads
            // the card model) BEFORE RequestItemCardInfo (this thunk), so
            // clearing here removes the floating model for apparel without
            // touching Inventory3DManager's code. Two cases: the optional
            // always-suppress, and re-hovering a row that is currently
            // previewed (the on-body preview replaces the card model, so
            // scrolling away and back must not bring it back).
            if (auto* obj = a_entry->GetObject(); obj && obj->As<RE::TESObjectARMO>()) {
                const auto& cfg = Settings::GetSingleton();
                const bool hidePreviewed =
                    cfg.clearModelOnPreview &&
                    PreviewSession::GetSingleton().IsPreviewedEntry(
                        obj->GetFormID(), PreviewSession::VariantKey(a_entry));
                if (cfg.suppressHoverModel || hidePreviewed) {
                    if (auto* i3d = RE::Inventory3DManager::GetSingleton()) {
                        i3d->Clear3D();
                        spdlog::debug("item3d cleared for '{}'{}", a_entry->GetDisplayName(),
                                      hidePreviewed ? " (previewed)" : "");
                    }
                }
            }
            // Checkpoint-1 diagnostics: which menu, what item, which thread.
            // variant= proves rows of one base form are distinguishable (it
            // differs per row) and that the menu really hands us one entry
            // per row rather than one shared entry per base form.
            auto*       ui   = RE::UI::GetSingleton();
            const char* menu = "Container";
            if (ui && ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
                menu = "Barter";
            } else if (ui && ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
                menu = "Inventory";
            }
            const auto* obj  = a_entry->GetObject();
            spdlog::debug("hover[{}] {:08X} '{}' armo={} variant={:08X} entry={} thread={}", menu,
                          obj ? obj->GetFormID() : 0, a_entry->GetDisplayName(),
                          obj && obj->As<RE::TESObjectARMO>() != nullptr,
                          PreviewSession::VariantKey(a_entry),
                          static_cast<const void*>(a_entry),
                          std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFF);
            // Refresh the native "Preview" nav-panel prompt for this selection
            // (adds it for previewable apparel, removes it otherwise). Deferred
            // to a UI task so it lands after SkyUI rebuilds the bottom bar.
            if (Settings::GetSingleton().showPreviewButton) {
                SKSE::GetTaskInterface()->AddUITask([]() { NavButton::Refresh(); });
            }
        }
        return func_(a_entry);
    }

}  // namespace AP
