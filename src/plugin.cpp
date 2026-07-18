#include "PCH.h"

#include "BipedHooks.h"
#include "DIIIIntegration.h"
#include "HoverTracker.h"
#include "InputListener.h"
#include "MenuWatcher.h"
#include "PreviewSession.h"
#include "Settings.h"
#include "SettingsUI.h"

namespace {
    constexpr auto kLogName = "ApparelPreview.log";
    constexpr auto kVersion = "0.4.2";

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

            // Outfit Slots (same studio) dispatches 'CLRP' when its transmog
            // editor opens: clear any hover preview so the editor starts from
            // the true look - the inventory is hidden under its modal editor,
            // so no un-hover event would ever fire.
            SKSE::GetMessagingInterface()->RegisterListener(
                "OutfitSlots", [](SKSE::MessagingInterface::Message* a_m) {
                    if (a_m && a_m->type == 'CLRP') {
                        spdlog::info("Outfit Slots editor opened - clearing preview.");
                        AP::PreviewSession::GetSingleton().Clear();
                    }
                });

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

    // Supported runtimes: SE 1.5.97 and AE next-gen (1.6.1130+). Every engine
    // address resolves via RELOCATION_ID(se, ae) with the AE values verified
    // against the 1.6.1170 binary; the four mid-function write_call sites also
    // guard on a 0xE8 byte, so a wrong offset on an unverified AE build
    // self-disables that one hook rather than crashing. VR (1.4.x) is excluded -
    // its offsets were never verified.
    const auto ver = a_skse->RuntimeVersion();
    const bool supported = (ver == SKSE::RUNTIME_SSE_1_5_97) ||
                           (ver >= REL::Version(1, 6, 1130, 0));
    if (!supported) {
        spdlog::error("Unsupported Skyrim runtime {} - Apparel Preview supports SE 1.5.97 "
                      "and AE 1.6.1130+ only; not loading.", ver.string());
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
