#!/usr/bin/env bash
# Stage + zip the Apparel Preview release. Layout mirrors the repo's dist/ tree:
# flat LICENSE/README/CHANGELOG/THIRD-PARTY-NOTICES at the archive root +
# SKSE/Plugins/ApparelPreview.dll + the optional Dynamic Inventory Icon Injector
# integration (SKSE/Plugins/DIII/zzz_ApparelPreview.json + the icon at
# Interface/ApparelPreview/previewicon.swf). No INI (settings live in FLICK /
# code defaults). No PDB. One build covers SE 1.5.97 and AE 1.6.1130+.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="$(sed -n 's/^project(ApparelPreview VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
DLL="$ROOT/build/release/ApparelPreview.dll"
STAGE="$ROOT/release/stage"
ZIP="$ROOT/release/ApparelPreviewSE-$VER.zip"

[ -f "$DLL" ] || { echo "no DLL at $DLL - build first"; exit 1; }

rm -rf "$STAGE" "$ZIP"
mkdir -p "$STAGE/SKSE/Plugins"

cp "$DLL" "$STAGE/SKSE/Plugins/"
# DIII icon config + Nithog's icon SWF. dist/ already mirrors the mod-folder tree.
cp -r "$ROOT/dist/SKSE/Plugins/DIII" "$STAGE/SKSE/Plugins/DIII"
cp -r "$ROOT/dist/Interface"         "$STAGE/Interface"
cp "$ROOT/LICENSE" "$ROOT/README.md" "$ROOT/CHANGELOG.md" \
   "$ROOT/THIRD-PARTY-NOTICES.md" "$ROOT/KNOWN-ISSUES.md" "$STAGE/"

(cd "$STAGE" && powershell -NoProfile -Command \
    "Compress-Archive -Path * -DestinationPath '$(cygpath -w "$ZIP")' -Force")

echo "packaged: $ZIP"
unzip -l "$ZIP"
