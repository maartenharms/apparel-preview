#pragma once
#include "PCH.h"

namespace AP {

    // Hooks on the engine's biped-skinning pass (SOS-documented SE sites).
    // Task 4 installs only the injection path; Task 5 adds suppression + the
    // worn-mask shim.
    class BipedHooks {
    public:
        static void Install();
        [[nodiscard]] static bool AllInstalled();

        // Times the engine's worn-skinning pass ran for the player. Lets the
        // refresh path detect whether a rebuild actually executed (paused-menu
        // diagnosis) instead of assuming.
        [[nodiscard]] static std::uint64_t PlayerWornPassCount();

    private:
        static void InstallInjection();     // 24231+0x81 (worn-pass wrap + preview inject)
        static void InstallCapture();       // 24232+0x302 (diagnostic: dormant during rebuilds on 1.5.97)
        static void InstallWornMaskShim();  // 24220+0x7C (mask |= preview slots while active)

        static inline bool injectionOk_ = false;
        static inline bool captureOk_   = false;
        static inline bool maskShimOk_  = false;
    };

}  // namespace AP
