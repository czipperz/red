#include "test_base.hpp"

#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include "file_contents.hpp"
#include "lex.hpp"
#include "location.hpp"

using red::lex::next_character;

#define SETUP(CONTENTS)                                       \
    red::File_Contents file_contents = {};                    \
    file_contents.load_str(CONTENTS, cz::heap_allocator());   \
    CZ_DEFER(file_contents.drop_array(cz::heap_allocator())); \
                                                              \
    red::Location location = {};                              \
    char ch;

TEST_CASE("next_character() empty file") {
    SETUP("");
    REQUIRE_FALSE(next_character(file_contents, &location, &ch));
    CHECK(location.index == 0);
    CHECK(location.line == 0);
    CHECK(location.column == 0);
}

TEST_CASE("next_character() normal chars") {
    SETUP("abc");

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == 'a');
    CHECK(location.index == 1);

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == 'b');
    CHECK(location.index == 2);

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == 'c');
    CHECK(location.index == 3);

    REQUIRE_FALSE(next_character(file_contents, &location, &ch));
    CHECK(location.index == 3);
    CHECK(location.line == 0);
    CHECK(location.column == 3);
}

TEST_CASE("next_character() trigraph") {
    SETUP(
        "??"
        "<");

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == '{');
    CHECK(location.index == 3);
    CHECK(location.line == 0);
    CHECK(location.column == 3);
}

TEST_CASE("next_character() question mark chain no trigraphs") {
    SETUP("??????");

    REQUIRE(next_character(file_contents, &location, &ch));
    REQUIRE(ch == '?');
    CHECK(location.index == 1);

    REQUIRE(next_character(file_contents, &location, &ch));
    REQUIRE(ch == '?');
    CHECK(location.index == 2);

    REQUIRE(next_character(file_contents, &location, &ch));
    REQUIRE(ch == '?');
    CHECK(location.index == 3);

    REQUIRE(next_character(file_contents, &location, &ch));
    REQUIRE(ch == '?');
    CHECK(location.index == 4);

    REQUIRE(next_character(file_contents, &location, &ch));
    REQUIRE(ch == '?');
    CHECK(location.index == 5);

    REQUIRE(next_character(file_contents, &location, &ch));
    REQUIRE(ch == '?');
    CHECK(location.index == 6);

    REQUIRE_FALSE(next_character(file_contents, &location, &ch));
    CHECK(location.index == 6);
}

TEST_CASE("next_character() backslash newline") {
    SETUP("\\\na");

    REQUIRE(next_character(file_contents, &location, &ch));
    REQUIRE(ch == 'a');
    CHECK(location.index == 3);
    CHECK(location.line == 1);
    CHECK(location.column == 1);
}

TEST_CASE("next_character() backslash trigraph newline") {
    SETUP(
        "??"
        "/\na");

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == 'a');
    CHECK(location.index == 5);
    CHECK(location.line == 1);
    CHECK(location.column == 1);
}

TEST_CASE("next_character() newline") {
    SETUP("a\nb");

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == 'a');
    CHECK(location.index == 1);
    CHECK(location.line == 0);
    CHECK(location.column == 1);

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == '\n');
    CHECK(location.index == 2);
    CHECK(location.line == 1);
    CHECK(location.column == 0);

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == 'b');
    CHECK(location.index == 3);
    CHECK(location.line == 1);
    CHECK(location.column == 1);
}

TEST_CASE("next_character() trigraph interrupted by backslash newline isn't a trigraph") {
    SETUP(
        "??"
        "\\\n>a");

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == '?');
    CHECK(location.index == 1);

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == '?');
    CHECK(location.index == 2);

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == '>');
    CHECK(location.index == 5);

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == 'a');
    CHECK(location.index == 6);
    CHECK(location.line == 1);
    CHECK(location.column == 2);
}

TEST_CASE("next_character() backslash newline repeatedly handled") {
    SETUP("\\\n\\\n\\\n0");

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(location.index == 7);
    CHECK(location.line == 3);
    CHECK(location.column == 1);
}
