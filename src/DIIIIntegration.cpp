#include "DIIIIntegration.h"

#include "PreviewSession.h"

#include <DIII_API.h>

namespace AP {

    namespace {
        std::atomic<bool> g_diiiActive{ false };

        class PreviewedCondition : public DIII::ICondition {
        public:
            explicit PreviewedCondition(bool a_expected) : expected_(a_expected) {}

            bool Match(RE::InventoryEntryData* a_entry) const override {
                const auto* obj = a_entry ? a_entry->GetObject() : nullptr;
                if (!obj) {
                    return !expected_;
                }
                // Row-exact (form + variant): plain, enchanted and tempered
                // rows of the same base form must not all show the eye.
                return PreviewSession::GetSingleton().IsPreviewedEntry(
                           obj->GetFormID(), PreviewSession::VariantKey(a_entry)) == expected_;
            }

        private:
            bool expected_;
        };
    }

    void DIIIIntegration::Register() {
        // MUST run at kPostLoad: skse64 resolves the sender name against the
        // loaded-plugin list at registration time and FAILS SILENTLY for a
        // not-yet-loaded sender. "ApparelPreview.dll" loads before
        // "DynamicInventoryIconInjector.dll" alphabetically, so a load-time
        // registration lands in the void (verified against skse64 source +
        // in-game logs, 2026-07-10).
        const bool ok = SKSE::GetMessagingInterface()->RegisterListener(
            "DynamicInventoryIconInjector", [](SKSE::MessagingInterface::Message* a_msg) {
            // Boundary instrumentation: log ANY message from DIII so a type or
            // payload mismatch is visible instead of silent.
            spdlog::info("DIII message received: type=0x{:X} dataLen={} data={}",
                         a_msg ? a_msg->type : 0, a_msg ? a_msg->dataLen : 0,
                         static_cast<const void*>(a_msg ? a_msg->data : nullptr));
            if (!a_msg || a_msg->type != DIII::kMessage_GetAPI || !a_msg->data) {
                return;
            }
            auto*      api = static_cast<DIII::IAPI*>(a_msg->data);
            const bool ok  = api->RegisterCondition(
                "apparelPreviewed",
                [](const Json::Value& a_val, RE::FormType) -> std::unique_ptr<DIII::ICondition> {
                    if (!a_val.isBool()) {
                        return nullptr;
                    }
                    return std::make_unique<PreviewedCondition>(a_val.asBool());
                });
            g_diiiActive = ok;
            spdlog::info("DIII: 'apparelPreviewed' condition registered={} (api v{}).", ok,
                         api->GetVersion());
        });
        spdlog::info("DIII: registration listener armed at kPostLoad (success={}).", ok);
    }

    bool DIIIIntegration::IsActive() { return g_diiiActive.load(); }

}  // namespace AP
