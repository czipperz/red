#include "test_base.hpp"

#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include <czt/mock_allocate.hpp>
#include "preprocess.hpp"

using red::cpp::next_token;

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
    cz::AllocatedString label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));

    CHECK(token.type == red::Token::LessThan);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 1);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() basic label") {
    char* buffer = (char*)"abc";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.drop());
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));

    CHECK(token.type == red::Token::Label);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 3);
    CHECK(is_bol == false);
    CHECK(label_value == "abc");
}

TEST_CASE("next_token() underscores in label") {
    char* buffer = (char*)"_ab_c";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.drop());
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));

    CHECK(token.type == red::Token::Label);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 5);
    CHECK(is_bol == false);
    CHECK(label_value == "_ab_c");
}

TEST_CASE("next_token() parenthesized label") {
    char* buffer = (char*)"(abc)";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.drop());
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::OpenParen);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 1);
    CHECK(is_bol == false);
    CHECK(label_value == "");

    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.span.start.index == 1);
    CHECK(token.span.end.index == 4);
    CHECK(is_bol == false);
    CHECK(label_value == "abc");

    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::CloseParen);
    CHECK(token.span.start.index == 4);
    CHECK(token.span.end.index == 5);
    CHECK(is_bol == false);
    /* shouldn't write */ CHECK(label_value == "abc");
}

TEST_CASE("next_token() digraph") {
    char* buffer = (char*)"<::><%%>";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::OpenSquare);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 2);
    CHECK(is_bol == false);

    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::CloseSquare);
    CHECK(token.span.start.index == 2);
    CHECK(token.span.end.index == 4);
    CHECK(is_bol == false);

    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::OpenCurly);
    CHECK(token.span.start.index == 4);
    CHECK(token.span.end.index == 6);
    CHECK(is_bol == false);

    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::CloseCurly);
    CHECK(token.span.start.index == 6);
    CHECK(token.span.end.index == 8);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() break token with whitespace") {
    char* buffer = (char*)"a b";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.drop());
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 1);
    CHECK(is_bol == false);
    CHECK(label_value == "a");

    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.span.start.index == 2);
    CHECK(token.span.end.index == 3);
    CHECK(is_bol == false);
    CHECK(label_value == "b");
    CHECK(location.index == token.span.end.index);
}

TEST_CASE("next_token() hash") {
    char* buffer = (char*)"#i";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Hash);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 1);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() hash hash") {
    char* buffer = (char*)"##";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::HashHash);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 2);
    CHECK(is_bol == false);
}

TEST_CASE("next_token() doesn't set is_bol when no newline") {
    char* buffer = (char*)"#";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(is_bol == false);

    location = {};
    is_bol = true;
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(is_bol == true);
}

TEST_CASE("next_token() hit newline sets is_bol") {
    char* buffer = (char*)"\n#";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Hash);
    CHECK(token.span.start.index == 1);
    CHECK(token.span.end.index == 2);
    CHECK(is_bol == true);
}

TEST_CASE("next_token() on error index is set after whitespace") {
    char* buffer = (char*)" $";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::test::panic_allocator();
    REQUIRE_FALSE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    REQUIRE(location.index == 1);
}

TEST_CASE("next_token() string") {
    char* buffer = (char*)"\"abc\"";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.drop());
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::String);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == 5);
    CHECK(label_value == "abc");
}

TEST_CASE("next_token() Block comment") {
    char* buffer = (char*)"/*abc*/x";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.drop());
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.span.start.index == 7);
    CHECK(token.span.end.index == 8);
    CHECK(label_value == "x");
}

TEST_CASE("next_token() Empty block comment") {
    char* buffer = (char*)"/**/x";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.drop());
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == red::Token::Label);
    CHECK(token.span.start.index == 4);
    CHECK(token.span.end.index == 5);
    CHECK(label_value == "x");
}

TEST_CASE("next_token() Block comment nothing after") {
    char* buffer = (char*)"/**/";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.drop());
    REQUIRE_FALSE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
}

TEST_CASE("next_token() Block comment is not recursive") {
    char* buffer = (char*)"/*/**/";
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.drop());
    REQUIRE_FALSE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
}

