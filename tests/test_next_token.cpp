#include "test_base.hpp"

#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include <czt/mock_allocate.hpp>
#include "context.hpp"
#include "file_contents.hpp"
#include "lex.hpp"
#include "token.hpp"

using red::Integer_Suffix;
using red::lex::Lexer;
using red::lex::next_token;

#define SETUP(CONTENTS)                                     \
    char* buffer = (char*)(CONTENTS);                       \
    red::File_Contents file_contents = mem_buffer(&buffer); \
    red::Location location = {};                            \
    red::Token token = {};                                  \
    bool is_bol = false;                                    \
                                                            \
    Lexer lexer = {};                                       \
    lexer.init();                                           \
                                                            \
    red::Context context = {};                              \
    context.init();                                         \
                                                            \
    CZ_DEFER({                                              \
        lexer.drop();                                       \
        context.destroy();                                  \
    });

red::File_Contents mem_buffer(char** buffers) {
    red::File_Contents file_contents;
    file_contents.buffers = buffers;
    file_contents.buffers_len = 1;
    file_contents.len = strlen(buffers[0]);
    return file_contents;
}

TEST_CASE("next_token() basic symbol") {
    SETUP("<");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::LessThan);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 1);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() integer") {
    SETUP("123123");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Integer);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 6);
    CHECK(is_bol == false);
    CHECK(token.v.integer.value == 123123);
    CHECK(token.v.integer.suffix == 0);
}

TEST_CASE("next_token() integer 0") {
    SETUP("0");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Integer);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 1);
    CHECK(is_bol == false);
    CHECK(token.v.integer.value == 0);
    CHECK(token.v.integer.suffix == 0);
}

TEST_CASE("next_token() integer 33ul") {
    SETUP("33ul");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Integer);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 4);
    CHECK(is_bol == false);
    CHECK(token.v.integer.value == 33);
    CHECK(token.v.integer.suffix == (Integer_Suffix::Unsigned | Integer_Suffix::Long));
}

TEST_CASE("next_token() basic identifier") {
    SETUP("abc");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Identifier);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 3);
    CHECK(is_bol == false);
    CHECK(token.v.identifier.str == "abc");
}

TEST_CASE("next_token() underscores in identifier") {
    SETUP("_ab_c");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Identifier);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 5);
    CHECK(is_bol == false);
    CHECK(token.v.identifier.str == "_ab_c");
}

TEST_CASE("next_token() parenthesized identifier") {
    SETUP("(abc)");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::OpenParen);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 1);
    CHECK(is_bol == false);

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Identifier);
    CHECK(token.span.start.index == 1);
    CHECK(token.span.end.index == 4);
    CHECK(is_bol == false);
    CHECK(token.v.identifier.str == "abc");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::CloseParen);
    CHECK(token.span.start.index == 4);
    CHECK(token.span.end.index == 5);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() digraph") {
    SETUP("<::><%%>");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::OpenSquare);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 2);
    CHECK(is_bol == false);

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::CloseSquare);
    CHECK(token.span.start.index == 2);
    CHECK(token.span.end.index == 4);
    CHECK(is_bol == false);

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::OpenCurly);
    CHECK(token.span.start.index == 4);
    CHECK(token.span.end.index == 6);
    CHECK(is_bol == false);

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::CloseCurly);
    CHECK(token.span.start.index == 6);
    CHECK(token.span.end.index == 8);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() break token with whitespace") {
    SETUP("a b");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Identifier);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 1);
    CHECK(is_bol == false);
    CHECK(token.v.identifier.str == "a");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Identifier);
    CHECK(token.span.start.index == 2);
    CHECK(token.span.end.index == 3);
    CHECK(is_bol == false);
    CHECK(token.v.identifier.str == "b");
    CHECK(location.index == token.span.end.index);
}

TEST_CASE("next_token() character literal 'a'") {
    SETUP("'a'");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 3);
    CHECK(is_bol == false);
    REQUIRE(token.type == red::Token::Character);
    CHECK(token.v.ch == 'a');
}

TEST_CASE("next_token() character literal '\\n'") {
    SETUP("'\\n'");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 4);
    CHECK(is_bol == false);
    REQUIRE(token.type == red::Token::Character);
    CHECK(token.v.ch == '\n');
}

TEST_CASE("next_token() hash") {
    SETUP("#i");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Hash);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 1);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() hash hash") {
    SETUP("##");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::HashHash);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 2);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() doesn't set is_bol when no newline") {
    SETUP("#");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(is_bol == false);

    location = {};
    is_bol = true;
    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(is_bol == true);
}

TEST_CASE("next_token() hit newline sets is_bol") {
    SETUP("\n#");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Hash);
    CHECK(token.span.start.index == 1);
    CHECK(token.span.end.index == 2);
    CHECK(is_bol == true);
}

TEST_CASE("next_token() on error index is set after whitespace") {
    SETUP(" $");

    REQUIRE_FALSE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    REQUIRE(location.index == 1);
}

TEST_CASE("next_token() string") {
    SETUP("\"abc\"");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::String);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 5);
    CHECK(token.v.identifier.str == "abc");
}

TEST_CASE("next_token() Block comment") {
    SETUP("/*abc*/x");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Identifier);
    CHECK(token.span.start.index == 7);
    CHECK(token.span.end.index == 8);
    CHECK(token.v.identifier.str == "x");
}

TEST_CASE("next_token() Empty block comment") {
    SETUP("/**/x");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Identifier);
    CHECK(token.span.start.index == 4);
    CHECK(token.span.end.index == 5);
    CHECK(token.v.identifier.str == "x");
}

TEST_CASE("next_token() Block comment nothing after") {
    SETUP("/**/");

    REQUIRE_FALSE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
}

TEST_CASE("next_token() Block comment is not recursive") {
    SETUP("/*/**/");

    REQUIRE_FALSE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
}

void check_keyword(const char* str, red::Token::Type type_expected) {
    SETUP(str);

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == type_expected);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == strlen(str));
}

TEST_CASE("next_token() pipe") {
    check_keyword("|", red::Token::Pipe);
}
TEST_CASE("next_token() or") {
    check_keyword("||", red::Token::Or);
}

TEST_CASE("next_token() question mark") {
    check_keyword("?", red::Token::QuestionMark);
}

TEST_CASE("next_token() ampersand") {
    check_keyword("&", red::Token::Ampersand);
}
TEST_CASE("next_token() and") {
    check_keyword("&&", red::Token::And);
}

TEST_CASE("next_token() tilde") {
    check_keyword("~", red::Token::Tilde);
}

TEST_CASE("next_token() ...") {
    check_keyword("...", red::Token::Preprocessor_Varargs_Parameter_Indicator);
}

TEST_CASE("next_token() string with escape characters") {
    SETUP("\"\\\"\\\\abc\\\"\"");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::String);
    CHECK(token.v.string == "\"\\abc\"");
    REQUIRE(context.errors.len() == 0);
}

TEST_CASE("next_token() identifier trigraph backslash and then newline") {
    SETUP("ab?""?/\nc");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Identifier);
    CHECK(token.v.string == "abc");
    REQUIRE(context.errors.len() == 0);

    REQUIRE(!next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
}

TEST_CASE("next_token() identifier backslash and then newline") {
    SETUP("ab\\\nc");

    REQUIRE(next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
    CHECK(token.type == red::Token::Identifier);
    CHECK(token.v.string == "abc");
    REQUIRE(context.errors.len() == 0);

    REQUIRE(!next_token(&context, &lexer, file_contents, &location, &token, &is_bol));
}
