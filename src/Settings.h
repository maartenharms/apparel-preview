#pragma once
#include "PCH.h"

namespace AP {

    // Runtime settings, INI-persisted (Data/SKSE/Plugins/ApparelPreview.ini).
    struct Settings {
        static Settings& GetSingleton();

        void Load();  // kDataLoaded
        void Save();  // on change from the settings UI

        bool enabled{ true };
        bool enableContainer{ true };
        bool enableBarter{ true };
        bool enableInventory{ true };  // player inventory (InventoryMenu)
        bool sceneKick{ true };            // paused-menu render kick
        bool allowShields{ true };         // shields flagged for testing (spec §7)
        bool notifyUnfitPreview{ true };   // notification when a piece has no armature
                                           // for the player's race (UBE etc.) - the
                                           // preview is declined either way
        bool suppressHoverModel{ false };  // hide the item-card 3D for apparel on hover
        bool clearModelOnPreview{ true };  // clear the floating item model when toggling
        bool blockApparelInspect{ true };  // keep the zoom/inspect mode (HUD-hiding) off for apparel
        bool blockPOVWhileArmed{ true };   // no camera POV switch while Container/Barter is open
                                           // (matters with unpaused-menu mods; vanilla pause = inert)
        // The gold DIII eye icon is the only previewed-row marker; it shows
        // automatically wherever Dynamic Inventory Icon Injector is installed.

        // 0 = resolve from the ControlMap's "Item Zoom" binding at kDataLoaded.
        std::uint32_t previewKeyDIK{ 0 };
        std::uint32_t previewGamepadButton{ 0 };

        // Native "Preview" prompt added to SkyUI's item-card nav panel (the same
        // mechanism as Compare Equipment NG's "Compare"): shows only on hovered
        // PREVIEWABLE apparel; the preview key does the preview. Needs SkyUI.
        bool        showPreviewButton{ true };
        std::string previewButtonText{ "Preview" };
    };

}  // namespace AP
