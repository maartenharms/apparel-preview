#pragma once

namespace AP::NavButton {
    // Refreshes the native "Preview" prompt in SkyUI's item-card nav panel for
    // the current hover: adds/updates it (device-aware key glyph) when the item
    // is previewable apparel, and removes it otherwise. Called (deferred to a UI
    // task) from the hover hook so it lands after SkyUI rebuilds the bottom bar.
    // No-op on a non-SkyUI UI (no nav panel) or while disarmed.
    void Refresh();
}
