#include <czt/test_base.hpp>

#include <limits.h>
#include <cz/defer.hpp>
#include <cz/mem/heap.hpp>
#include <czt/mock_allocate.hpp>
#include "string_map.hpp"

using namespace cz;

TEST_CASE("StringMap construction") {
    StringMap<int> map;

    CHECK(map.count() == 0);
    CHECK(map.cap() == 0);
}

TEST_CASE("StringMap reserve") {
    StringMap<int> map;
    auto allocator = cz::mem::heap_allocator();
    CZ_DEFER(map.drop(allocator));
    map.reserve(allocator, 4);

    CHECK(map.count() == 0);
    CHECK(map.cap() == 4);
}

TEST_CASE("StringMap drop") {
    StringMap<int> map;
    char mask_buffer[(4 + (CHAR_BIT / 2) - 1) / (CHAR_BIT / 2)];
    Hash hash_buffer[4];
    Str str_buffer[4];
    int int_buffer[4];

    cz::test::MockAllocate deallocs[] = {cz::test::mock_dealloc({mask_buffer, sizeof(mask_buffer)}),
                                         cz::test::mock_dealloc({hash_buffer, sizeof(hash_buffer)}),
                                         cz::test::mock_dealloc({str_buffer, sizeof(str_buffer)}),
                                         cz::test::mock_dealloc({int_buffer, sizeof(int_buffer)})};
    cz::test::MockAllocateMultiple mock_dealloc(deallocs);
    CZ_DEFER(map.drop(mock_dealloc.allocator()));

    cz::test::MockAllocate allocs[] = {
        cz::test::mock_alloc(mask_buffer, {sizeof(mask_buffer), alignof(char)}),
        cz::test::mock_alloc(hash_buffer, {sizeof(hash_buffer), alignof(Hash)}),
        cz::test::mock_alloc(str_buffer, {sizeof(str_buffer), alignof(Str)}),
        cz::test::mock_alloc(int_buffer, {sizeof(int_buffer), alignof(int)})};
    cz::test::MockAllocateMultiple mock_alloc(allocs);
    map.reserve(mock_alloc.allocator(), 4);

    CHECK(map.cap() == 4);
}

TEST_CASE("StringMap find empty map") {
    StringMap<int> map;
    auto allocator = cz::mem::heap_allocator();
    CZ_DEFER(map.drop(allocator));

    auto entry = map.find("a");
    REQUIRE_FALSE(entry.is_present());
    REQUIRE_FALSE(entry.can_insert());
    REQUIRE(entry.and_set(3) == nullptr);
    REQUIRE_FALSE(entry.and_remove(allocator));
}

TEST_CASE("StringMap finds spot immediately") {
    StringMap<int> map;
    auto allocator = cz::mem::heap_allocator();
    CZ_DEFER(map.drop(allocator));

    map.reserve(allocator, 1);

    auto entry = map.find("a");
    REQUIRE_FALSE(entry.is_present());
    REQUIRE(entry.can_insert());
    REQUIRE(entry.and_set(3) == nullptr);
    REQUIRE_FALSE(entry.and_remove(allocator));

    REQUIRE(entry.or_insert(allocator, 3) == 3);

    REQUIRE(entry.is_present());
}

TEST_CASE("StringMap after insert can be found") {
    StringMap<int> map;
    auto allocator = cz::mem::heap_allocator();
    CZ_DEFER(map.drop(allocator));

    map.reserve(allocator, 1);

    map.find("a").or_insert(allocator, 3);

    auto entry = map.find("a");
    REQUIRE(entry.is_present());
    REQUIRE(entry.value() != nullptr);
    REQUIRE(*entry.value() == 3);
}

