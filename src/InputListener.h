#pragma once
#include "PCH.h"

namespace AP {

    // Preview-key handling (keyboard + gamepad) while a supported menu is
    // armed, plus the sinks that end a preview when its item stops being
    // previewable: acquired (container/barter) or equipped (inventory menu).
    class InputListener : public RE::BSTEventSink<RE::InputEvent*>,
                          public RE::BSTEventSink<RE::TESContainerChangedEvent>,
                          public RE::BSTEventSink<RE::TESEquipEvent> {
    public:
        static InputListener& GetSingleton();
        static void           Register();         // kDataLoaded
        static void           ResolveBindings();  // kDataLoaded, after Settings::Load
        static void           InstallPOVGate();   // load-time, with the other vfunc gates

        // The resolved gamepad preview button as the Scaleform key code Flash
        // receives (266 + button-bit index; R3 0x0080 -> 273). 0 when no pad
        // binding or when the binding is not a plain button mask (triggers).
        // FIELD NOTE 2026-07-12: the engine can also deliver gamepad buttons
        // to Flash as TRANSLATED KEYBOARD codes (log evidence: R3 arrived as
        // 112/F1; other buttons as 18/9) - the correlation window below is
        // what actually catches those.
        [[nodiscard]] static std::uint32_t ResolvedPadScaleformKey();

        // True while a physical preview-pad-button edge (down or up) happened
        // within the last few frames - the Scaleform gate eats WHATEVER key
        // code Flash receives in that window, regardless of the engine's
        // gamepad->keyboard translation table.
        [[nodiscard]] static bool PadPressInFlight();

        // The resolved keyboard preview key (DIK scancode), for the nav-panel
        // button glyph. Valid after ResolveBindings().
        [[nodiscard]] static std::uint32_t KeyboardKey() { return GetSingleton().kbKey_; }

        // Last input device seen (tracked unconditionally) - the nav-panel
        // button uses it to show the keyboard key vs the gamepad (R3) glyph.
        [[nodiscard]] static bool LastInputWasGamepad() {
            return GetSingleton().lastInputDevice_.load() == RE::INPUT_DEVICE::kGamepad;
        }

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events,
                                              RE::BSTEventSource<RE::InputEvent*>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* a_event,
                                              RE::BSTEventSource<RE::TESContainerChangedEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event,
                                              RE::BSTEventSource<RE::TESEquipEvent>*) override;

    private:
        static bool POVGate(RE::TogglePOVHandler* a_this, RE::InputEvent* a_event);

        static inline REL::Relocation<decltype(&POVGate)> origPOVCanProcess_;

        std::uint32_t kbKey_{ 0x2E };   // DIK C fallback
        std::uint32_t padKey_{ 0xFF };  // kInvalid until resolved
        std::atomic<std::uint64_t> lastPadEdgeMs_{ 0 };  // steady_clock ms of the last pad-key edge
        std::atomic<RE::INPUT_DEVICE> lastInputDevice_{ RE::INPUT_DEVICE::kKeyboard };  // nav-button glyph
    };

}  // namespace AP
