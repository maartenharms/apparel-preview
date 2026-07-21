#pragma once

// Startup address self-check - the whole diagnosis for a runtime we do not
// have. See VersionCheck.cpp for why every part of it exists.
//
// ⚠ THIRD COPY OF THIS COMPONENT (Menu Studio, Fitting Room, here). Fitting
// Room's is the reference - it has the anchor and ordinal cases. Extract the
// common part to one vendored header once all three are field-verified; until
// then a fix here almost certainly belongs in the other two.
namespace AP::VersionCheck {
    // Run once from SKSEPluginLoad, BEFORE any hook installs.
    void Run();

    // TRUE when the worn-pass and worn-mask sites were located. They are what
    // Apparel Preview IS, so a false here means "do not load".
    bool CriticalOk();

    // Exact-membership test against the running build's Address Library.
    // ⚠ NOT the same question as "REL::Relocation gave me an address".
    // CommonLib's id2offset does a lower_bound and, on SE/AE, only fails when
    // the id is past the END of the database - an id that is simply ABSENT
    // resolves to its NEIGHBOUR's offset with no error at all.
    bool IdOk(std::uint64_t a_id);
    bool IdOk(const REL::RelocationID& a_id);

    // Located call sites, relative to their containing function. 0 = absent,
    // and callers MUST treat it as "do not install this hook".
    std::ptrdiff_t WornPassCallOffset();    // BipedHooks::InstallInjection
    std::ptrdiff_t WornMaskCallOffset();    // BipedHooks::InstallWornMaskShim
    std::ptrdiff_t ApplyAddonCallOffset();  // BipedHooks::InstallCapture (diagnostic)
    std::ptrdiff_t HoverTrackerCallOffset();// HoverTracker
}
