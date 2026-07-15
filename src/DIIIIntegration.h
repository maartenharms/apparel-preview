#pragma once
#include "PCH.h"

namespace AP {

    // Soft integration with Dynamic Inventory Icon Injector (Nexus 174136):
    // registers an "apparelPreviewed" match field so a DIII rule (shipped in
    // dist/SKSE/Plugins/DIII/zzz_ApparelPreview.json) can show our eye icon on
    // previewed entries. No-op when DIII isn't installed.
    class DIIIIntegration {
    public:
        static void Register();  // call at kPostLoad (sender-name resolution)

        // True once the 'apparelPreviewed' condition is registered with DIII -
        // i.e. the eye icon will be shown and text fallbacks are redundant.
        [[nodiscard]] static bool IsActive();
    };

}  // namespace AP
