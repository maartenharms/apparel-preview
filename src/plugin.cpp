#include "PCH.h"

#include "BipedHooks.h"
#include "DIIIIntegration.h"
#include "HoverTracker.h"
#include "InputListener.h"
#include "MenuWatcher.h"
#include "PreviewSession.h"
#include "Settings.h"
#include "VersionCheck.h"
#include "SettingsUI.h"

namespace {
    constexpr auto kLogName = "ApparelPreview.log";
    constexpr auto kVersion = "0.4.3";

    void SetupLog() {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }
        *path /= kLogName;

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto logger = std::make_shared<spdlog::logger>("global", std::move(sink));
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::debug);

        spdlog::set_default_logger(std::move(logger));
        spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    }

    void OnMessage(SKSE::MessagingInterface::Message* a_msg) {
        if (!a_msg) {
            return;
        }
        if (a_msg->type == SKSE::MessagingInterface::kPostLoad) {
            // All plugins are loaded now - cross-plugin listener registration
            // (sender-name resolution) only works from here on.
            AP::DIIIIntegration::Register();

            // Fitting Room (same studio) dispatches 'CLRP' when its transmog
            // editor opens: clear any hover preview so the editor starts from
            // the true look - the inventory is hidden under its modal editor,
            // so no un-hover event would ever fire.
            //
            // ⚠ REGISTERED UNDER BOTH NAMES, AND THE OLD ONE ALONE WAS A BUG.
            // The mod was renamed Outfit Slots -> Fitting Room and its SKSE
            // plugin name went with it, so this listener had been failing ever
            // since - "Failed to register messaging listener for OutfitSlots"
            // sat in the log and the preview simply never cleared when the
            // editor opened. Registering a name nobody publishes fails
            // harmlessly, so the old one stays for anyone still on a
            // pre-rename build.
            auto* const messaging = SKSE::GetMessagingInterface();
            const auto  onClear   = [](SKSE::MessagingInterface::Message* a_m) {
                if (a_m && a_m->type == 'CLRP') {
                    spdlog::info("Fitting Room editor opened - clearing preview.");
                    AP::PreviewSession::GetSingleton().Clear();
                }
            };
            if (!messaging->RegisterListener("FittingRoom", onClear)) {
                spdlog::info("Fitting Room not present - no editor-open preview clear. Normal "
                             "unless you run both mods.");
            }
            messaging->RegisterListener("OutfitSlots", onClear);  // pre-rename builds

            // Skyrim Outfit System (any lineage) tampers with the same biped
            // pipeline; running both would fight over the rebuild. Detect and
            // stay dormant rather than misbehave.
            const bool conflict =
                std::filesystem::exists("Data/SKSE/Plugins/SkyrimOutfitSystemSE.dll") ||
                std::filesystem::exists("Data/SKSE/Plugins/SkyrimOutfitEquipmentSystemNG.dll");
            if (conflict) {
                spdlog::error("Skyrim Outfit System detected - Apparel Preview shares its biped "
                              "pipeline and stays DISABLED this session.");
            } else {
                AP::BipedHooks::Install();
            }
        }
        if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
            AP::Settings::GetSingleton().Load();
            AP::MenuWatcher::Register();
            AP::InputListener::Register();
            AP::InputListener::ResolveBindings();
            AP::SettingsUI::Register();
        }
        // Backstop: a save loaded while a container/barter menu was open (which
        // unpaused-menu mods make possible) never delivers that menu's close
        // event. Drop all menu-scoped state so nothing outlives the session.
        if (a_msg->type == SKSE::MessagingInterface::kPreLoadGame) {
            AP::MenuWatcher::ForceReset("pre-load");
        }
        if (a_msg->type == SKSE::MessagingInterface::kNewGame) {
            AP::MenuWatcher::ForceReset("new game");
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SetupLog();
    spdlog::info("ApparelPreview v{} loading...", kVersion);

    // Universal SE + AE build: SE 1.5.97 and every AE from 1.6.317 up.
    //
    // ⚠ THE OLD GATE STOPPED AT 1.6.1130, AND THAT WAS THE BUG. Users on the
    // mid 1.6 builds got SKSE's "reported as incompatible during load" dialog,
    // which is this function returning false - our own message, never a crash.
    // Menu Studio and Fitting Room both refused the same way for the same
    // reason: the AE half of the address table had been verified against
    // exactly one binary, and "verified on one build" became "refuse every
    // other build".
    //
    // Every id used here resolves on every AE Address Library from 1.6.317 to
    // 1.6.1179 (Tools/re/allmods_callsites.py). What does NOT survive a
    // version change is the other kind of address - the hand-measured byte
    // offsets into functions - so those are hints now and VersionCheck locates
    // each site by what it CALLS. The gate refuses on a MEASUREMENT rather
    // than on a version number.
    const auto ver = a_skse->RuntimeVersion();
    if (ver != SKSE::RUNTIME_SSE_1_5_97 && ver < REL::Version(1, 6, 317, 0)) {
        spdlog::error("Unsupported Skyrim runtime {} - Apparel Preview supports SE 1.5.97 "
                      "and AE 1.6.317+; not loading.", ver.string());
        return false;
    }

    AP::VersionCheck::Run();
    if (!AP::VersionCheck::CriticalOk()) {
        spdlog::error("Address self-check FAILED on runtime {} - the biped hooks have nowhere "
                      "to install, so Apparel Preview would load and preview nothing. Not "
                      "loading. Please send ApparelPreview.log to the author.", ver.string());
        return false;
    }

    SKSE::Init(a_skse);
    SKSE::AllocTrampoline(256);

    AP::HoverTracker::Install();
    AP::InputListener::InstallPOVGate();
    // BipedHooks install happens at kPostLoad, after the Outfit System scan.

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnMessage)) {
        spdlog::error("Failed to register SKSE messaging listener; aborting load.");
        return false;
    }

    spdlog::info("ApparelPreview loaded.");
    return true;
}
