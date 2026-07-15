# Apparel Preview

Preview armor, clothing and jewelry on your own character while looting, shopping or browsing your inventory, without equipping anything. Hover an apparel item in the container, barter or inventory menu, press the Item Zoom key (C by default, controller works too), and the piece appears on you; press again and it is gone. Close the menu and your normal look is back.

SKSE plugin for Skyrim Special Edition **1.5.97** and Anniversary Edition **1.6.1130+**. One build covers both.

## Features

- Works in the container menu (looting), the barter menu (shopping, both panes) and your own inventory; steal-mode containers included, since nothing is actually taken.
- A native "Preview" button on the item card, right next to the game's own prompts. Device aware: it shows the keyboard key or the controller glyph, and only appears on apparel you can actually preview.
- Purely visual. Nothing is equipped: no equip sounds, no inventory changes, no armor rating changes, nothing written to your save.
- A previewed piece replaces the overlapping worn gear on your body exactly like wearing it would, and a previewed helmet hides your hair. Stack pieces that occupy different slots; previewing something that overlaps an earlier preview swaps it out.
- Previewed rows are marked in the item list with a gold eye icon via Dynamic Inventory Icon Injector (optional). Rows that share a base item but differ in enchantment, temper, ownership or custom name are tracked separately, so only the row you toggled is marked.
- Equipping a previewed piece ends its preview (the real equip takes the slot). Previewing something you already wear is a rejected no-op. Buying or taking a previewed item drops it from the preview; you own it now, so wear it for real.
- Pieces with no armature for your body (for example gear without a UBE patch on a UBE character) are declined with a "Cannot preview" notice instead of silently doing nothing.
- The Item Zoom key keeps its vanilla behavior on non-apparel items. On apparel it toggles the preview instead of the HUD-hiding inspect view (configurable). While a piece is previewed its floating item-card model stays hidden so it never blocks the view, and it returns when you toggle off.

## Requirements

- Skyrim SE **1.5.97** or AE **1.6.1130+**. VR and older runtimes are refused.
- [SKSE64](https://skse.silverlock.org/) for your runtime
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (the SE or AE database to match)
- [Show Player In Inventory](https://www.nexusmods.com/skyrimspecialedition/mods/178689) So you can actually see your character.
- Optional: [FLICK](https://www.nexusmods.com/skyrimspecialedition/mods/181603) for the in-game settings panel; the mod is fully configurable through the INI without it
- Optional: [Dynamic Inventory Icon Injector](https://www.nexusmods.com/skyrimspecialedition/mods/174136) for the gold eye icon on previewed rows

## Settings

With FLICK installed, open its panel and pick **Apparel Preview**; changes save immediately. Without it, edit `Data/SKSE/Plugins/ApparelPreview.ini` (created on first run; under MO2 it lands in the overwrite folder). Every feature has a switch: per-menu enables (container, barter, inventory), shield previews, the row markers, the item-card model handling, the "Cannot preview" notice, POV-switch blocking for unpaused-menu setups, and a key rebind. Key changes take effect on the next game load.

## Compatibility

- **Show Player In Menus / Show Player In Inventory**: the intended companions; no shared hooks.
- **Menu Studio**: built to pair with it (pause the world in menus while your character stays live and posed).
- **Fitting Room** (transmog): coexists; when its editor opens, any active preview is cleared so the editor starts from your true look.
- **Helmet Toggle 2**: previewing headwear while your worn headgear is toggled hidden is declined with a notice, because Helmet Toggle's variant swap would otherwise swallow the preview and show a bald head. Unhide first to preview normally. No effect without it installed.
- **Skyrim Outfit System** (SE Revived and the NG fork): uses the same engine hook sites. Apparel Preview detects it at startup and disables itself for the session; the log says so.
- **Skyrim Souls RE** (unpaused menus): supported; POV switching is blocked while the container or barter menu is open so the gamepad preview toggle (right-stick click) cannot flip the camera.
- **QuickLoot EE/IE**: no conflict. The QuickLoot widget itself is not supported yet; press R for the full container view and preview from there.
- **SkyUI and UI overhauls built on it**: fine. The icon-tint marker fallback needs a SkyUI-family list; the name-suffix marker works on any UI.
- Every hook site is byte-verified before patching. If another mod got there first, the affected feature disables itself and the mod stays inert rather than fighting over the site.

## Known limitations

- First person without a "show player in menus" mod: the preview is applied to your third-person body, so you will not see it until the camera does.
- List markers can drop on engine-driven list rebuilds (item transfers) until you toggle again.
- Enchantment glows are not rendered on previewed pieces.
- Weapons are out of scope; this is about apparel.
- A duplicate "Preview" button can appear after the item list rebuilds; see [KNOWN-ISSUES.md](KNOWN-ISSUES.md).

## How it works

The mod wraps the engine's own worn-armor skinning pass for the player and injects the previewed pieces after the vanilla gear, using the same per-slot replacement rules the game itself uses (which is why hair hiding and slot swaps behave exactly like real equipping). A worn-mask shim makes the engine treat previewed slots as occupied during the rebuild. The menu pause is bypassed for the rebuild only, through the engine's own refresh path. No inventory, equip state or save data is ever touched; when the menu closes, one regular refresh restores reality. Every patch site is resolved through the Address Library and byte-checked before installation, so a mismatch means a disabled feature rather than a crash. On AE the same sites resolve through RELOCATION_ID and the mid-function hooks self-disable if a byte does not match, so an unverified build fails safe.

## Credits

- **Skyrim Outfit System** by DavidJCobb, with the SE ports by aers and MetricExpansion: the "worn is not shown" biped-override technique and the documented SE hook sites. Reimplemented from their published documentation; no code copied.
- **Model-Swapper** by Thiago099: the item-card hover hook pattern. Reimplemented.
- **Immersive Equipment Displays** by SlavicPotato: the paused-menu scene refresh recipe. Reimplemented.
- **Dynamic Inventory Icon Injector** by JerryYOJ: the eye icon integrates through DIII's plugin API.
- **FLICK** by Fuzzles: the in-game settings panel.
- **Show Player In Menus** and **Show Player In Inventory** by derickso and ItzIvy05: the companion mods that make all of this visible.
- **Nithog**: the preview eye icon art.
- The SKSE team, the CommonLibSSE-NG maintainers, and the Address Library project, without which none of this exists.

## Building

CMake + vcpkg (`commonlibsse-ng`, `xbyak`, `simpleini`, `jsoncpp`, `imgui`). Run `tools/build.bat` from PowerShell; it configures, builds, runs the unit tests and deploys. The release FOMOD installer is assembled by `tools/make_fomod.sh` (metadata and installer logic live in `fomod/`).

## License

GPL-3.0; see [LICENSE](LICENSE). The vendored `extern/DIII_API.h` (Dynamic Inventory Icon Injector) and `extern/FUCK_API.h` (FLICK) are both GPL-3.0. Attributions and the licenses of all build dependencies are listed in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
