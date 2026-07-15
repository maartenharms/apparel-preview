#pragma once
#include "PCH.h"

namespace AP {

    // Marks previewed entries in the open Container/Barter item list with the
    // gold Dynamic Inventory Icon Injector eye icon (the only marker). Edits
    // each entry's _DIIIIcons array in place and invalidates the list, so no
    // engine rebuild and no stutter. No-op when DIII is not installed.
    class ListMarker {
    public:
        // Sync markers with the current preview set. Game thread, menu open;
        // idempotent, cheap when nothing changed.
        static void Apply();

    private:
        static void ApplyToList(RE::ItemList* a_list);
    };

}  // namespace AP
