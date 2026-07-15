#include "Settings.h"

#define SI_NO_CONVERSION 1
#include <SimpleIni.h>

namespace AP {

    namespace {
        const char* IniPath() { return "Data/SKSE/Plugins/ApparelPreview.ini"; }
    }

    Settings& Settings::GetSingleton() {
        static Settings instance;
        return instance;
    }

    void Settings::Load() {
        CSimpleIniA ini;
        ini.SetUnicode();
        if (ini.LoadFile(IniPath()) < 0) {
            spdlog::info("Settings: no INI yet, writing defaults.");
            Save();
            return;
        }
        enabled             = ini.GetBoolValue("General", "bEnabled", enabled);
        enableContainer     = ini.GetBoolValue("General", "bEnableContainerMenu", enableContainer);
        enableBarter        = ini.GetBoolValue("General", "bEnableBarterMenu", enableBarter);
        enableInventory     = ini.GetBoolValue("General", "bEnableInventoryMenu", enableInventory);
        suppressHoverModel  = ini.GetBoolValue("General", "bSuppressApparelHoverModel", suppressHoverModel);
        clearModelOnPreview = ini.GetBoolValue("General", "bClearItemModelOnPreview", clearModelOnPreview);
        blockApparelInspect = ini.GetBoolValue("General", "bBlockApparelInspect", blockApparelInspect);
        blockPOVWhileArmed  = ini.GetBoolValue("General", "bBlockPOVSwitch", blockPOVWhileArmed);
        notifyUnfitPreview  = ini.GetBoolValue("General", "bNotifyUnfitPreview", notifyUnfitPreview);
        sceneKick           = ini.GetBoolValue("Advanced", "bSceneKick", sceneKick);
        allowShields        = ini.GetBoolValue("Advanced", "bAllowShields", allowShields);
        previewKeyDIK       = static_cast<std::uint32_t>(
            ini.GetLongValue("Input", "iPreviewKeyDIK", static_cast<long>(previewKeyDIK)));
        previewGamepadButton = static_cast<std::uint32_t>(
            ini.GetLongValue("Input", "iPreviewGamepadButton", static_cast<long>(previewGamepadButton)));
        showPreviewButton = ini.GetBoolValue("General", "bShowPreviewButton", showPreviewButton);
        if (const char* t = ini.GetValue("General", "sPreviewButtonText", previewButtonText.c_str());
            t && *t) {
            previewButtonText = t;
        }
        spdlog::info("Settings loaded (key override=0x{:X}, pad override=0x{:X}).",
                     previewKeyDIK, previewGamepadButton);
    }

    void Settings::Save() {
        CSimpleIniA ini;
        ini.SetUnicode();
        ini.SetBoolValue("General", "bEnabled", enabled);
        ini.SetBoolValue("General", "bEnableContainerMenu", enableContainer);
        ini.SetBoolValue("General", "bEnableBarterMenu", enableBarter);
        ini.SetBoolValue("General", "bEnableInventoryMenu", enableInventory);
        ini.SetBoolValue("General", "bSuppressApparelHoverModel", suppressHoverModel);
        ini.SetBoolValue("General", "bClearItemModelOnPreview", clearModelOnPreview);
        ini.SetBoolValue("General", "bBlockApparelInspect", blockApparelInspect);
        ini.SetBoolValue("General", "bBlockPOVSwitch", blockPOVWhileArmed,
                         "; block camera POV switching while the container/barter menu is open\n"
                         "; (only relevant with unpaused-menu mods like Skyrim Souls RE)");
        ini.SetBoolValue("General", "bNotifyUnfitPreview", notifyUnfitPreview,
                         "; show a notification when a piece cannot fit your body\n"
                         "; (no armature for your race, e.g. unpatched gear on a UBE body);\n"
                         "; the preview is declined either way");
        ini.SetBoolValue("General", "bShowPreviewButton", showPreviewButton,
                         "; add a native 'Preview' prompt to SkyUI's item-card button bar\n"
                         "; for hovered previewable apparel (the preview key does it; needs SkyUI)");
        ini.SetValue("General", "sPreviewButtonText", previewButtonText.c_str(),
                     "; label shown on that prompt");
        ini.SetBoolValue("Advanced", "bSceneKick", sceneKick);
        ini.SetBoolValue("Advanced", "bAllowShields", allowShields);
        ini.SetLongValue("Input", "iPreviewKeyDIK", static_cast<long>(previewKeyDIK),
                         "; 0 = follow the game's 'Item Zoom' binding");
        ini.SetLongValue("Input", "iPreviewGamepadButton", static_cast<long>(previewGamepadButton),
                         "; 0 = follow the game's 'Item Zoom' binding");
        std::error_code ec;
        std::filesystem::create_directories("Data/SKSE/Plugins", ec);
        ini.SaveFile(IniPath());
        spdlog::info("Settings saved.");
    }

}  // namespace AP