void check_keyword(const char* str, red::Token::Type type_expected) {
    char* buffer = (char*)str;
    red::FileBuffer file_buffer = mem_buffer(&buffer);

    red::Location location = {};
    red::Token token;
    bool is_bol = false;
    cz::AllocatedString label_value;
    label_value.allocator = cz::heap_allocator();
    CZ_DEFER(label_value.drop());
    REQUIRE(next_token(nullptr, file_buffer, &location, &token, &is_bol, &label_value));
    CHECK(token.type == type_expected);
    CHECK(token.span.start.index == 0);
    CHECK(token.span.end.index == strlen(str));
}

TEST_CASE("next_token() keyword auto") {
    check_keyword("auto", red::Token::Auto);
}
TEST_CASE("next_token() keyword break") {
    check_keyword("break", red::Token::Break);
}
TEST_CASE("next_token() keyword case") {
    check_keyword("case", red::Token::Case);
}
TEST_CASE("next_token() keyword char") {
    check_keyword("char", red::Token::Char);
}
TEST_CASE("next_token() keyword const") {
    check_keyword("const", red::Token::Const);
}
TEST_CASE("next_token() keyword continue") {
    check_keyword("continue", red::Token::Continue);
}
TEST_CASE("next_token() keyword default") {
    check_keyword("default", red::Token::Default);
}
TEST_CASE("next_token() keyword do") {
    check_keyword("do", red::Token::Do);
}
TEST_CASE("next_token() keyword double") {
    check_keyword("double", red::Token::Double);
}
TEST_CASE("next_token() keyword else") {
    check_keyword("else", red::Token::Else);
}
TEST_CASE("next_token() keyword enum") {
    check_keyword("enum", red::Token::Enum);
}
TEST_CASE("next_token() keyword extern") {
    check_keyword("extern", red::Token::Extern);
}
TEST_CASE("next_token() keyword float") {
    check_keyword("float", red::Token::Float);
}
TEST_CASE("next_token() keyword for") {
    check_keyword("for", red::Token::For);
}
TEST_CASE("next_token() keyword goto") {
    check_keyword("goto", red::Token::Goto);
}
TEST_CASE("next_token() keyword if") {
    check_keyword("if", red::Token::If);
}
TEST_CASE("next_token() keyword int") {
    check_keyword("int", red::Token::Int);
}
TEST_CASE("next_token() keyword long") {
    check_keyword("long", red::Token::Long);
}
TEST_CASE("next_token() keyword register") {
    check_keyword("register", red::Token::Register);
}
TEST_CASE("next_token() keyword return") {
    check_keyword("return", red::Token::Return);
}
TEST_CASE("next_token() keyword short") {
    check_keyword("short", red::Token::Short);
}
TEST_CASE("next_token() keyword signed") {
    check_keyword("signed", red::Token::Signed);
}
TEST_CASE("next_token() keyword sizeof") {
    check_keyword("sizeof", red::Token::Sizeof);
}
TEST_CASE("next_token() keyword static") {
    check_keyword("static", red::Token::Static);
}
TEST_CASE("next_token() keyword struct") {
    check_keyword("struct", red::Token::Struct);
}
TEST_CASE("next_token() keyword switch") {
    check_keyword("switch", red::Token::Switch);
}
TEST_CASE("next_token() keyword typedef") {
    check_keyword("typedef", red::Token::Typedef);
}
TEST_CASE("next_token() keyword union") {
    check_keyword("union", red::Token::Union);
}
TEST_CASE("next_token() keyword unsigned") {
    check_keyword("unsigned", red::Token::Unsigned);
}
TEST_CASE("next_token() keyword void") {
    check_keyword("void", red::Token::Void);
}
TEST_CASE("next_token() keyword volatile") {
    check_keyword("volatile", red::Token::Volatile);
}
TEST_CASE("next_token() keyword while") {
    check_keyword("while", red::Token::While);
}

TEST_CASE("next_token() pipe") {
    check_keyword("|", red::Token::Pipe);
}
TEST_CASE("next_token() or") {
    check_keyword("||", red::Token::Or);
}

TEST_CASE("next_token() ampersand") {
    check_keyword("&", red::Token::Ampersand);
}
TEST_CASE("next_token() and") {
    check_keyword("&&", red::Token::And);
}
