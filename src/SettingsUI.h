#pragma once
#include "PCH.h"

namespace AP {

    // FLICK (FUCK) sidebar tool (soft dependency: INI-only when FUCK.dll is
    // absent). Register at kDataLoaded.
    class SettingsUI {
    public:
        static void Register();  // kDataLoaded
    };

}  // namespace AP
