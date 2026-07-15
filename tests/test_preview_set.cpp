// Pure-logic tests for PreviewSet. No engine, no RE:: types.
#include "PreviewSet.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(expr)                                                     \
    do {                                                                \
        if (!(expr)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++g_failures;                                               \
        }                                                               \
    } while (0)

int main() {
    using AP::PreviewSet;
    constexpr std::uint32_t kHead = 1u << 0;   // BipedObjectSlot masks are 1<<(slot-30)
    constexpr std::uint32_t kBody = 1u << 2;
    constexpr std::uint32_t kBodyAndHands = (1u << 2) | (1u << 3);
    // Variant keys as PreviewSession::VariantKey would produce them: 0 for a
    // plain row, distinct nonzero folds for enchanted / tempered rows.
    constexpr std::uint32_t kPlain    = 0;
    constexpr std::uint32_t kEnchanted = 0xA1B2C3D4;
    constexpr std::uint32_t kTempered  = 0x55667788;

    {  // toggle on, toggle off
        PreviewSet s;
        CHECK(s.Empty());
        CHECK(s.Toggle(0x100, kPlain, kHead) == PreviewSet::Change::kAdded);
        CHECK(!s.Empty());
        CHECK(s.Contains(0x100));
        CHECK(s.ContainsExact(0x100, kPlain));
        CHECK(s.UnionMask() == kHead);
        CHECK(s.Toggle(0x100, kPlain, kHead) == PreviewSet::Change::kRemoved);
        CHECK(s.Empty());
    }
    {  // non-overlapping pieces stack
        PreviewSet s;
        s.Toggle(0x100, kPlain, kHead);
        s.Toggle(0x200, kPlain, kBody);
        CHECK(s.Contains(0x100) && s.Contains(0x200));
        CHECK(s.UnionMask() == (kHead | kBody));
        CHECK(s.Size() == 2);
    }
    {  // overlapping piece evicts what it overlaps (and only that)
        PreviewSet s;
        s.Toggle(0x100, kPlain, kHead);
        s.Toggle(0x200, kPlain, kBody);
        s.Toggle(0x300, kPlain, kBodyAndHands);  // overlaps 0x200, not 0x100
        CHECK(!s.Contains(0x200));
        CHECK(s.Contains(0x100) && s.Contains(0x300));
        CHECK(s.UnionMask() == (kHead | kBodyAndHands));
    }
    {  // same identity toggled with a different mask still removes
        PreviewSet s;
        s.Toggle(0x100, kPlain, kHead);
        CHECK(s.Toggle(0x100, kPlain, kBody) == PreviewSet::Change::kRemoved);
        CHECK(s.Empty());
    }
    {  // THE BUG THIS GUARDS: rows sharing a base form but differing in extra
       // data (plain vs enchanted vs tempered Steel Helmet) are distinct
       // identities. Toggling the second one SWAPS the preview (same slots ->
       // eviction); only the toggled row is row-exact "previewed".
        PreviewSet s;
        CHECK(s.Toggle(0x100, kPlain, kHead) == PreviewSet::Change::kAdded);
        CHECK(s.Toggle(0x100, kEnchanted, kHead) == PreviewSet::Change::kAdded);
        CHECK(s.Size() == 1);
        CHECK(!s.ContainsExact(0x100, kPlain));
        CHECK(s.ContainsExact(0x100, kEnchanted));
        CHECK(!s.ContainsExact(0x100, kTempered));
        CHECK(s.Contains(0x100));  // form-level stays true: same 3D is injected
        CHECK(s.Toggle(0x100, kEnchanted, kHead) == PreviewSet::Change::kRemoved);
        CHECK(s.Empty());
    }
    {  // a tempered row is likewise distinct from the plain row
        PreviewSet s;
        s.Toggle(0x100, kTempered, kHead);
        CHECK(s.ContainsExact(0x100, kTempered));
        CHECK(!s.ContainsExact(0x100, kPlain));
        CHECK(s.RemoveExact(0x100, kPlain) == false);  // wrong row: no-op
        CHECK(s.RemoveExact(0x100, kTempered) == true);
        CHECK(s.Empty());
    }
    {  // Remove() (container-changed path) drops any variant of the base form
       // and leaves other forms alone; removing an absent id is a no-op
        PreviewSet s;
        s.Toggle(0x100, kEnchanted, kHead);
        s.Toggle(0x200, kPlain, kBody);
        CHECK(s.Remove(0x999) == false);
        CHECK(s.Remove(0x100) == true);
        CHECK(s.Size() == 1);
        CHECK(s.Contains(0x200));
        CHECK(s.Remove(0x200) == true);
        CHECK(s.Empty());
    }
    {  // Clear
        PreviewSet s;
        s.Toggle(0x100, kPlain, kHead);
        s.Toggle(0x200, kPlain, kBody);
        s.Clear();
        CHECK(s.Empty());
        CHECK(s.UnionMask() == 0);
    }
    {  // zero-mask items are rejected (no visible slots -> nothing to preview)
        PreviewSet s;
        CHECK(s.Toggle(0x100, kPlain, 0) == PreviewSet::Change::kRejected);
        CHECK(s.Empty());
    }

    if (g_failures == 0) {
        std::printf("PreviewSetTests: all passed\n");
        return 0;
    }
    std::printf("PreviewSetTests: %d failure(s)\n", g_failures);
    return 1;
}
