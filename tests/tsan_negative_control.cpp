// Not a ctest: shares a non-atomic tree between threads on purpose, so that a
// ThreadSanitizer build must report races. Guards against a silently inert TSan.
#include "game/hulh.hpp"
#include "search/search.hpp"
int main() {
    pm::Rng rng(1);
    pm::Hulh s = pm::Hulh::deal(rng);
    pm::Tree<pm::Hulh, pm::PlainF64> t{pm::SearchConfig{}};
    pm::tree_search<pm::Hulh, pm::PlainF64>(t, s, s.player(), 4000, 4, 1);
}
