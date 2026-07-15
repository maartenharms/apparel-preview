#pragma once
#include "PCH.h"

namespace AP {

    // Arms the mod while ContainerMenu/BarterMenu/InventoryMenu is open; the
    // close event is the master safety net that clears any active preview.
    class MenuWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        static MenuWatcher& GetSingleton();
        static void         Register();  // call at kDataLoaded

        [[nodiscard]] static bool IsArmed();

        // Armed AND a supported menu is genuinely open. Paths reachable from
        // GAMEPLAY (the POV gate, the preview key) must use this: a missed
        // close event - e.g. loading a save taken with the menu open, which
        // Skyrim Souls RE makes possible - would otherwise strand armed_ and
        // silently break POV switching / deref a freed hover entry.
        [[nodiscard]] static bool IsArmedAndMenuOpen();

        // kPreLoadGame / kNewGame backstop: drop all menu-scoped state.
        static void ForceReset(const char* a_reason);

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
                                              RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

    private:
        std::atomic<bool> armed_{ false };
    };

}  // namespace AP
