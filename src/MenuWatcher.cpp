#include "MenuWatcher.h"

#include "BipedHooks.h"
#include "HoverTracker.h"
#include "PreviewSession.h"
#include "Settings.h"

namespace AP {

    MenuWatcher& MenuWatcher::GetSingleton() {
        static MenuWatcher instance;
        return instance;
    }

    void MenuWatcher::Register() {
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink(&GetSingleton());
            spdlog::info("MenuWatcher registered.");
        } else {
            spdlog::error("MenuWatcher: RE::UI singleton unavailable.");
        }
    }

    bool MenuWatcher::IsArmed() { return GetSingleton().armed_.load(); }

    bool MenuWatcher::IsArmedAndMenuOpen() {
        if (!GetSingleton().armed_.load()) {
            return false;
        }
        auto* ui = RE::UI::GetSingleton();
        return ui && (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) ||
                      ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) ||
                      ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME));
    }

    void MenuWatcher::ForceReset(const char* a_reason) {
        const bool wasArmed = GetSingleton().armed_.exchange(false);
        HoverTracker::ClearHovered();
        PreviewSession::GetSingleton().Clear();
        if (wasArmed) {
            spdlog::warn("MenuWatcher: force reset while armed ({}) - a menu close was missed.",
                         a_reason);
        }
    }

    RE::BSEventNotifyControl MenuWatcher::ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
                                                       RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }
        const bool container = a_event->menuName == RE::ContainerMenu::MENU_NAME;
        const bool barter    = a_event->menuName == RE::BarterMenu::MENU_NAME;
        const bool inventory = a_event->menuName == RE::InventoryMenu::MENU_NAME;
        if (!container && !barter && !inventory) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (a_event->opening) {
            const auto& cfg = Settings::GetSingleton();
            const bool  menuEnabled =
                container ? cfg.enableContainer : (barter ? cfg.enableBarter : cfg.enableInventory);
            if (!cfg.enabled || !menuEnabled ||
                !BipedHooks::AllInstalled()) {  // dormant on hook failure / SOS conflict
                return RE::BSEventNotifyControl::kContinue;
            }
            armed_ = true;
            spdlog::debug("MenuWatcher: {} opened -> armed", a_event->menuName.c_str());
        } else {
            armed_ = false;
            HoverTracker::ClearHovered();
            PreviewSession::GetSingleton().Clear();  // master revert on EVERY close path
            spdlog::debug("MenuWatcher: {} closed -> cleared + disarmed", a_event->menuName.c_str());
        }
        return RE::BSEventNotifyControl::kContinue;
    }

}  // namespace AP
