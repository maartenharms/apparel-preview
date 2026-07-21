#include "BipedHooks.h"

#include "MenuWatcher.h"
#include "PreviewSession.h"
#include "REAugments.h"
#include "VersionCheck.h"

#include <xbyak/xbyak.h>

namespace AP {

    namespace {
        std::atomic<std::uint64_t> g_playerWornPass{ 0 };

        // Per-pass context. The skinning pass runs synchronously on one thread,
        // so thread_local state set around the displaced call is safe.
        struct PassContext {
            bool                     playerPass{ false };
            REAug::ActorWeightModel* awmFromApply{ nullptr };
        };
        thread_local PassContext t_pass;

        // ---- 24232+0x302: vanilla per-item ApplyArmorAddon call ----
        // Gives us the pass's ActorWeightModel as a real C++ argument - the
        // authoritative value to inject with (no register archaeology).
        using ApplyAddon_t = bool (*)(RE::TESObjectARMO*, RE::TESRace*, REAug::ActorWeightModel*, bool);
        ApplyAddon_t g_origApplyAddon = nullptr;

        bool ApplyAddonThunk(RE::TESObjectARMO* a_armor, RE::TESRace* a_race,
                             REAug::ActorWeightModel* a_awm, bool a_isFemale) {
            if (t_pass.playerPass) {
                t_pass.awmFromApply = a_awm;
                if (MenuWatcher::IsArmed()) {
                    spdlog::debug("vanilla apply '{}' awm={}",
                                  a_armor ? a_armor->GetName() : "?",
                                  static_cast<const void*>(a_awm));
                }
            }
            return g_origApplyAddon(a_armor, a_race, a_awm, a_isFemale);
        }

        // ---- 24231+0x81: the worn-pass (ExecuteVisitorOnWorn) wrap ----
        using VisitWorn_t = void (*)(RE::InventoryChanges*, RE::InventoryChanges::IItemChangeVisitor&);
        VisitWorn_t g_origVisitWorn = nullptr;

        // Actor identification uses the displaced call's own argument:
        // InventoryChanges::owner. The SOS-era "rbx = Actor" register contract
        // does NOT hold at this site on 1.5.97 (observed rbx == 0x14 in-game),
        // so the stub-captured registers are kept for diagnostics/fallback only.
        void HandleWornPass(RE::InventoryChanges* a_changes,
                            RE::InventoryChanges::IItemChangeVisitor& a_visitor,
                            RE::Actor* a_rbxActor, REAug::ActorWeightModel* a_rdiAWM) {
            auto& session = PreviewSession::GetSingleton();
            auto* player  = RE::PlayerCharacter::GetSingleton();
            auto* owner   = a_changes ? a_changes->owner : nullptr;
            const bool forPlayer = player && owner == player;

            if (forPlayer) {
                g_playerWornPass.fetch_add(1, std::memory_order_relaxed);
            }
            if (MenuWatcher::IsArmed()) {
                spdlog::debug("wornpass owner={:08X} forPlayer={} rbx={} rdiAWM={} active={}",
                              owner ? owner->GetFormID() : 0, forPlayer,
                              static_cast<const void*>(a_rbxActor),
                              static_cast<const void*>(a_rdiAWM), session.IsActive());
            }

            t_pass = { forPlayer, nullptr };
            g_origVisitWorn(a_changes, a_visitor);  // vanilla worn skinning

            if (forPlayer && session.IsActive()) {
                // AWM: prefer the value captured from the vanilla apply call;
                // otherwise derive it register-free from the actor - the
                // address of the 3rd-person biped smart-pointer field is
                // byte-for-byte the holder shape ApplyArmorAddon consumes
                // (its per-ARMA callee only ever *reads* *holder; decompile-
                // verified on BOTH 1.5.97 and 1.6.1170, 2026-07-15). The old
                // rdi fallback is dead: at the AE site (24725+0x1EF) rdi does
                // not hold the AWM, so injecting through it would be garbage.
                auto* awm = t_pass.awmFromApply;
                if (!awm) {
                    awm = reinterpret_cast<REAug::ActorWeightModel*>(
                        const_cast<RE::BSTSmartPointer<RE::BipedAnim>*>(&player->GetBiped1(false)));
                }
                auto*      base     = player->GetActorBase();
                const bool isFemale = base && base->IsFemale();
                auto*      race     = player->GetRace();
                if (awm) {
                    session.VisitPreviewArmors([&](RE::TESObjectARMO* armo) {
                        const bool ok = REAug::ApplyArmorAddon(armo, race, awm, isFemale);
                        spdlog::debug("inject '{}' via {} awm={} -> {}", armo->GetName(),
                                      t_pass.awmFromApply ? "captured" : "actor",
                                      static_cast<const void*>(awm), ok);
                    });
                } else {
                    spdlog::warn("inject skipped: no AWM available this pass (naked actor?)");
                }
            }
            t_pass = {};
        }

