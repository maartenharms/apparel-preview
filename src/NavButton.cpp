#include "NavButton.h"

#include "HoverTracker.h"
#include "InputListener.h"
#include "MenuWatcher.h"
#include "Settings.h"

#include <vector>

namespace AP::NavButton {

    namespace {
        // Mirrors PreviewSession::Toggle's primary gate: apparel that has an
        // armature for the player's race (fails on UBE/unpatched gear) and is
        // not a disabled shield. HT2's worn-state decline is left to Toggle.
        bool CanPreview(RE::TESObjectARMO* a_armo) {
            if (!a_armo) {
                return false;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* race   = player ? player->GetRace() : nullptr;
            if (!race || !a_armo->GetArmorAddon(race)) {
                return false;  // no armature for this body - cannot preview
            }
            using Slot      = RE::BGSBipedObjectForm::BipedObjectSlot;
            const auto mask = static_cast<std::uint32_t>(a_armo->GetSlotMask());
            if (!Settings::GetSingleton().allowShields &&
                (mask & static_cast<std::uint32_t>(Slot::kShield)) != 0) {
                return false;  // shield preview disabled
            }
            return true;
        }

        RE::GFxMovieView* ItemMenuView() {
            auto* ui = RE::UI::GetSingleton();
            if (!ui) {
                return nullptr;
            }
            for (auto name : { RE::InventoryMenu::MENU_NAME, RE::BarterMenu::MENU_NAME,
                               RE::ContainerMenu::MENU_NAME }) {
                if (auto menu = ui->GetMenu(name); menu && menu->uiMovie) {
                    return menu->uiMovie.get();
                }
            }
            return nullptr;
        }
    }

