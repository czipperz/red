#include "test_base.hpp"

#include <cz/defer.hpp>
#include "context.hpp"
#include "file_contents.hpp"
#include "load.hpp"
#include "parse.hpp"

using namespace red;
using namespace red::parse;

static void setup(Context* context, Parser* parser, cz::Str contents) {
    context->init();

    parser->init();
    include_file_reserve(&context->files, &parser->preprocessor);
    File_Contents file_contents;
    file_contents.load_str(contents, context->files.file_array_buffer_array.allocator());
    Hashed_Str file_path = Hashed_Str::from_str("*test_file*");
    force_include_file(&context->files, &parser->preprocessor, file_path, file_contents);
}

#define SETUP(CONTENTS)                 \
    Context context = {};               \
    Parser parser = {};                 \
    setup(&context, &parser, CONTENTS); \
                                        \
    CZ_DEFER({                          \
        parser.drop();                  \
        context.destroy();              \
    })

TEST_CASE("parse_declaration type but no identifier") {
    SETUP("int;");

    REQUIRE(parse_declaration(&context, &parser).is_ok());
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 0);
}

TEST_CASE("parse_declaration type with identifier") {
    SETUP("int abc;");

    REQUIRE(parse_declaration(&context, &parser).is_ok());
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
    CHECK(abc->o_value == nullptr);
}

TEST_CASE("parse_declaration two variables same type") {
    SETUP("int abc, def;");

    REQUIRE(parse_declaration(&context, &parser).is_ok());
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 2);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
    CHECK(abc->o_value == nullptr);

    Declaration* def = parser.declaration_stack[0].get_hash("def");
    REQUIRE(def);
    CHECK(def->type.get_type() == parser.type_int);
    CHECK_FALSE(def->type.is_const());
    CHECK_FALSE(def->type.is_volatile());
    CHECK(def->o_value == nullptr);
}

TEST_CASE("parse_declaration const applies to both variables") {
    SETUP("int const abc, def;");

    REQUIRE(parse_declaration(&context, &parser).is_ok());
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 2);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
    CHECK(abc->o_value == nullptr);

    Declaration* def = parser.declaration_stack[0].get_hash("def");
    REQUIRE(def);
    CHECK(def->type.get_type() == parser.type_int);
    CHECK(def->type.is_const());
    CHECK_FALSE(def->type.is_volatile());
    CHECK(def->o_value == nullptr);
}

TEST_CASE("parse_declaration second variable is pointer") {
    SETUP("int abc, *def;");

    REQUIRE(parse_declaration(&context, &parser).is_ok());
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 2);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
    CHECK(abc->o_value == nullptr);

    Declaration* def = parser.declaration_stack[0].get_hash("def");
    REQUIRE(def);
    CHECK_FALSE(def->type.is_const());
    CHECK_FALSE(def->type.is_volatile());
    CHECK(def->o_value == nullptr);
    Type* def_t = def->type.get_type();
    REQUIRE(def_t);
    REQUIRE(def_t->tag == Type::Pointer);
    Type_Pointer* def_type = (Type_Pointer*)def_t;
    CHECK(def_type->inner.get_type() == parser.type_int);
    CHECK_FALSE(def_type->inner.is_const());
    CHECK_FALSE(def_type->inner.is_volatile());
}

TEST_CASE("parse_declaration const cannot be used after an identifier") {
    SETUP("int abc, const *def;");

    REQUIRE(parse_declaration(&context, &parser).is_err());
    REQUIRE(parser.declaration_stack.len() == 1);
    CHECK(parser.declaration_stack[0].count == 1);

    Declaration* abc = parser.declaration_stack[0].get_hash("abc");
    REQUIRE(abc);
    CHECK(abc->type.get_type() == parser.type_int);
    CHECK_FALSE(abc->type.is_const());
    CHECK_FALSE(abc->type.is_volatile());
    CHECK(abc->o_value == nullptr);
}

TEST_CASE("parse_expression defined variable") {
    SETUP("int abc; abc;");

    REQUIRE(parse_declaration(&context, &parser).is_ok());

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).is_ok());
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Variable);
    Expression_Variable* e = (Expression_Variable*)expression;
    CHECK(e->variable.str == "abc");
}

TEST_CASE("parse_expression undefined variable") {
    SETUP("abc;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).is_err());
}

TEST_CASE("parse_expression integer") {
    SETUP("123;");

    Expression* expression;
    REQUIRE(parse_expression(&context, &parser, &expression).is_ok());
    REQUIRE(expression);
    REQUIRE(expression->tag == Expression::Integer);
    Expression_Integer* e = (Expression_Integer*)expression;
    CHECK(e->value == 123);
}