        // ---- 24220 (SE +0x7C / AE +0x80): InventoryChanges::GetWornMask call ----
        // The mask drives skin generation and hair/head-part hiding. While a
        // preview is active the player's mask must also cover the previewed
        // slots (e.g. a previewed helmet hides hair). Union only - worn slots
        // stay set because overlapped worn 3D is replaced per-slot anyway.
        using GetWornMask_t = std::uint32_t (*)(RE::InventoryChanges*);
        GetWornMask_t g_origGetWornMask = nullptr;

        std::uint32_t GetWornMaskThunk(RE::InventoryChanges* a_changes) {
            const auto real    = g_origGetWornMask(a_changes);
            auto&      session = PreviewSession::GetSingleton();
            auto*      player  = RE::PlayerCharacter::GetSingleton();
            if (!session.IsActive() || !a_changes || !player || a_changes->owner != player) {
                return real;
            }
            const auto shimmed = real | session.PreviewMask();
            if (MenuWatcher::IsArmed()) {
                spdlog::debug("wornmask real={:08X} -> shimmed={:08X}", real, shimmed);
            }
            return shimmed;
        }

        // Stub: keep the displaced call's args (rcx, rdx), append rbx -> r8 and
        // rdi -> r9 (diagnostics/fallback), then tail-jump into HandleWornPass.
        struct WornPassStub : Xbyak::CodeGenerator {
            WornPassStub() {
                mov(r8, rbx);
                mov(r9, rdi);
                mov(rax, reinterpret_cast<std::uintptr_t>(&HandleWornPass));
                jmp(rax);
            }
        };
    }

    void BipedHooks::InstallInjection() {
        // SE: the worn skinning exec (15856) is called from the small wrapper
        // 24231 at +0x81. AE: the compiler INLINED that wrapper into the
        // rebuild parent 24725 - the old 24735 still exists in the binary but
        // has ZERO callers (whole-exe xref, 2026-07-15; Ivy's 1.6.1170 diag
        // log showed the hook installed yet "wornpass ran 0x"). The live AE
        // site is the inlined call to 16096 at 24725+0x1EF; same displaced-
        // call ABI (rcx = InventoryChanges*, rdx = visitor&). NOTE: rbx/rdi
        // at the AE site are NOT the SE-era register contract - the stub still
        // forwards them, but they are diagnostics-only (see HandleWornPass).
        // ⚠ LOCATED, NOT HARDCODED. On AE the parent calls the visit
        // function TWICE and only one is the worn pass; VersionCheck picks the
        // one handed the worn-visitor vtable. See VersionCheck.cpp.
        const auto callOffset = VersionCheck::WornPassCallOffset();
        if (callOffset == 0) {
            spdlog::error("BipedHooks: no worn-pass call site on this runtime - injection NOT "
                          "installed.");
            return;
        }
        const REL::Relocation<std::uintptr_t> site{ REL::RelocationID(24231, 24725), callOffset };
        if (*reinterpret_cast<std::uint8_t*>(site.address()) != 0xE8) {
            spdlog::error("BipedHooks: expected E8 at the worn-pass site (SE 24231+0x81 / AE 24725+0x1EF), found {:02X} - injection NOT installed.",
                          *reinterpret_cast<std::uint8_t*>(site.address()));
            return;
        }
        auto& trampoline = SKSE::GetTrampoline();

        WornPassStub stub;
        stub.ready();
        auto* stubMem = trampoline.allocate(stub.getSize());
        std::memcpy(stubMem, stub.getCode(), stub.getSize());

        g_origVisitWorn = reinterpret_cast<VisitWorn_t>(
            trampoline.write_call<5>(site.address(), reinterpret_cast<std::uintptr_t>(stubMem)));
        injectionOk_ = true;
        spdlog::info("BipedHooks: injection hook installed at SE 24231+0x81 / AE 24725+0x1EF.");
    }

