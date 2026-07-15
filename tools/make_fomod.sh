#!/usr/bin/env bash
# Build the Apparel Preview FOMOD installer zip (the release artifact).
#
# Apparel Preview is one DLL that loads on both SE 1.5.97 and AE 1.6.1130+, so
# there is no SE/AE file choice and no options; everything installs together and
# the FOMOD is a branded page with a short explanation and an endorse reminder.
# For a SINGLE-runtime mod that needs different SE and AE DLLs, add a step with a
# type="SelectExactlyOne" group ("Skyrim SE 1.5.97" / "Skyrim AE 1.6.x"), each
# plugin installing its own DLL folder; the rest of this layout is reusable.
#
# Package layout (zip root): fomod/{info.xml,ModuleConfig.xml}, Images/, core/
# (game files, installed to Data), + LICENSE and a short README.txt at the root
# (NOT installed). Docs single-source-of-truth is GitHub; the download carries
# only LICENSE (GPL) + a README.txt pointer, no CHANGELOG/KNOWN-ISSUES/THIRD-PARTY.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="$(sed -n 's/^project(ApparelPreview VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
DLL="$ROOT/build/release/ApparelPreview.dll"
STAGE="$ROOT/release/fomod-stage"
ZIP="$ROOT/release/ApparelPreview-$VER.zip"

[ -f "$DLL" ] || { echo "no DLL at $DLL - build first"; exit 1; }

rm -rf "$STAGE" "$ZIP"

# FOMOD metadata + banner (ModuleConfig references Images\apparelpreview.png).
mkdir -p "$STAGE/fomod" "$STAGE/Images"
cp "$ROOT/fomod/info.xml" "$ROOT/fomod/ModuleConfig.xml" "$STAGE/fomod/"
cp "$ROOT/fomod/banner.png" "$STAGE/Images/apparelpreview.png"

# core: the game files that install to Data (DLL + the eye-icon SWF + the DIII
# rule, harmless without DIII). No docs in core - see below.
mkdir -p "$STAGE/core/SKSE/Plugins"
cp "$DLL" "$STAGE/core/SKSE/Plugins/"
cp -r "$ROOT/dist/SKSE/Plugins/DIII" "$STAGE/core/SKSE/Plugins/DIII"
cp -r "$ROOT/dist/Interface" "$STAGE/core/Interface"
# Docs at the ARCHIVE ROOT (not installed to Data). Single-source-of-truth is
# GitHub: ship LICENSE (GPL requires it in the download) + a short README.txt
# pointer only. Full README/CHANGELOG/KNOWN-ISSUES/THIRD-PARTY live on GitHub.
cp "$ROOT/LICENSE" "$STAGE/"
cat > "$STAGE/README.txt" <<'EOF'
Apparel Preview
See armor, clothing and jewelry on your character before you equip it,
right from the container, barter and inventory menus.

Documentation, changelog, source code and issue tracker:
  https://github.com/maartenharms/apparel-preview

Licensed under GPL-3.0 (see LICENSE).
EOF

(cd "$STAGE" && powershell -NoProfile -Command \
    "Compress-Archive -Path * -DestinationPath '$(cygpath -w "$ZIP")' -Force")

echo "packaged FOMOD: $ZIP"
unzip -l "$ZIP"
