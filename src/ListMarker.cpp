#include "ListMarker.h"

#include "DIIIIntegration.h"
#include "MenuWatcher.h"
#include "PreviewSession.h"

namespace AP {

    namespace {
        constexpr const char* kIconLabel = "PreviewIcon";

        // Add/remove our icon in the entry's _DIIIIcons array - the exact
        // member DIII's formatName hook reads (verified in its source). This
        // avoids an engine list rebuild entirely: no stutter, no hover reset,
        // no re-stamping of every entry by every inventory mod.
        bool SyncDIIIIcon(RE::GFxMovieView* a_view, RE::GFxValue& a_entry, bool a_marked) {
            RE::GFxValue arr;
            const bool hasArr = a_entry.GetMember("_DIIIIcons", &arr) && arr.IsArray();

            std::int32_t found = -1;
            if (hasArr) {
                const auto n = arr.GetArraySize();
                for (std::uint32_t i = 0; i < n; ++i) {
                    RE::GFxValue elem, label;
                    if (arr.GetElement(i, &elem) && elem.IsObject() &&
                        elem.GetMember("label", &label) && label.IsString() &&
                        std::string_view{ label.GetString() } == kIconLabel) {
                        found = static_cast<std::int32_t>(i);
                        break;
                    }
                }
            }

            if (a_marked && found < 0) {
                if (!hasArr) {
                    a_view->CreateArray(&arr);
                }
                RE::GFxValue icon;
                a_view->CreateObject(&icon);
                icon.SetMember("label", RE::GFxValue{ kIconLabel });
                arr.PushBack(icon);
                a_entry.SetMember("_DIIIIcons", arr);
                return true;
            }
            if (!a_marked && found >= 0) {
                arr.RemoveElements(static_cast<std::uint32_t>(found), 1);
                a_entry.SetMember("_DIIIIcons", arr);
                return true;
            }
            return false;
        }
    }

    void ListMarker::Apply() {
        if (!MenuWatcher::IsArmed()) {
            return;
        }
        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            return;
        }
        RE::ItemList* list = nullptr;
        if (const auto container = ui->GetMenu<RE::ContainerMenu>()) {
            list = container->GetRuntimeData().itemList;
        } else if (const auto barter = ui->GetMenu<RE::BarterMenu>()) {
            list = barter->GetRuntimeData().itemList;
        } else if (const auto inventory = ui->GetMenu<RE::InventoryMenu>()) {
            list = inventory->GetRuntimeData().itemList;
        }
        if (list) {
            ApplyToList(list);
        }
    }

    void ListMarker::ApplyToList(RE::ItemList* a_list) {
        // The gold DIII eye icon is the only previewed-row marker. Without
        // Dynamic Inventory Icon Injector active there is nothing to draw (the
        // " [P]" suffix and gold-tint fallbacks were removed by design).
        if (!DIIIIntegration::IsActive()) {
            return;
        }
        auto* view = a_list->view.get();
        if (!view) {
            return;
        }
        auto& session = PreviewSession::GetSingleton();

        bool changed = false;
        for (auto* item : a_list->items) {
            if (!item) {
                continue;
            }
            auto* entry = item->data.objDesc;
            auto* obj   = entry ? entry->GetObject() : nullptr;
            if (!obj) {
                continue;
            }
            // Row-exact: several rows can share a base form (plain, enchanted,
            // tempered); only the row actually toggled gets the eye.
            const bool marked = session.IsPreviewedEntry(obj->GetFormID(),
                                                         PreviewSession::VariantKey(entry));
            changed |= SyncDIIIIcon(view, item->obj, marked);
        }

        if (changed) {
            a_list->root.Invoke("InvalidateData", nullptr, nullptr, 0);
            spdlog::debug("list markers synced (DIII icon)");
        }
    }

}  // namespace AP
