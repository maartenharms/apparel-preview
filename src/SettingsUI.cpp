#include "PCH.h"

#include "SettingsUI.h"

#include "MenuWatcher.h"
#include "PreviewSession.h"
#include "Settings.h"

#include <SimpleIni.h>  // CSimpleIniA, referenced by FUCK_API.h's INI callback typedefs

#include "FUCK_API.h"

namespace AP {

    namespace {
        bool g_capturingKb = false;

        struct KeyMap {
            ImGuiKey      imKey;
            std::uint32_t dik;
            const char*   name;
        };
        // Letters + common rebind targets; DIK codes per DirectInput. (F1/F5 and
        // F9-F12 are left out on purpose: quicksave/quickload etc.)
        constexpr KeyMap kKeyMap[] = {
            { ImGuiKey_A, 0x1E, "A" }, { ImGuiKey_B, 0x30, "B" },
            { ImGuiKey_C, 0x2E, "C" }, { ImGuiKey_D, 0x20, "D" },
            { ImGuiKey_E, 0x12, "E" }, { ImGuiKey_F, 0x21, "F" },
            { ImGuiKey_G, 0x22, "G" }, { ImGuiKey_H, 0x23, "H" },
            { ImGuiKey_I, 0x17, "I" }, { ImGuiKey_J, 0x24, "J" },
            { ImGuiKey_K, 0x25, "K" }, { ImGuiKey_L, 0x26, "L" },
            { ImGuiKey_M, 0x32, "M" }, { ImGuiKey_N, 0x31, "N" },
            { ImGuiKey_O, 0x18, "O" }, { ImGuiKey_P, 0x19, "P" },
            { ImGuiKey_Q, 0x10, "Q" }, { ImGuiKey_R, 0x13, "R" },
            { ImGuiKey_S, 0x1F, "S" }, { ImGuiKey_T, 0x14, "T" },
            { ImGuiKey_U, 0x16, "U" }, { ImGuiKey_V, 0x2F, "V" },
            { ImGuiKey_W, 0x11, "W" }, { ImGuiKey_X, 0x2D, "X" },
            { ImGuiKey_Y, 0x15, "Y" }, { ImGuiKey_Z, 0x2C, "Z" },
            { ImGuiKey_F2, 0x3C, "F2" }, { ImGuiKey_F3, 0x3D, "F3" },
            { ImGuiKey_F4, 0x3E, "F4" }, { ImGuiKey_F6, 0x40, "F6" },
            { ImGuiKey_F7, 0x41, "F7" }, { ImGuiKey_F8, 0x42, "F8" },
        };

        const KeyMap* PollCapturedKey() {
            for (const auto& entry : kKeyMap) {
                if (FUCK::IsKeyPressed(entry.imKey, false)) {
                    return &entry;
                }
            }
            return nullptr;
        }

        // Tooltip for the item just drawn (shown only on hover).
        void Tip(const char* a_desc) {
            if (FUCK::IsItemHovered()) {
                FUCK::SetTooltip(a_desc);
            }
        }

