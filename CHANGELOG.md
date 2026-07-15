# Changelog

## 0.4.0 (2026-07-15)

- **Anniversary Edition support.** A single build now loads on both Skyrim SE 1.5.97 and AE 1.6.1130+. Every engine hook resolves through the Address Library and RELOCATION_ID; a site whose bytes do not match self-disables rather than crashing.

## 0.3.0 (2026-07-14)

- **Native "Preview" button** on the item card, next to the game's own prompts. Device-aware (keyboard key or controller glyph), and shown only on apparel you can actually preview.
- **Settings on FLICK.** The config panel moved to the FLICK in-game UI framework (INI still works without it).
- Gold eye icon on previewed rows via Dynamic Inventory Icon Injector, with icon art by Nithog.
- Pieces that cannot fit your body (no armature for your race, e.g. gear without a UBE patch on a UBE character) now show a "Cannot preview" notification instead of silently doing nothing. The preview was always declined for these; now you know why. Toggle with `bNotifyUnfitPreview` / "Notify when a piece cannot fit your body" (default ON).
- Helmet Toggle 2 compatibility: previewing headwear while your worn headgear is toggled hidden used to render a bald character (Helmet Toggle's armor-variant swap also swallows the previewed piece). Such previews are now declined with a "hidden by Helmet Toggle" notification; headwear in other slots (e.g. a circlet while a hood is hidden) still previews. Unhide first to preview normally. No effect without Helmet Toggle 2 installed.
- Preview now also works in your own inventory (default ON; `bEnableInventoryMenu` / "Preview in inventory" in the settings panel). Hover a piece you own, press the preview key, see it on yourself without equipping.
- Equipping a previewed item ends its preview; the real equip takes over the slot.
- Previewing a piece you are currently wearing is rejected (it would show nothing new); this also covers worn gear listed in barter's sell tab.

## 0.1.0

Initial release.

- Preview armor, clothing and jewelry on your character in the container and barter menus: hover the item, press the Item Zoom key (C by default), press again to remove. Nothing is ever equipped; closing the menu restores your normal look.
- Purely visual by construction: no equip events, no sounds, no inventory or armor-rating changes, nothing persisted to saves.
- Multi-piece previews: stack non-overlapping pieces; a new piece replaces whatever it overlaps by body slot. Previewed pieces replace the overlapping worn gear, and hair hiding follows the previewed piece like real equipping.
- Preview key follows your actual Item Zoom binding for keyboard and gamepad, with INI overrides and an in-panel rebind.
- Previewed rows are marked with a gold eye icon via Dynamic Inventory Icon Injector. Rows sharing a base item but differing in enchantment, temper, ownership or custom name count as separate items, so only the toggled row is marked.
- Master enable in the settings panel is a live kill switch: turning it off clears any active preview and disarms the mod immediately.
- On apparel, the key toggles the preview instead of the HUD-hiding inspect mode; non-apparel items keep vanilla zoom. The item-card hover model is preserved, stays hidden while that row's piece is previewed, and is restored on toggle-off.
- Unpaused-menu setups (Skyrim Souls RE) are supported; camera POV switching is blocked while the container/barter menu is open so the gamepad preview toggle (right-stick click) cannot flip the camera.
- Buying or taking a previewed item drops it from the preview automatically.
- Settings panel via SKSE Menu Framework (optional): per-menu enables, shield toggle, marker and item-card options, key rebind, clear-preview button; everything persists to `ApparelPreview.ini`, and the mod is fully functional INI-only.
- Safety guards: Skyrim SE 1.5.97 only; auto-disables for the session when Skyrim Outfit System is detected; every hook site is byte-checked before patching and fails safe.