    void BipedHooks::InstallCapture() {
        const auto callOffset = VersionCheck::ApplyAddonCallOffset();
        if (callOffset == 0) {
            spdlog::info("BipedHooks: no ApplyArmorAddon capture site on this runtime; the "
                         "capture is a diagnostic, so everything else works.");
            return;
        }
        const REL::Relocation<std::uintptr_t> site{ REL::RelocationID(24232, 24736), callOffset };
        if (*reinterpret_cast<std::uint8_t*>(site.address()) != 0xE8) {
            // Diagnostic-only hook. Vanilla has a `call ApplyArmorAddon` (E8)
            // here; another load-order plugin has patched it (0x90/NOP seen in
            // the field). It never fires during our menu rebuilds on 1.5.97
            // anyway, and injection takes its ActorWeightModel from the
            // worn-pass capture instead - so preview is unaffected. Warn, not
            // error: this is expected on some load orders, not a failure.
            spdlog::warn("BipedHooks: 24232+0x302 is {:02X}, not E8 - another plugin patched this "
                         "call; AWM capture skipped (diagnostic only, preview unaffected).",
                         *reinterpret_cast<std::uint8_t*>(site.address()));
            return;
        }
        g_origApplyAddon = reinterpret_cast<ApplyAddon_t>(
            SKSE::GetTrampoline().write_call<5>(site.address(), ApplyAddonThunk));
        captureOk_ = true;
        spdlog::info("BipedHooks: apply-capture hook installed at 24232+0x302.");
    }

    void BipedHooks::InstallWornMaskShim() {
        // Worn-mask GetWornMask call. On AE 1.6.1170 the call sits 4 bytes later
        // (+0x80) - the function itself matches SE 24220 1:1 (verified by
        // call-sequence alignment against the AE binary).
        const auto callOffset = VersionCheck::WornMaskCallOffset();
        if (callOffset == 0) {
            spdlog::error("BipedHooks: no worn-mask call site on this runtime - mask shim NOT "
                          "installed.");
            return;
        }
        const REL::Relocation<std::uintptr_t> site{ REL::RelocationID(24220, 24724), callOffset };
        if (*reinterpret_cast<std::uint8_t*>(site.address()) != 0xE8) {
            spdlog::error("BipedHooks: expected E8 at 24220 (SE +0x7C / AE +0x80), found {:02X} - mask shim NOT installed.",
                          *reinterpret_cast<std::uint8_t*>(site.address()));
            return;
        }
        g_origGetWornMask = reinterpret_cast<GetWornMask_t>(
            SKSE::GetTrampoline().write_call<5>(site.address(), GetWornMaskThunk));
        maskShimOk_ = true;
        spdlog::info("BipedHooks: worn-mask shim installed at 24220 (SE +0x7C / AE +0x80).");
    }

    void BipedHooks::Install() {
        InstallInjection();
        InstallCapture();
        InstallWornMaskShim();
    }

    bool BipedHooks::AllInstalled() { return injectionOk_ && maskShimOk_; }

    std::uint64_t BipedHooks::PlayerWornPassCount() {
        return g_playerWornPass.load(std::memory_order_relaxed);
    }

}  // namespace AP
