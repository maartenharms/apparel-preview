# Third-party notices

Apparel Preview is licensed GPL-3.0 (see [LICENSE](LICENSE)). It builds on the following third-party components; each remains under its own license.

## Vendored headers (in `extern/`)

| File | Origin | License |
|---|---|---|
| `DIII_API.h` | [JerryYOJ/Dynamic-Inventory-Icon-Injector-SKSE](https://github.com/JerryYOJ/Dynamic-Inventory-Icon-Injector-SKSE), `api/DIII_API.h` | GPL-3.0 |
| `FUCK_API.h` | [FLICK](https://www.nexusmods.com/skyrimspecialedition/mods/181603) by Fuzzles | GPL-3.0 (project license; the header itself carries no separate notice) |

Both headers declare a C-ABI that binds at runtime: `DIII_API.h` through Dynamic Inventory Icon Injector's plugin API, and `FUCK_API.h` against `FUCK.dll` via `GetModuleHandle`/`RequestFUCK`. No third-party object code is linked into this plugin. `FUCK_API.h` references [Dear ImGui](https://github.com/ocornut/imgui) types (MIT), pulled in as a build dependency below.

## Build dependencies (via vcpkg)

| Component | License |
|---|---|
| [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG) | MIT |
| [xbyak](https://github.com/herumi/xbyak) | BSD-3-Clause |
| [simpleini](https://github.com/brofield/simpleini) | MIT |
| [jsoncpp](https://github.com/open-source-parsers/jsoncpp) | Public Domain / MIT (dual) |
| [Dear ImGui](https://github.com/ocornut/imgui) | MIT |
| [fmt](https://github.com/fmtlib/fmt) | MIT, with binary-embedding exception |
| [spdlog](https://github.com/gabime/spdlog) | MIT |

## Techniques (documentation, no code copied)

The biped-override technique and its SE hook sites are documented from Skyrim Outfit System (DavidJCobb, aers, MetricExpansion); the hover hook pattern from Model-Swapper (Thiago099); the paused-menu refresh recipe from Immersive Equipment Displays (SlavicPotato). All were reimplemented from published documentation and public reverse-engineering facts; no source code from those projects is included.