TEST_CASE("StringMap after reserve can find") {
    StringMap<int> map;
    auto allocator = cz::mem::heap_allocator();
    CZ_DEFER(map.drop(allocator));

    map.reserve(allocator, 1);

    map.find("a").or_insert(allocator, 3);

    map.reserve(allocator, 7);

    auto entry = map.find("a");
    REQUIRE(entry.is_present());
    REQUIRE(entry.value() != nullptr);
    REQUIRE(*entry.value() == 3);

    entry = map.find("b");
    REQUIRE_FALSE(entry.is_present());
    REQUIRE(entry.value() == nullptr);
    REQUIRE(entry.or_insert(allocator, -42) == -42);
}

TEST_CASE("StringMap has collision advances to next slot") {
    StringMap<int> map;
    auto allocator = cz::mem::heap_allocator();
    CZ_DEFER(map.drop(allocator));

    map.reserve(allocator, 2);

    map.find("a").or_insert(allocator, 3);

    auto entry = map.find("c");  // With the current hash algorithm this collides
    REQUIRE_FALSE(entry.is_present());
    REQUIRE(entry.value() == nullptr);
    REQUIRE(entry.or_insert(allocator, -42) == -42);

    REQUIRE(map.find("a").value() != nullptr);
    REQUIRE(*map.find("a").value() == 3);
    REQUIRE(map.find("c").value() != nullptr);
    REQUIRE(*map.find("c").value() == -42);
}

TEST_CASE("StringMap after reserve can find #2") {
    StringMap<int> map;
    auto allocator = cz::mem::heap_allocator();
    CZ_DEFER(map.drop(allocator));

    map.reserve(allocator, 2);

    map.find("a").or_insert(allocator, 3);

    auto entry = map.find("c");  // With the current hash algorithm this collides
    REQUIRE_FALSE(entry.is_present());
    REQUIRE(entry.value() == nullptr);
    REQUIRE(entry.or_insert(allocator, -42) == -42);

    REQUIRE(map.find("a").value() != nullptr);
    REQUIRE(*map.find("a").value() == 3);
    REQUIRE(map.find("c").value() != nullptr);
    REQUIRE(*map.find("c").value() == -42);
}

TEST_CASE("StringMap insert insert collision remove") {
    StringMap<int> map;
    auto allocator = cz::mem::heap_allocator();
    CZ_DEFER(map.drop(allocator));

    map.reserve(allocator, 2);

    map.find("a").or_insert(allocator, 3);
    map.find("c").or_insert(allocator, -42);  // With the current hash algorithm this collides
    map.find("a").and_remove(allocator);

    REQUIRE_FALSE(map.find("a").is_present());
    REQUIRE(map.find("c").value() != nullptr);
    REQUIRE(*map.find("c").value() == -42);
}

TEST_CASE("StringMap or_insert doesn't change the value") {
    StringMap<int> map;
    auto allocator = cz::mem::heap_allocator();
    CZ_DEFER(map.drop(allocator));

    map.reserve(allocator, 2);

    map.find("a").or_insert(allocator, 3);
    map.find("a").or_insert(cz::test::panic_allocator(), 5);

    REQUIRE(map.find("a").value() != nullptr);
    REQUIRE(*map.find("a").value() == 3);
    REQUIRE(map.count() == 1);
}

TEST_CASE("StringMap and_set not present does nothing") {
    StringMap<int> map;
    auto allocator = cz::mem::heap_allocator();
    CZ_DEFER(map.drop(allocator));

    map.reserve(allocator, 2);

    REQUIRE(map.find("a").and_set(1) == nullptr);

    REQUIRE(map.find("a").value() == nullptr);
}

TEST_CASE("StringMap and_set overrides value") {
    StringMap<int> map;
    auto allocator = cz::mem::heap_allocator();
    CZ_DEFER(map.drop(allocator));

    map.reserve(allocator, 2);

    map.find("a").or_insert(allocator, 1);
    map.find("a").and_set(3);

    REQUIRE(map.find("a").value() != nullptr);
    REQUIRE(*map.find("a").value() == 3);
    REQUIRE(map.count() == 1);
}
