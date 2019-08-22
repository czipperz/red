#include "test_base.hpp"

#include "token.hpp"

TEST_CASE("next_token() basic symbol") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"<";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    REQUIRE(next_token(file_buffer, &index, &token));

    CHECK(token.type == red::Token::LessThan);
    CHECK(token.start == 0);
    CHECK(token.end == 1);
}

TEST_CASE("next_token() basic label") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"abc";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    REQUIRE(next_token(file_buffer, &index, &token));

    CHECK(token.type == red::Token::Label);
    CHECK(token.start == 0);
    CHECK(token.end == 3);
}

TEST_CASE("next_token() underscores in label") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"_ab_c";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    REQUIRE(next_token(file_buffer, &index, &token));

    CHECK(token.type == red::Token::Label);
    CHECK(token.start == 0);
    CHECK(token.end == 5);
}

TEST_CASE("next_token() parenthesized label") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"(abc)";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    REQUIRE(next_token(file_buffer, &index, &token));
    CHECK(token.type == red::Token::OpenParen);
    CHECK(token.start == 0);
    CHECK(token.end == 1);

    REQUIRE(next_token(file_buffer, &index, &token));
    CHECK(token.type == red::Token::Label);
    CHECK(token.start == 1);
    CHECK(token.end == 4);

    REQUIRE(next_token(file_buffer, &index, &token));
    CHECK(token.type == red::Token::CloseParen);
    CHECK(token.start == 4);
    CHECK(token.end == 5);
}

TEST_CASE("next_token() digraph") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"<::><%%>";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    REQUIRE(next_token(file_buffer, &index, &token));
    CHECK(token.type == red::Token::OpenSquare);
    CHECK(token.start == 0);
    CHECK(token.end == 2);

    REQUIRE(next_token(file_buffer, &index, &token));
    CHECK(token.type == red::Token::CloseSquare);
    CHECK(token.start == 2);
    CHECK(token.end == 4);

    REQUIRE(next_token(file_buffer, &index, &token));
    CHECK(token.type == red::Token::OpenCurly);
    CHECK(token.start == 4);
    CHECK(token.end == 6);

    REQUIRE(next_token(file_buffer, &index, &token));
    CHECK(token.type == red::Token::CloseCurly);
    CHECK(token.start == 6);
    CHECK(token.end == 8);
}

TEST_CASE("next_token() break token with whitespace") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"a b";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    REQUIRE(next_token(file_buffer, &index, &token));
    CHECK(token.type == red::Token::Label);
    CHECK(token.start == 0);
    CHECK(token.end == 1);

    REQUIRE(next_token(file_buffer, &index, &token));
    CHECK(token.type == red::Token::Label);
    CHECK(token.start == 2);
    CHECK(token.end == 3);
}

TEST_CASE("next_token() hash") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"#i";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    REQUIRE(next_token(file_buffer, &index, &token));
    CHECK(token.type == red::Token::Hash);
    CHECK(token.start == 0);
    CHECK(token.end == 1);
}

TEST_CASE("next_token() hash hash") {
    red::FileBuffer file_buffer;
    char* buffer = (char*)"##";
    file_buffer.buffers = &buffer;
    file_buffer.buffers_len = 1;
    file_buffer.last_len = strlen(buffer);

    size_t index = 0;
    red::Token token;
    REQUIRE(next_token(file_buffer, &index, &token));
    CHECK(token.type == red::Token::HashHash);
    CHECK(token.start == 0);
    CHECK(token.end == 2);
}
