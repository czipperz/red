#include "test_base.hpp"

#include "text_replacement.hpp"

TEST_CASE("next_character() empty file") {
    red::FileBuffer file_buffer;

    red::Location location = {};
    char ch = red::next_character(file_buffer, &location);

    REQUIRE(ch == '\0');
    REQUIRE(location.index == 0);
    REQUIRE(location.line == 0);
    REQUIRE(location.column == 0);
}

TEST_CASE("next_character() normal chars") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"abc";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    red::Location location = {};
    char ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == 'a');
    REQUIRE(location.index == 1);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == 'b');
    REQUIRE(location.index == 2);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == 'c');
    REQUIRE(location.index == 3);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '\0');
    REQUIRE(location.index == 3);
    REQUIRE(location.line == 0);
    REQUIRE(location.column == 3);
}

TEST_CASE("next_character() trigraph") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"??""<";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    red::Location location = {};
    char ch = red::next_character(file_buffer, &location);

    REQUIRE(ch == '{');
    REQUIRE(location.index == 3);
    REQUIRE(location.line == 0);
    REQUIRE(location.column == 3); // @Fix: this fails right now
}

TEST_CASE("next_character() question mark chain no trigraphs") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"??????";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    red::Location location = {};
    char ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '?');
    REQUIRE(location.index == 1);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '?');
    REQUIRE(location.index == 2);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '?');
    REQUIRE(location.index == 3);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '?');
    REQUIRE(location.index == 4);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '?');
    REQUIRE(location.index == 5);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '?');
    REQUIRE(location.index == 6);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '\0');
    REQUIRE(location.index == 6);
}

TEST_CASE("next_character() backslash newline") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"\\\na";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    red::Location location = {};
    char ch = red::next_character(file_buffer, &location);

    REQUIRE(ch == 'a');
    REQUIRE(location.index == 3);
    REQUIRE(location.line == 1);
    REQUIRE(location.column == 1);
}

TEST_CASE("next_character() backslash trigraph newline") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"??""/\na";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    red::Location location = {};
    char ch = red::next_character(file_buffer, &location);

    REQUIRE(ch == 'a');
    REQUIRE(location.index == 5);
    REQUIRE(location.line == 1);
    REQUIRE(location.column == 1);
}

TEST_CASE("next_character() newline") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"a\nb";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    red::Location location = {};
    char ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == 'a');
    REQUIRE(location.index == 1);
    REQUIRE(location.line == 0);
    REQUIRE(location.column == 1);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '\n');
    REQUIRE(location.index == 2);
    REQUIRE(location.line == 1);
    REQUIRE(location.column == 0);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == 'b');
    REQUIRE(location.index == 3);
    REQUIRE(location.line == 1);
    REQUIRE(location.column == 1);
}

TEST_CASE("next_character() trigraph interrupted by backslash newline isn't a trigraph") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"??""\\\n>a";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    red::Location location = {};
    char ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '?');
    REQUIRE(location.index == 1);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '?');
    REQUIRE(location.index == 2);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == '>');
    REQUIRE(location.index == 5);

    ch = red::next_character(file_buffer, &location);
    REQUIRE(ch == 'a');
    REQUIRE(location.index == 6);
    REQUIRE(location.line == 1);
    REQUIRE(location.column == 2);
}

TEST_CASE("next_character() backslash newline repeatedly handled") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"\\\n\\\n\\\n0";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    red::Location location = {};
    char ch = red::next_character(file_buffer, &location);

    REQUIRE(ch == '0');
    REQUIRE(location.index == 7);
    REQUIRE(location.line == 3);
    REQUIRE(location.column == 1);
}
