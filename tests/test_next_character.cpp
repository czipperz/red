#include "test_base.hpp"

#include "file_contents.hpp"
#include "lex.hpp"
#include "location.hpp"

using red::lex::next_character;

TEST_CASE("next_character() empty file") {
    red::File_Contents file_contents = {};

    red::Location location = {};
    char ch;

    REQUIRE_FALSE(next_character(file_contents, &location, &ch));
    CHECK(location.index == 0);
    CHECK(location.line == 0);
    CHECK(location.column == 0);
}

TEST_CASE("next_character() normal chars") {
    red::File_Contents file_contents = {};
    char* buffer = (char*)"abc";
    file_contents.buffers = &buffer;
    file_contents.buffers_len = 1;
    file_contents.len = strlen(buffer);

    red::Location location = {};
    char ch;
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
    red::File_Contents file_contents = {};
    char* buffer = (char*)"??""<";
    file_contents.buffers = &buffer;
    file_contents.buffers_len = 1;
    file_contents.len = strlen(buffer);

    red::Location location = {};
    char ch;

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == '{');
    CHECK(location.index == 3);
    CHECK(location.line == 0);
    CHECK(location.column == 3);
}

TEST_CASE("next_character() question mark chain no trigraphs") {
    red::File_Contents file_contents = {};
    char* buffer = (char*)"??????";
    file_contents.buffers = &buffer;
    file_contents.buffers_len = 1;
    file_contents.len = strlen(buffer);

    red::Location location = {};
    char ch;
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
    red::File_Contents file_contents = {};
    char* buffer = (char*)"\\\na";
    file_contents.buffers = &buffer;
    file_contents.buffers_len = 1;
    file_contents.len = strlen(buffer);

    red::Location location = {};
    char ch;

    REQUIRE(next_character(file_contents, &location, &ch));
    REQUIRE(ch == 'a');
    CHECK(location.index == 3);
    CHECK(location.line == 1);
    CHECK(location.column == 1);
}

TEST_CASE("next_character() backslash trigraph newline") {
    red::File_Contents file_contents = {};
    char* buffer = (char*)"??""/\na";
    file_contents.buffers = &buffer;
    file_contents.buffers_len = 1;
    file_contents.len = strlen(buffer);

    red::Location location = {};
    char ch;

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(ch == 'a');
    CHECK(location.index == 5);
    CHECK(location.line == 1);
    CHECK(location.column == 1);
}

TEST_CASE("next_character() newline") {
    red::File_Contents file_contents = {};
    char* buffer = (char*)"a\nb";
    file_contents.buffers = &buffer;
    file_contents.buffers_len = 1;
    file_contents.len = strlen(buffer);

    red::Location location = {};
    char ch;
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
    red::File_Contents file_contents = {};
    char* buffer = (char*)"??""\\\n>a";
    file_contents.buffers = &buffer;
    file_contents.buffers_len = 1;
    file_contents.len = strlen(buffer);

    red::Location location = {};
    char ch;
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
    red::File_Contents file_contents = {};
    char* buffer = (char*)"\\\n\\\n\\\n0";
    file_contents.buffers = &buffer;
    file_contents.buffers_len = 1;
    file_contents.len = strlen(buffer);

    red::Location location = {};
    char ch;

    REQUIRE(next_character(file_contents, &location, &ch));
    CHECK(location.index == 7);
    CHECK(location.line == 3);
    CHECK(location.column == 1);
}
