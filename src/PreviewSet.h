#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace AP {

    // The set of armors currently being previewed. Identity is FormID plus a
    // "variant key" derived from the row's extra data, because several list
    // rows can share one base form: a plain Steel Helmet, a player-enchanted
    // one, and a tempered "(Fine)" one are three rows of the same ARMO. The
    // biped slot mask is cached per entry. Pure logic: engine-free so it is
    // unit-testable (PreviewSession::VariantKey computes the key engine-side).
    // Not thread-safe; callers synchronize (PreviewSession owns the lock).
    class PreviewSet {
    public:
        enum class Change { kAdded, kRemoved, kRejected };

        struct Entry {
            std::uint32_t formID{ 0 };
            std::uint32_t variantKey{ 0 };  // 0 = plain row (no distinguishing extra data)
            std::uint32_t slotMask{ 0 };
        };

        Change Toggle(std::uint32_t a_formID, std::uint32_t a_variantKey, std::uint32_t a_slotMask) {
            if (RemoveExact(a_formID, a_variantKey)) {
                return Change::kRemoved;
            }
            if (a_slotMask == 0) {
                return Change::kRejected;
            }
            // A new piece evicts everything it overlaps (including another
            // variant of the same base form - identical slots).
            std::erase_if(entries_, [&](const Entry& e) { return (e.slotMask & a_slotMask) != 0; });
            entries_.push_back({ a_formID, a_variantKey, a_slotMask });
            return Change::kAdded;
        }

        bool RemoveExact(std::uint32_t a_formID, std::uint32_t a_variantKey) {
            return std::erase_if(entries_, [&](const Entry& e) {
                       return e.formID == a_formID && e.variantKey == a_variantKey;
                   }) > 0;
        }

        // Any-variant removal: the container-changed event only carries the
        // base form, so acquiring any variant drops the preview.
        bool Remove(std::uint32_t a_formID) {
            return std::erase_if(entries_, [&](const Entry& e) { return e.formID == a_formID; }) > 0;
        }

        void Clear() { entries_.clear(); }

        // Form-level: the injected 3D is per base form, so BipedHooks and the
        // worn-replacement logic ask this. Row markers must NOT - see
        // ContainsExact.
        [[nodiscard]] bool Contains(std::uint32_t a_formID) const {
            return std::ranges::any_of(entries_, [&](const Entry& e) { return e.formID == a_formID; });
        }

        // Row-exact: only the row the user actually toggled.
        [[nodiscard]] bool ContainsExact(std::uint32_t a_formID, std::uint32_t a_variantKey) const {
            return std::ranges::any_of(entries_, [&](const Entry& e) {
                return e.formID == a_formID && e.variantKey == a_variantKey;
            });
        }

        [[nodiscard]] std::uint32_t UnionMask() const {
            std::uint32_t mask = 0;
            for (const auto& e : entries_) {
                mask |= e.slotMask;
            }
            return mask;
        }

        [[nodiscard]] bool        Empty() const { return entries_.empty(); }
        [[nodiscard]] std::size_t Size() const { return entries_.size(); }
        [[nodiscard]] const std::vector<Entry>& Entries() const { return entries_; }

    private:
        std::vector<Entry> entries_;
    };

}  // namespace AP
