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
        bool CanPreview(RE::InventoryEntryData* a_entry, RE::TESObjectARMO* a_armo) {
            if (!a_armo) {
                return false;
            }
            // Already wearing it (user, 2026-07-18: "if we have gear equipped
            // the prompt doesn't show because well, we have that piece of gear
            // already equipped"). Previewing worn gear would render exactly what
            // is on screen already, so the prompt is pure noise on every armor
            // row the player has equipped. IsWorn reads the entry's own extra
            // lists, so it follows the hovered STACK rather than the form -
            // a second unworn copy of the same armor still offers a preview.
            if (a_entry && a_entry->IsWorn()) {
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
        const bool canPreview = obj && CanPreview(entry, obj->As<RE::TESObjectARMO>());

        // ---- IDEMPOTENCY --------------------------------------------------
        // Field 2026-07-18: "duplicated or even triple preview prompts when I
        // drop an item or unequip gear like a helmet and reequip it without
        // moving which item is highlighted."
        //
        // The dedup scan below is DEAD and cannot be repaired where it stands.
        // It searches `navPanel.buttons`, which on this UI is SkyUI's fixed
        // POOL of 6 stage clips, not a data array. `addButton` consumes our
        // {text, apPreview, controls} object into a pooled clip's internals, so
        // the marker never lands anywhere the scan can read: the field log has
        // found=0 on 255 of 255 samples, i.e. EVERY refresh re-added blindly.
        //
        // The highlight detail in the report is the whole mechanism. SkyUI's
        // only reset primitive is clearButtons() (the panel exposes no
        // removeButton), and it runs on the SELECTION-CHANGE path. Move the
        // highlight and the panel clears before our deferred add lands, so one
        // prompt survives and the stray adds are absorbed. Mutate the inventory
        // on the SAME row and the item card is re-requested - our hook fires
        // again - while SkyUI never clears, so each re-request stacks another
        // prompt. It tops out at "triple" because the pool holds 6 and three
        // are already vanilla, which is why it is never quadruple.
        //
        // So the guard has to come from OUR state rather than from reading the
        // panel. Memoise the selection and re-add only when it genuinely
        // changed, or when the panel's own live counter proves a clear ran.
        std::uint32_t keyCode = InputListener::LastInputWasGamepad()
                                    ? InputListener::ResolvedPadScaleformKey()
                                    : 0;
        if (keyCode == 0) {
            keyCode = InputListener::KeyboardKey();
        }
        double liveCount = -1.0;
        if (RE::GFxValue bc; navPanel.GetMember("_buttonCount", &bc) && bc.IsNumber()) {
            liveCount = bc.GetNumber();
        }
        static const void*   s_lastView       = nullptr;
        static const void*   s_lastEntry      = nullptr;
        static bool          s_lastCanPreview = false;
        static std::uint32_t s_lastKeyCode    = 0;
        static double        s_countAfterAdd  = -1.0;
        static bool          s_added          = false;
        if (view != s_lastView) {
            s_lastView = view;  // new movie: nothing we added survives it
            s_added    = false;
        }
        // The count test is deliberately an EXACT match, not >=. If SkyUI
        // cleared and rebuilt, ours is gone and the count differs, so we re-add.
        // Erring this way costs at worst a duplicate (cosmetic, and the old
        // behaviour) whereas erring the other way loses the prompt entirely.
        if (s_added && entry == s_lastEntry && canPreview == s_lastCanPreview &&
            keyCode == s_lastKeyCode && liveCount >= 0.0 && liveCount == s_countAfterAdd) {
            return;  // same row, same glyph, panel untouched since our add
        }

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

        // ⚠ LANDMINE. This splices `navPanel.buttons`, which on this UI is
        // SkyUI's FIXED POOL of 6 stage clips - not a list of our own entries.
        // It is harmless today only because the scan above can never match, so
        // `found` is always empty and this never runs. If anyone ever "fixes"
        // the scan so it does match, this will start permanently shrinking the
        // pool (6 -> 5 -> 4 ...) and break the bottom bar for the rest of the
        // menu session. Removal must NOT go through here: prompt removal on a
        // non-previewable row already works, by way of SkyUI's own clear.
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

        s_added = false;  // any path that does not add must leave this false
        if (canPreview) {
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
                // Remember exactly what we just added against, including the
                // panel's live counter READ BACK after the add - that read-back
                // is the reference the next refresh compares to, and it is the
                // only thing that can tell us a clearButtons() happened.
                s_lastEntry      = entry;
                s_lastCanPreview = canPreview;
                s_lastKeyCode    = keyCode;
                s_added          = true;
                s_countAfterAdd  = -1.0;
                if (RE::GFxValue bc2;
                    navPanel.GetMember("_buttonCount", &bc2) && bc2.IsNumber()) {
                    s_countAfterAdd = bc2.GetNumber();
                }
                spdlog::debug("navbtn: added (entry={} keyCode={} countAfterAdd={:.0f})",
                              static_cast<const void*>(entry), keyCode, s_countAfterAdd);
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