        void DrawPanel() {
            auto& cfg   = Settings::GetSingleton();
            bool  dirty = false;

            FUCK::TextDisabled("Preview apparel on your character while looting, "
                               "shopping and in your inventory.");

            // Master switch. Unchecking it live is a real kill: drop any active
            // preview and disarm now rather than waiting for the menu to close.
            if (FUCK::Checkbox("Enable Apparel Preview", &cfg.enabled)) {
                dirty = true;
                if (!cfg.enabled) {
                    MenuWatcher::ForceReset("disabled via settings");
                }
            }
            Tip("Master switch. Unchecking it drops any active preview right away.");

            FUCK::SeparatorText("Where to preview");
            dirty |= FUCK::Checkbox("Container menu (looting)", &cfg.enableContainer);
            Tip("Show previews while looting chests, corpses and containers.");
            dirty |= FUCK::Checkbox("Barter menu (shopping)", &cfg.enableBarter);
            Tip("Show previews while trading with a merchant.");
            dirty |= FUCK::Checkbox("Inventory (your own items)", &cfg.enableInventory);
            Tip("Show previews in your own inventory.");

            FUCK::SeparatorText("Options");
            dirty |= FUCK::Checkbox("Allow shield preview", &cfg.allowShields);
            Tip("Include shields. Some sit oddly on the arm; turn this off to skip them.");
            dirty |= FUCK::Checkbox("Notify when a piece cannot fit your body",
                                    &cfg.notifyUnfitPreview);
            Tip("Show a message when a piece has no armour made for your race or "
                "body, instead of doing nothing.");

            FUCK::SeparatorText("Item card");
            dirty |= FUCK::Checkbox("Block the HUD-hiding inspect mode for apparel",
                                    &cfg.blockApparelInspect);
            Tip("Stops the item-zoom inspect view from hiding the HUD when you "
                "preview apparel.");
            dirty |= FUCK::Checkbox("Block camera POV switching while a preview menu is open",
                                    &cfg.blockPOVWhileArmed);
            Tip("Keeps the POV key or right-stick click from flipping the camera "
                "while a preview menu is open. For unpaused-menu setups (Skyrim Souls).");
            dirty |= FUCK::Checkbox("Clear the floating item model when previewing",
                                    &cfg.clearModelOnPreview);
            Tip("Hides the little rotating item model on the card while a preview "
                "is active.");
            dirty |= FUCK::Checkbox("Never show the item model for apparel",
                                    &cfg.suppressHoverModel);
            Tip("Always hides the rotating item model for apparel, for an "
                "unobstructed look at your character.");

            FUCK::SeparatorText("Preview key");
            if (cfg.previewKeyDIK == 0) {
                FUCK::Text("Following the game's 'Item Zoom' binding");
            } else {
                FUCK::Text("Override: DIK 0x%X", cfg.previewKeyDIK);
            }
            FUCK::SameLine(0.0f, 8.0f);
            if (!g_capturingKb) {
                if (FUCK::Button("Rebind##kb")) {
                    g_capturingKb = true;
                }
            } else {
                FUCK::Text("... press a key (letters / F-keys)");
                if (const auto* key = PollCapturedKey()) {
                    cfg.previewKeyDIK = key->dik;
                    g_capturingKb     = false;
                    dirty             = true;
                    spdlog::info("SettingsUI: preview key rebound to {} (DIK 0x{:X}).",
                                 key->name, key->dik);
                }
            }
            if (cfg.previewKeyDIK != 0) {
                FUCK::SameLine(0.0f, 8.0f);
                if (FUCK::Button("Reset to Item Zoom##kb")) {
                    cfg.previewKeyDIK = 0;
                    dirty             = true;
                }
            }
            Tip("The key that toggles a preview on the hovered item. Takes effect "
                "after the next game load.");

            FUCK::SeparatorText("Actions");
            if (FUCK::Button("Clear active preview")) {
                PreviewSession::GetSingleton().Clear();
            }
            Tip("Remove any preview showing on your character right now.");

            if (dirty) {
                cfg.Save();
            }
        }

        // FLICK sidebar entry: the user opens FUCK (hotkey / controller menu) and
        // picks "Apparel Preview".
        class SettingsTool : public FUCK::ITool {
        public:
            const char* Name() const override { return "Apparel Preview"; }
            void        Draw() override { DrawPanel(); }
        };

        SettingsTool g_settingsTool;  // process-lifetime; registered pointer stays valid
    }

    void SettingsUI::Register() {
        // Soft dependency: without FUCK.dll the mod stays INI-only with one log
        // line. The name passed here is what FLICK shows in its sidebar / the
        // registered-plugin panel.
        if (!FUCK::Connect("Apparel Preview")) {
            spdlog::info("SettingsUI: FUCK / FLICK not present; INI-only mode.");
            return;
        }
        FUCK::RegisterTool(&g_settingsTool);
        spdlog::info("SettingsUI: settings registered as a FLICK (FUCK) sidebar tool.");
    }

}  // namespace AP
