#include "test_base.hpp"

#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include <czt/mock_allocate.hpp>
#include "token.hpp"

red::FileBuffer mem_buffer(char** buffers) {
    red::FileBuffer file_buffer;
    file_buffer.buffers = buffers;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffers[0]);
    return file_buffer;
}

TEST_CASE("next_token() basic symbol") {
    char* buffer = (char*)"<";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));

    CHECK(token.type == red::Token::LessThan);
    CHECK(token.start.index == 0);
    CHECK(token.end.index == 1);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() basic label") {
    char* buffer = (char*)"abc";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));

    CHECK(token.type == red::Token::Label);
    CHECK(token.start.index == 0);
    CHECK(token.end.index == 3);
    CHECK(is_bol == false);
    CHECK(label_value.object == "abc");
}

TEST_CASE("next_token() underscores in label") {
    char* buffer = (char*)"_ab_c";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));

    CHECK(token.type == red::Token::Label);
    CHECK(token.start.index == 0);
    CHECK(token.end.index == 5);
    CHECK(is_bol == false);
    CHECK(label_value.object == "_ab_c");
}

TEST_CASE("next_token() parenthesized label") {
    char* buffer = (char*)"(abc)";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::OpenParen);
    CHECK(token.start.index == 0);
    CHECK(token.end.index == 1);
    CHECK(is_bol == false);
    CHECK(label_value.object == "");

    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.start.index == 1);
    CHECK(token.end.index == 4);
    CHECK(is_bol == false);
    CHECK(label_value.object == "abc");

    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::CloseParen);
    CHECK(token.start.index == 4);
    CHECK(token.end.index == 5);
    CHECK(is_bol == false);
    /* shouldn't write */ CHECK(label_value.object == "abc");
}

TEST_CASE("next_token() digraph") {
    char* buffer = (char*)"<::><%%>";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::OpenSquare);
    CHECK(token.start.index == 0);
    CHECK(token.end.index == 2);
    CHECK(is_bol == false);

    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::CloseSquare);
    CHECK(token.start.index == 2);
    CHECK(token.end.index == 4);
    CHECK(is_bol == false);

    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::OpenCurly);
    CHECK(token.start.index == 4);
    CHECK(token.end.index == 6);
    CHECK(is_bol == false);

    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::CloseCurly);
    CHECK(token.start.index == 6);
    CHECK(token.end.index == 8);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() break token with whitespace") {
    char* buffer = (char*)"a b";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.start.index == 0);
    CHECK(token.end.index == 1);
    CHECK(is_bol == false);
    CHECK(label_value.object == "a");

    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.start.index == 2);
    CHECK(token.end.index == 3);
    CHECK(is_bol == false);
    CHECK(label_value.object == "b");
    CHECK(location.index == token.end.index);
}

TEST_CASE("next_token() hash") {
    char* buffer = (char*)"#i";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Hash);
    CHECK(token.start.index == 0);
    CHECK(token.end.index == 1);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() hash hash") {
    char* buffer = (char*)"##";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::HashHash);
    CHECK(token.start.index == 0);
    CHECK(token.end.index == 2);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() doesn't set is_bol when no newline") {
    char* buffer = (char*)"#";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(is_bol == false);

    location = {};
    is_bol = true;
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(is_bol == true);
}

TEST_CASE("next_token() hit newline sets is_bol") {
    char* buffer = (char*)"\n#";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Hash);
    CHECK(token.start.index == 1);
    CHECK(token.end.index == 2);
    CHECK(is_bol == true);
}

TEST_CASE("next_token() on error index is set after whitespace") {
    char* buffer = (char*)" $";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE_FALSE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    REQUIRE(location.index == 1);
}

TEST_CASE("next_token() string") {
    char* buffer = (char*)"\"abc\"";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::String);
    CHECK(token.start.index == 0);
    CHECK(token.end.index == 5);
    CHECK(label_value.object == "abc");
}

TEST_CASE("Block comment") {
    char* buffer = (char*)"/*abc*/x";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.start.index == 7);
    CHECK(token.end.index == 8);
    CHECK(label_value.object == "x");
}

TEST_CASE("Empty block comment") {
    char* buffer = (char*)"/**/x";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.start.index == 4);
    CHECK(token.end.index == 5);
    CHECK(label_value.object == "x");
}

TEST_CASE("Block comment nothing after") {
    char* buffer = (char*)"/**/";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE_FALSE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
}

TEST_CASE("Block comment is not recursive") {
    char* buffer = (char*)"/*/**/";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::Allocated<cz::String> label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.object.drop(label_value.allocator));
    REQUIRE_FALSE(next_token(file_buffer, &location, &token, &is_bol, &label_value));
}
