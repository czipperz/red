#include "test_base.hpp"

#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include "context.hpp"
#include "file_contents.hpp"
#include "hashed_str.hpp"
#include "lex.hpp"
#include "load.hpp"
#include "preprocess.hpp"
#include "token.hpp"

using namespace red;
using namespace red::cpp;

static void setup(Context* context, Preprocessor* preprocessor, cz::Str contents) {
    context->init();

    include_file_reserve(&context->files, preprocessor);

    File_Contents file_contents;
    file_contents.load_str(contents, context->files.file_array_buffer_array.allocator());
    Hashed_Str file_path = Hashed_Str::from_str("*test_file*");
    force_include_file(&context->files, preprocessor, file_path, file_contents);
}

#define SETUP(CONTENTS)                       \
    Context context = {};                     \
    Preprocessor preprocessor = {};           \
    setup(&context, &preprocessor, CONTENTS); \
                                              \
    lex::Lexer lexer = {};                    \
    lexer.init();                             \
                                              \
    Token token;                              \
                                              \
    CZ_DEFER({                                \
        lexer.drop();                         \
        preprocessor.destroy();               \
        context.destroy();                    \
    })

#define EAT_NEXT() next_token(&context, &preprocessor, &lexer, &token)

TEST_CASE("Preprocessor::next empty file") {
    SETUP("");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ignores empty #pragma") {
    SETUP("#pragma\n<");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::LessThan);
    CHECK(token.span.start.file == 0);
    CHECK(token.span.start.index == 8);
    CHECK(token.span.start.line == 1);
    CHECK(token.span.start.column == 0);
    CHECK(token.span.end.file == 0);
    CHECK(token.span.end.index == 9);
    CHECK(token.span.end.line == 1);
    CHECK(token.span.end.column == 1);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next define is skipped with value") {
    SETUP("#define x a\n");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next define is skipped no value") {
    SETUP("#define x\na");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ifdef without definition is skipped") {
    SETUP("#ifdef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ifdef without definition empty body") {
    SETUP("#ifdef x\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ifndef without definition is preserved") {
    SETUP("#ifndef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ifndef without definition empty body") {
    SETUP("#ifndef x\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ifdef with definition is preserved") {
    SETUP("#define x\n#ifdef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ifndef with definition is skipped") {
    SETUP("#define x\n#ifndef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next #undef then #ifndef") {
    SETUP("#define x\n#undef x\n#ifndef x\nabc\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "abc");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next #undef then #ifdef") {
    SETUP("#define x\n#undef x\n#ifdef x\nabc\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE(
    "Preprocessor::next in not taken #if, newline between # and endif shouldn't recognize it as "
    "#endif") {
    SETUP("#ifdef x\n#\nendif\n#endif\n");

    REQUIRE(EAT_NEXT().type == Result::Done);

    REQUIRE(context.errors.len() == 0);
}

TEST_CASE(
    "Preprocessor::next in taken #if, newline between # and endif shouldn't recognize it as "
    "#endif") {
    SETUP("#ifndef x\n#\nendif\n#endif\n");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "endif");

    REQUIRE(EAT_NEXT().type == Result::Done);

    REQUIRE(context.errors.len() == 0);
}

TEST_CASE("Preprocessor::next random #endif is error") {
    SETUP("#endif");

    REQUIRE(EAT_NEXT().type == Result::ErrorInvalidInput);

    REQUIRE(context.errors.len() > 0);
}

TEST_CASE("Preprocessor::next unterminated untaken #if is error") {
    SETUP("#ifdef x");

    REQUIRE(EAT_NEXT().type == Result::ErrorInvalidInput);

    REQUIRE(context.errors.len() > 0);
}

TEST_CASE("Preprocessor::next unterminated taken #if is error") {
    SETUP("#ifndef x");

    REQUIRE(EAT_NEXT().type == Result::ErrorInvalidInput);

    REQUIRE(context.errors.len() > 0);
}

TEST_CASE("Preprocessor::next #if isn't terminated by # newline endif") {
    SETUP("#ifndef x\n#\nendif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "endif");

    REQUIRE(EAT_NEXT().type == Result::Done);

    REQUIRE(context.errors.len() > 0);
}

TEST_CASE("Preprocessor::next continue over #error and record error") {
    SETUP("#error\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "abc");
    REQUIRE(context.errors.len() > 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next continue over #random and record error") {
    SETUP("#ooooo\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "abc");
    REQUIRE(context.errors.len() > 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next #random inside #if false is ignored") {
    SETUP("#ifdef x\n#ooooo\n#endif\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "abc");
    REQUIRE(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next #random inside #if true is error and continue") {
    SETUP("#ifndef x\n#ooooo\n#endif\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "abc");
    REQUIRE(context.errors.len() > 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next #if undefined is false") {
    SETUP("#if x\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");
    REQUIRE(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next #if defined 0 is false") {
    SETUP("#define x 0\n#if x\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");
    REQUIRE(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next #define no value") {
    SETUP("#define abc\nabc");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next #define one value") {
    SETUP("#define abc def\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "def");
    REQUIRE(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}
