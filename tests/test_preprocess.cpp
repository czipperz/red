#include "test_base.hpp"

#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include "context.hpp"
#include "definition.hpp"
#include "file_contents.hpp"
#include "hashed_str.hpp"
#include "lex.hpp"
#include "load.hpp"
#include "preprocess.hpp"
#include "token.hpp"

using namespace red;
using red::pre::Preprocessor;

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

#define EAT_NEXT() cpp::next_token(&context, &preprocessor, &lexer, &token)

TEST_CASE("cpp::next_token empty file") {
    SETUP("");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token ignores empty #pragma") {
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

TEST_CASE("cpp::next_token define is skipped with value") {
    SETUP("#define x a\n");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token define is skipped no value") {
    SETUP("#define x\na");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token ifdef without definition is skipped") {
    SETUP("#ifdef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token ifdef without definition empty body") {
    SETUP("#ifdef x\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token ifndef without definition is preserved") {
    SETUP("#ifndef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token ifndef without definition empty body") {
    SETUP("#ifndef x\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token ifdef with definition is preserved") {
    SETUP("#define x\n#ifdef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token ifndef with definition is skipped") {
    SETUP("#define x\n#ifndef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #undef then #ifndef") {
    SETUP("#define x\n#undef x\n#ifndef x\nabc\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "abc");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #undef then #ifdef") {
    SETUP("#define x\n#undef x\n#ifdef x\nabc\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE(
    "cpp::next_token in not taken #if, newline between # and endif shouldn't recognize it as "
    "#endif") {
    SETUP("#ifdef x\n#\nendif\n#endif\n");

    REQUIRE(EAT_NEXT().type == Result::Done);

    CHECK(context.errors.len() == 0);
}

TEST_CASE(
    "cpp::next_token in taken #if, newline between # and endif shouldn't recognize it as "
    "#endif") {
    SETUP("#ifndef x\n#\nendif\n#endif\n");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "endif");

    REQUIRE(EAT_NEXT().type == Result::Done);

    CHECK(context.errors.len() == 0);
}

TEST_CASE("cpp::next_token random #endif is error") {
    SETUP("#endif");

    REQUIRE(EAT_NEXT().type == Result::ErrorInvalidInput);

    REQUIRE(context.errors.len() > 0);
}

TEST_CASE("cpp::next_token unterminated untaken #if is error") {
    SETUP("#ifdef x");

    REQUIRE(EAT_NEXT().type == Result::ErrorInvalidInput);

    REQUIRE(context.errors.len() > 0);
}

TEST_CASE("cpp::next_token unterminated taken #if is error") {
    SETUP("#ifndef x");

    REQUIRE(EAT_NEXT().type == Result::ErrorInvalidInput);

    REQUIRE(context.errors.len() > 0);
}

TEST_CASE("cpp::next_token #if isn't terminated by # newline endif") {
    SETUP("#ifndef x\n#\nendif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "endif");

    REQUIRE(EAT_NEXT().type == Result::Done);

    REQUIRE(context.errors.len() > 0);
}

TEST_CASE("cpp::next_token continue over #error and record error") {
    SETUP("#error\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "abc");
    REQUIRE(context.errors.len() > 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token continue over #random and record error") {
    SETUP("#ooooo\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "abc");
    REQUIRE(context.errors.len() > 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #random inside #if false is ignored") {
    SETUP("#ifdef x\n#ooooo\n#endif\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "abc");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #random inside #if true is error and continue") {
    SETUP("#ifndef x\n#ooooo\n#endif\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "abc");
    REQUIRE(context.errors.len() > 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if x undefined is false") {
    SETUP("#if x\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if x defined 0 is false") {
    SETUP("#define x 0\n#if x\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if x defined 1 is true") {
    SETUP("#define x 1\n#if x\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if defined(x) defined 0 is true") {
    SETUP("#define x 0\n#if defined(x)\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if defined(x) undefined is false") {
    SETUP("#if defined(x)\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if !defined(x) undefined is true (not operator works)") {
    SETUP("#if !defined(x)\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if function macro using ##") {
    SETUP("#define f(x) a##x\n#if f(x)\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if 1 + 1 > 1 || 1 is true") {
    SETUP("#if 1 + 1 > 1 || 1\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if 1 + 1 > 1 is true") {
    SETUP("#if 1 + 1 > 1\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if 1 + 1 > 2 is false") {
    SETUP("#if 1 + 1 > 2\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token order of operations #if 0 - 1 - 2 == -3 is true") {
    SETUP("#if 0 - 1 - 2 == -3\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if parenthesis and order of operations") {
    SETUP("#if 0 - (1 - 2) == 1\na\n#else\nb\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if inside #if parsed correctly") {
    SETUP("#if 1\n#if 1\na\n#else\nb\n#endif\n#else\nc\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if ternary operator") {
    SETUP("#if 1 ? 0 : 1\nx\n#else\ny\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "y");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if ternary operator precedence") {
    SETUP("#if 1 - 0 ? 2 : 1\nx\n#else\ny\n#endif");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "x");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #if taken #elif ignored") {
    SETUP("#if 1\na\n#elif 2\nb\n#else\nc\n#endif\n");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
    CHECK(context.errors.len() == 0);
}

TEST_CASE("cpp::next_token #elif taken") {
    SETUP("#if 0\na\n#elif 2\nb\n#else\nc\n#endif\n");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
    CHECK(context.errors.len() == 0);
}

TEST_CASE("cpp::next_token #elif not taken") {
    SETUP("#if 0\na\n#elif 0\nb\n#else\nc\n#endif\n");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "c");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
    CHECK(context.errors.len() == 0);
}

TEST_CASE("cpp::next_token #endif trailing comment") {
    SETUP("#if 1\n#if 1\nx\n#endif //\n#endif\ny\n");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "x");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "y");

    REQUIRE(EAT_NEXT().type == Result::Done);
    CHECK(context.errors.len() == 0);
}

TEST_CASE("cpp::next_token #define no value") {
    SETUP("#define abc\nabc");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define one value") {
    SETUP("#define abc def\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "def");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define value with parenthesis") {
    SETUP("#define abc (def)\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::OpenParen);
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "def");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::CloseParen);
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define function macro no parameters isn't replaced when not invoked") {
    SETUP("#define abc() def\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "abc");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define function macro no parameters is replaced when invoked") {
    SETUP("#define abc() def\nabc()");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "def");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define function macro one parameter basic case") {
    SETUP("#define abc(def) def\nabc(a) b");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define function macro one parameter no arguments is ok") {
    SETUP("#define abc(def) def\nabc() b");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");
    CHECK(context.errors.len() == 0);
}

TEST_CASE("cpp::next_token #define function higher order") {
    SETUP("#define abc(def) def(1)\n#define mac(x) 2\nabc(mac) b");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Integer);
    CHECK(token.v.integer.value == 2);
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define varargs only param and no args") {
    SETUP("#define abc(...) 2 __VAR_ARGS__ 3\n1 abc() 4");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Integer);
    CHECK(token.v.integer.value == 1);
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Integer);
    CHECK(token.v.integer.value == 2);
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Integer);
    CHECK(token.v.integer.value == 3);
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Integer);
    CHECK(token.v.integer.value == 4);
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define # make undefined identifier string") {
    SETUP("#define abc #def\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::String);
    CHECK(token.v.string == "def");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define # parameter") {
    SETUP("#define abc(def) #def\nabc(x)");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::String);
    CHECK(token.v.string == "x");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define # argument isn't expanded") {
    SETUP("#define abc(def) #def\n#define x y\nabc(x)");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::String);
    CHECK(token.v.string == "x");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define # after function call argument is expanded") {
    SETUP("#define abc2(def) #def\n#define abc(def) abc2(def)\n#define x y\nabc(x)");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::String);
    CHECK(token.v.string == "y");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define # parameter string double escapes it") {
    SETUP("#define abc(def) #def\nabc(\"\\\"\\\\abc\\\"\")");
    // string syntax = "\"\\abc\""
    // string value = "\abc"

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::String);
    CHECK(token.v.string == "\"\\\"\\\\abc\\\"\"");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define # inside argument") {
    SETUP("#define abc(def) def\nabc(#x)");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Hash);

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "x");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define ## two non parameters") {
    SETUP("#define abc a##b\nabc");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.string == "ab");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define ## two parameters") {
    SETUP("#define abc(a, b) a##b\nabc(x, y)");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.string == "xy");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define ## argument is empty deletes that side right") {
    SETUP("#define abc(b) a##b\nabc()");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(token.type == Token::Identifier);
    CHECK(token.v.string == "a");
    CHECK(context.errors.len() == 0);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define ## argument is empty deletes that side left") {
    SETUP("#define abc(a) a##b\nabc()");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(token.type == Token::Identifier);
    CHECK(token.v.string == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define ## chain") {
    SETUP("#define abc(a, b) x##a##y##b##z\nabc(h, j)");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(token.type == Token::Identifier);
    CHECK(token.v.string == "xhyjz");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define ## combine to make keyword") {
    SETUP("#define abc(a, b) a ## b\nabc(str, uct)");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
    CHECK(token.type == Token::Struct);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("cpp::next_token #define function no body") {
    SETUP("#define abc()");

    REQUIRE(EAT_NEXT().type == Result::Done);
    CHECK(context.errors.len() == 0);
}

TEST_CASE("cpp::next_token #define no body") {
    SETUP("#define abc\na");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");

    pre::Definition* definition = preprocessor.definitions.get_hash("abc");
    REQUIRE(definition);
    CHECK(definition->tokens.len() == 0);
}

TEST_CASE("cpp::next_token #define usage with comma inside parenthesis is still one argument") {
    SETUP("#define abc(x) x\nabc((a, b))");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
    CHECK(token.type == Token::OpenParen);

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "a");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
    CHECK(token.type == Token::Comma);

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
    REQUIRE(token.type == Token::Identifier);
    CHECK(token.v.identifier.str == "b");

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
    CHECK(token.type == Token::CloseParen);

    REQUIRE(EAT_NEXT().type == Result::Done);
    CHECK(context.errors.len() == 0);
}

static void check_keyword(const char* str, red::Token::Type type_expected) {
    SETUP(str);

    REQUIRE(EAT_NEXT().type == Result::Success);
    CHECK(context.errors.len() == 0);
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