    void Refresh() {
        const auto& cfg = Settings::GetSingleton();
        if (!cfg.enabled || !cfg.showPreviewButton || !MenuWatcher::IsArmed()) {
            return;
        }

        auto* view = ItemMenuView();
        if (!view) {
            return;
        }
        RE::GFxValue navPanel;
        if (!view->GetVariable(&navPanel, "_root.Menu_mc.navPanel") || !navPanel.IsObject()) {
            return;  // not a SkyUI-style nav panel (non-SkyUI UI)
        }

        auto*      entry      = HoverTracker::GetHovered();
        auto*      obj        = entry ? entry->GetObject() : nullptr;
        const bool canPreview = obj && CanPreview(obj->As<RE::TESObjectARMO>());

        // Find ALL of our buttons in the nav panel's live button array. A list
        // rebuild (dropping/transferring an item) can re-emit the panel with a
        // second copy of our button, so match every occurrence and keep one.
        // The array is named differently across UIs: this SkyUI BottomBar
        // buttonPanel exposes it as `.buttons` (field navprobe: navPanel =
        // /Menu_mc/bottomBar/buttonPanel, `.buttons` = array of 6), while some
        // skins use `._buttonData`. Reading the wrong name is exactly why the
        // dedup was DEAD CODE and the button duplicated - every hover re-added
        // blindly. Try the known names; the first array wins.
        RE::GFxValue buttons;
        bool         haveArray = false;
        for (const char* arrName : { "buttons", "_buttonData", "buttonData" }) {
            if (navPanel.GetMember(arrName, &buttons) && buttons.IsArray()) {
                haveArray = true;
                break;
            }
        }
        // Identify OUR button in the array. Which member carries the label
        // differs by UI: a plain {text} entry (some SkyUI skins), or a Button
        // COMPONENT that keeps the original entry under `.data` (this UI - field
        // navprobe showed `buttons[i].text` is NOT a string). We also tag our own
        // entry with `apPreview` when we add it, which is the most robust match of
        // all. Try tag, then data, then plain text.
        const auto matchesOurs = [&](const RE::GFxValue& a_el) -> bool {
            if (!a_el.IsObject()) {
                return false;
            }
            RE::GFxValue m;
            if (a_el.GetMember("apPreview", &m) && m.IsBool() && m.GetBool()) {
                return true;  // our marker, directly on the entry
            }
            if (RE::GFxValue data; a_el.GetMember("data", &data) && data.IsObject()) {
                if (data.GetMember("apPreview", &m) && m.IsBool() && m.GetBool()) {
                    return true;  // our marker on the component's stored entry
                }
                if (data.GetMember("text", &m) && m.IsString() &&
                    cfg.previewButtonText == m.GetString()) {
                    return true;
                }
            }
            return a_el.GetMember("text", &m) && m.IsString() &&
                   cfg.previewButtonText == m.GetString();
        };
        std::vector<std::uint32_t> found;
        RE::GFxValue               firstEl;
        if (haveArray) {
            const std::uint32_t n = buttons.GetArraySize();
            for (std::uint32_t i = 0; i < n; ++i) {
                RE::GFxValue el;
                if (buttons.GetElement(i, &el) && matchesOurs(el)) {
                    if (found.empty()) {
                        firstEl = el;
                    }
                    found.push_back(i);
                }
            }
        }

        // ONE-SHOT STRUCTURE PROBE (once per session): dump where the label and
        // our marker actually land on each entry, so a field log confirms the
        // match above finds our button. Field showed buttons[i].text is not a
        // string, so on this UI the label lives under `.data` (Button components).
        {
            static bool s_probed = false;
            if (!s_probed && haveArray) {
                s_probed = true;
                const std::uint32_t n = buttons.GetArraySize();
                spdlog::info("navprobe: arraySize={}", n);
                for (std::uint32_t i = 0; i < n && i < 8; ++i) {
                    RE::GFxValue el;
                    if (!buttons.GetElement(i, &el) || !el.IsObject()) {
                        spdlog::info("navprobe:   buttons[{}] = <not object>", i);
                        continue;
                    }
                    RE::GFxValue t, d, dt, tag, dtag;
                    const bool hasData = el.GetMember("data", &d) && d.IsObject();
                    spdlog::info("navprobe:   buttons[{}] text={} tag={} data={} data.text='{}' data.tag={}",
                                 i, el.GetMember("text", &t) ? (t.IsString() ? "str" : "obj") : "-",
                                 (el.GetMember("apPreview", &tag) && tag.IsBool()) ? "yes" : "-",
                                 hasData ? "obj" : "-",
                                 (hasData && d.GetMember("text", &dt) && dt.IsString()) ? dt.GetString() : "-",
                                 (hasData && d.GetMember("apPreview", &dtag) && dtag.IsBool()) ? "yes" : "-");
                }
            }
        }

        const auto spliceAt = [&](std::uint32_t a_idx) {
            RE::GFxValue args[2]{ RE::GFxValue{ static_cast<double>(a_idx) }, RE::GFxValue{ 1.0 } };
            buttons.Invoke("splice", nullptr, args, 2);
        };

        bool changed = false;
        // Remove any duplicates beyond the first, highest index first so the
        // lower indices (including found[0]) stay valid as we splice.
        for (std::size_t k = found.size(); k > 1; --k) {
            spliceAt(found[k - 1]);
            changed = true;
        }
        const bool present = !found.empty();

        if (canPreview) {
            std::uint32_t keyCode = InputListener::LastInputWasGamepad()
                                        ? InputListener::ResolvedPadScaleformKey()
                                        : 0;
            if (keyCode == 0) {
                keyCode = InputListener::KeyboardKey();
            }
            if (present) {
                // Keep the single button; sync its key glyph to the device.
                if (RE::GFxValue controls;
                    firstEl.GetMember("controls", &controls) && controls.IsObject()) {
                    controls.SetMember("keyCode", RE::GFxValue{ static_cast<double>(keyCode) });
                    changed = true;
                }
            } else {
                // SkyUI ItemCard nav-panel API: the same call Compare Equipment
                // NG makes from ActionScript, addButton({text, controls:{keyCode}}).
                RE::GFxValue arg;
                view->CreateObject(&arg);
                arg.SetMember("text", RE::GFxValue{ cfg.previewButtonText.c_str() });
                arg.SetMember("apPreview", RE::GFxValue{ true });  // our marker, for dedup
                RE::GFxValue controls;
                view->CreateObject(&controls);
                controls.SetMember("keyCode", RE::GFxValue{ static_cast<double>(keyCode) });
                arg.SetMember("controls", controls);
                navPanel.Invoke("addButton", nullptr, &arg, 1);
                changed = true;
                // DIAGNOSTIC (non-destructive): addButton did NOT grow
                // `navPanel.buttons` (field: size stayed 6 after add), so our
                // Preview button lives in a DIFFERENT container than the one the
                // dedup scans - which is the whole reason found=0 and it kept
                // re-adding. Dump navPanel's own members + the sizes of the
                // candidate arrays so the next log names the real container.
                // (The earlier stamp-on-buttons[last] tagged a VANILLA button and
                // hid Preview entirely - reverted; this only logs.)
                {
                    static bool s_probed2 = false;
                    if (!s_probed2) {
                        s_probed2 = true;
                        for (const char* nm : { "buttons", "_buttonData", "buttonData",
                                                "entryList", "_entryList", "list", "_list",
                                                "buttonCount", "_buttonCount", "numButtons",
                                                "_buttons", "buttonHolder", "entries" }) {
                            RE::GFxValue v;
                            if (navPanel.GetMember(nm, &v)) {
                                if (v.IsArray()) {
                                    spdlog::info("navprobe2:   navPanel.{} = array size {}", nm,
                                                 v.GetArraySize());
                                } else if (v.IsNumber()) {
                                    spdlog::info("navprobe2:   navPanel.{} = number {}", nm,
                                                 v.GetNumber());
                                } else if (v.IsObject()) {
                                    spdlog::info("navprobe2:   navPanel.{} = object", nm);
                                } else {
                                    spdlog::info("navprobe2:   navPanel.{} = (present)", nm);
                                }
                            }
                        }
                    }
                }
            }
        } else if (present && haveArray) {
            // Not previewable: splice our remaining button out so the prompt
            // only shows where it applies.
            spliceAt(found[0]);
            changed = true;
        }

        if (changed) {
            RE::GFxValue refresh{ true };
            navPanel.Invoke("updateButtons", nullptr, &refresh, 1);
        }
        spdlog::debug("navbtn: canPreview={} haveArray={} found={} changed={}", canPreview,
                      haveArray, found.size(), changed);
    }

}  // namespace AP::NavButton
