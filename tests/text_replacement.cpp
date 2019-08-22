#include "test_base.hpp"

#include "text_replacement.hpp"

TEST_CASE("next_character() empty file") {
    red::FileBuffer file_buffer;

    size_t index = 0;
    char ch = red::next_character(file_buffer, &index);

    REQUIRE(ch == '\0');
    REQUIRE(index == 0);
}

TEST_CASE("next_character() normal chars") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"abc";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    char ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == 'a');
    REQUIRE(index == 1);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == 'b');
    REQUIRE(index == 2);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == 'c');
    REQUIRE(index == 3);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == '\0');
    REQUIRE(index == 3);
}

TEST_CASE("next_character() trigraph") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"??""<";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    char ch = red::next_character(file_buffer, &index);

    REQUIRE(ch == '{');
    REQUIRE(index == 3);
}

TEST_CASE("next_character() question mark chain no trigraphs") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"??????";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    char ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == '?');
    REQUIRE(index == 1);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == '?');
    REQUIRE(index == 2);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == '?');
    REQUIRE(index == 3);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == '?');
    REQUIRE(index == 4);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == '?');
    REQUIRE(index == 5);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == '?');
    REQUIRE(index == 6);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == '\0');
    REQUIRE(index == 6);
}

TEST_CASE("next_character() backslash newline") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"\\\na";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);
    size_t index = 0;
    char ch = red::next_character(file_buffer, &index);

    REQUIRE(ch == 'a');
    REQUIRE(index == 3);
}

TEST_CASE("next_character() backslash trigraph newline") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"??""/\na";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    char ch = red::next_character(file_buffer, &index);

    REQUIRE(ch == 'a');
    REQUIRE(index == 5);
}

TEST_CASE("next_character() trigraph interrupted by backslash newline isn't a trigraph") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"??""\\\n>a";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    char ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == '?');
    REQUIRE(index == 1);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == '?');
    REQUIRE(index == 2);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == '>');
    REQUIRE(index == 5);

    ch = red::next_character(file_buffer, &index);
    REQUIRE(ch == 'a');
    REQUIRE(index == 6);
}

TEST_CASE("next_character() backslash newline repeatedly handled") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"\\\n\\\n\\\n0";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);
    size_t index = 0;
    char ch = red::next_character(file_buffer, &index);

    REQUIRE(ch == '0');
    REQUIRE(index == 7);
}
