#include "test_base.hpp"

#include <cz/defer.hpp>
#include <cz/mem/heap.hpp>
#include <czt/mock_allocate.hpp>
#include "token.hpp"

TEST_CASE("next_token() basic symbol") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"<";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    bool is_bol = false;
    cz::mem::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));

    CHECK(token.type == red::Token::LessThan);
    CHECK(token.start == 0);
    CHECK(token.end == 1);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() basic label") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"abc";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    bool is_bol = false;
    cz::mem::Allocated<cz::String> label_value;
    label_value.allocator = cz::mem::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));

    CHECK(token.type == red::Token::Label);
    CHECK(token.start == 0);
    CHECK(token.end == 3);
    CHECK(is_bol == false);
    CHECK(label_value.object == "abc");
}

TEST_CASE("next_token() underscores in label") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"_ab_c";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    bool is_bol = false;
    cz::mem::Allocated<cz::String> label_value;
    label_value.allocator = cz::mem::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));

    CHECK(token.type == red::Token::Label);
    CHECK(token.start == 0);
    CHECK(token.end == 5);
    CHECK(is_bol == false);
    CHECK(label_value.object == "_ab_c");
}

TEST_CASE("next_token() parenthesized label") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"(abc)";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    bool is_bol = false;
    cz::mem::Allocated<cz::String> label_value;
    label_value.allocator = cz::mem::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::OpenParen);
    CHECK(token.start == 0);
    CHECK(token.end == 1);
    CHECK(is_bol == false);
    CHECK(label_value.object == "");

    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.start == 1);
    CHECK(token.end == 4);
    CHECK(is_bol == false);
    CHECK(label_value.object == "abc");

    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::CloseParen);
    CHECK(token.start == 4);
    CHECK(token.end == 5);
    CHECK(is_bol == false);
    /* shouldn't write */ CHECK(label_value.object == "abc");
}

TEST_CASE("next_token() digraph") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"<::><%%>";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    bool is_bol = false;
    cz::mem::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::OpenSquare);
    CHECK(token.start == 0);
    CHECK(token.end == 2);
    CHECK(is_bol == false);

    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::CloseSquare);
    CHECK(token.start == 2);
    CHECK(token.end == 4);
    CHECK(is_bol == false);

    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::OpenCurly);
    CHECK(token.start == 4);
    CHECK(token.end == 6);
    CHECK(is_bol == false);

    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::CloseCurly);
    CHECK(token.start == 6);
    CHECK(token.end == 8);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() break token with whitespace") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"a b";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    bool is_bol = false;
    cz::mem::Allocated<cz::String> label_value;
    label_value.allocator = cz::mem::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.start == 0);
    CHECK(token.end == 1);
    CHECK(is_bol == false);
    CHECK(label_value.object == "a");

    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.start == 2);
    CHECK(token.end == 3);
    CHECK(is_bol == false);
    CHECK(label_value.object == "b");
    CHECK(index == token.end);
}

TEST_CASE("next_token() hash") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"#i";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    bool is_bol = false;
    cz::mem::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Hash);
    CHECK(token.start == 0);
    CHECK(token.end == 1);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() hash hash") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"##";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    bool is_bol = false;
    cz::mem::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::HashHash);
    CHECK(token.start == 0);
    CHECK(token.end == 2);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() doesn't set is_bol when no newline") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"#";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    bool is_bol = false;
    cz::mem::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(is_bol == false);

    index = 0;
    is_bol = true;
    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(is_bol == true);
}

TEST_CASE("next_token() hit newline sets is_bol") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"\n#";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    bool is_bol = false;
    cz::mem::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Hash);
    CHECK(token.start == 1);
    CHECK(token.end == 2);
    CHECK(is_bol == true);
}

TEST_CASE("next_token() on error index is set after whitespace") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)" $";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    bool is_bol = false;
    cz::mem::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE_FALSE(next_token(file_buffer, &index, &token, &is_bol, &label_value));
    REQUIRE(index == 1);
}
